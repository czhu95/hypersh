#include "callbacks.h"

#include <iostream>
#include <syscall.h>
#include "threads.h"

#define VERBOSE 0

// static const char * SiftModeStr[] = {
//     "ModeUnknown", "ModeIcount", "ModeMemory", "ModeDetailed", "ModeStop" };

struct tb_info
{

};

struct insn_info
{

};

static void tb_exec_count_insns(unsigned int threadid, void *userdata)
{
    auto count = (uint64_t)userdata;
    thread_data[threadid].icount += count;

    thread_data[threadid].icount_reported += count;
    if (thread_data[threadid].icount_reported > FlowControlFF) {
        control_mtx.lock_shared();
        thread_data[threadid].output->InstructionCount(
                thread_data[threadid].icount_reported);
        thread_data[threadid].icount_reported = 0;
        control_mtx.unlock_shared();
    }
}

static void insn_exec_count_insn(unsigned int threadid, void *userdata)
{
    thread_data[threadid].icount_cacheonly_pending ++;
}

static void insn_exec_branch(unsigned int threadid, void *userdata)
{
    thread_data[threadid].br_addr = (uint64_t)userdata;
}

static void insn_exec_branch_ram_addr(unsigned int threadid, void *userdata)
{
    thread_data[threadid].output->Translate(thread_data[threadid].br_addr,
                                            (uint64_t)userdata);
}

static void mem_update_pc(unsigned int threadid,
                          qemu_plugin_meminfo_t meminfo, uint64_t vaddr,
                          void *userdata)
{
    thread_data[threadid].pc = (uint64_t)userdata;
}

static void mem_send_cacheonly(unsigned int threadid,
                               qemu_plugin_meminfo_t meminfo, uint64_t vaddr,
                               void *userdata)
{
    uint64_t pc_ram_addr = (uint64_t)userdata;
    auto type = qemu_plugin_mem_is_store(meminfo) ?
                Sift::CacheOnlyMemWrite : Sift::CacheOnlyMemRead;

    uint64_t icount_pending = thread_data[threadid].icount_cacheonly_pending;

    auto hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);
    uint64_t paddr = qemu_plugin_hwaddr_device_offset(hwaddr);
    thread_data[threadid].output->Translate(vaddr, paddr);

    uint64_t pc = thread_data[threadid].pc;

    // PLUGIN_PRINT_INFO("pc: %lx, icount_pending: %lx, vaddr: %lx, paddr: %lx, pc_paddr: %lx",
    //                   pc, icount_pending, vaddr, paddr, pc_ram_addr);

    if (icount_pending) {
        thread_data[threadid].output->Translate(pc, pc_ram_addr);
        thread_data[threadid].icount_cacheonly += icount_pending;
        thread_data[threadid].icount_reported += icount_pending;
        thread_data[threadid].icount_cacheonly_pending = 0;
    }

    thread_data[threadid].output->CacheOnly(icount_pending, type, pc, vaddr);

    if (thread_data[threadid].icount_reported > FlowControlFF) {
        thread_data[threadid].output->Sync();
        thread_data[threadid].icount_reported = 0;
    }
}

static void tb_exec_update_target(unsigned int threadid, void *userdata)
{
    thread_data[threadid].br_target = (uint64_t)userdata;
    thread_data[threadid].tb_vaddr1 = (uint64_t)userdata;
}

static void tb_exec_update_fallthrough(unsigned int threadid, void *userdata)
{
    thread_data[threadid].br_fallthrough = (uint64_t)userdata;
}

static void tb_exec_branch_cacheonly(unsigned int threadid, void *userdata)
{
    if (thread_data[threadid].br_addr == 0)
        return;

    uint64_t pc_ram_addr = (uint64_t)userdata;
    auto icount_pending = thread_data[threadid].icount_cacheonly_pending;

    assert(thread_data[threadid].br_fallthrough != 0);
    auto type = thread_data[threadid].br_target ==
                thread_data[threadid].br_fallthrough ?
                Sift::CacheOnlyBranchNotTaken : Sift::CacheOnlyBranchTaken;

    thread_data[threadid].output->Translate(thread_data[threadid].br_target,
                                            pc_ram_addr);
    /* br_addr: pc of branch instruction
     * br_target: pc of branch target. */
    thread_data[threadid].output->CacheOnly(icount_pending, type,
                                            thread_data[threadid].br_addr,
                                            thread_data[threadid].br_target);

    thread_data[threadid].icount_cacheonly += icount_pending;
    thread_data[threadid].icount_reported += icount_pending;
    thread_data[threadid].icount_cacheonly_pending = 0;

    thread_data[threadid].br_addr = 0;
    thread_data[threadid].br_fallthrough = 0;

    if (thread_data[threadid].icount_reported > FlowControlFF) {
        thread_data[threadid].output->Sync();
        thread_data[threadid].icount_reported = 0;
    }
}

static void insn_exec_update_pc(unsigned int threadid, void *userdata)
{
#if VERBOSE > 1
    PLUGIN_PRINT_INFO("InstructionEnd.");
#endif
    thread_data[threadid].output->InstructionEnd();

    thread_data[threadid].pc = (uint64_t)userdata;
    thread_data[threadid].icount_detailed ++;

    if (thread_data[threadid].icount_detailed >
        thread_data[threadid].flowcontrol_target)
    {
        thread_data[threadid].output->Sync();
        thread_data[threadid].flowcontrol_target += FlowControl;
    }
}

static void insn_exec_pc_next(unsigned int threadid, void *userdata)
{
    uint64_t pc_next = (uint64_t)userdata;
    uint64_t pc = thread_data[threadid].pc;

#if VERBOSE > 1
    PLUGIN_PRINT_INFO("InstructionBegin:  %lx, %lx", pc, pc_next - pc);
#endif
    thread_data[threadid].output->InstructionBegin(pc, pc_next - pc, false,
                                                   true);
}

static void mem_send_detailed(unsigned int threadid,
                              qemu_plugin_meminfo_t meminfo, uint64_t vaddr,
                              void *userdata)
{
    // bool is_store = qemu_plugin_mem_is_store(meminfo);

    // uint64_t wr_vaddr = thread_data[threadid].wr_vaddr;

    // if (is_store) {
    //     if (thread_data[threadid].wr_vaddr != 0)
    //         return;
    // } else {
    //     uint64_t rd_vaddr1 = thread_data[threadid].rd_vaddr1;
    //     uint64_t rd_vaddr2 = thread_data[threadid].rd_vaddr2;
    //     if (rd_vaddr1 != 0) {
    //         if (rd_vaddr2 != 0) {
    //             if (rd_vaddr2 - rd_vaddr1 == 8)
    //                 thread_data[threadid].rd_vaddr2 = vaddr;
    //             else if (vaddr - rd_vaddr2 == 8)
    //                 return;
    //             else
    //                 assert(false);
    //         } else {
    //             thread_data[threadid].rd_vaddr2 = vaddr;
    //         }
    //     } else {

    //     }
    // }
    auto hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);
    uint64_t paddr = qemu_plugin_hwaddr_device_offset(hwaddr);
    thread_data[threadid].output->Translate(vaddr, paddr);

#if VERBOSE > 1
    PLUGIN_PRINT_INFO("InstructionMem:    %lx -> %lx, %c", vaddr, paddr,
                      qemu_plugin_mem_is_store(meminfo) ? 'w' : 'r');
#endif

    thread_data[threadid].output->InstructionMem(vaddr);
}

static void tb_exec_branch_detailed(unsigned int threadid, void *userdata)
{
    if (thread_data[threadid].br_addr == 0)
        return;

    assert(thread_data[threadid].br_fallthrough != 0);
    bool taken = thread_data[threadid].br_target !=
                 thread_data[threadid].br_fallthrough;

#if VERBOSE > 1
    PLUGIN_PRINT_INFO("InstructionBranch: -> %lx (%s)",
                      thread_data[threadid].br_target,
                      taken ? "taken" : "not taken");
#endif

    /* br_addr: pc of branch instruction
     * br_target: pc of branch target. */
    thread_data[threadid].output->InstructionBranch(taken);

    thread_data[threadid].br_addr = 0;
    thread_data[threadid].br_fallthrough = 0;
}

static void tb_exec_send_icache(unsigned int threadid, void *userdata)
{
    uint64_t pgd = qemu_plugin_in_kernel() ? 0 : qemu_plugin_page_directory();
    thread_data[threadid].output->SendICache(thread_data[threadid].tb_vaddr1,
                                             (uint8_t *)userdata, pgd);
}

static void tb_exec_vaddr2(unsigned int threadid, void *userdata)
{
    thread_data[threadid].tb_vaddr2 = (uint64_t)userdata;
}

static void tb_exec_send_icache2(unsigned int threadid, void *userdata)
{
    uint64_t pgd = qemu_plugin_in_kernel() ? 0 : qemu_plugin_page_directory();
    thread_data[threadid].output->SendICache(thread_data[threadid].tb_vaddr2,
                                             (uint8_t *)userdata, pgd);
}

// static void tb_exec_flush_icache(unsigned int threadid, void *userdata)
// {
//     if (qemu_plugin_in_kernel())
//         return;
// 
//     uint64_t pgd = qemu_plugin_page_directory();
//     if (pgd != thread_data[threadid].pgd) {
//         thread_data[threadid].pgd = pgd;
//         thread_data[threadid].output->FlushICache();
//     }
// }

void vcpu_interrupt_cb(qemu_plugin_id_t id, unsigned int threadid)
{
    if (current_mode == Sift::ModeDetailed) {
#if VERBOSE > 1
        PLUGIN_PRINT_INFO("Int @icount = %lu", thread_data[threadid].icount_detailed);
        PLUGIN_PRINT_INFO("InstructionEnd.");
#endif
        thread_data[threadid].output->InstructionEnd();
    }
}

void vcpu_interrupt_ret_cb(qemu_plugin_id_t id, unsigned int threadid)
{
    if (current_mode == Sift::ModeDetailed) {
#if VERBOSE > 1
        PLUGIN_PRINT_INFO("InstructionEnd.");
#endif
        thread_data[threadid].output->InstructionEnd();
    }
}

static inline bool syscall_is_exec(int64_t num)
{
    return num == SYS_execve || num == SYS_execveat;
}

void vcpu_syscall_cb(qemu_plugin_id_t id, unsigned int vcpu_index,
                     int64_t num, uint64_t a1, uint64_t a2,
                     uint64_t a3, uint64_t a4, uint64_t a5,
                     uint64_t a6, uint64_t a7, uint64_t a8)
{
    if (syscall_is_exec(num) && current_mode == Sift::ModeDetailed) {
        auto pgd = qemu_plugin_page_directory();
        /* We should be safe invalidating icache of other vcpus, as current
         * user process is making a execve syscall so is not using multiple
         * vcpus. */
        for (unsigned int threadid = 0; threadid < smp_vcpus; threadid ++)
            thread_data[threadid].output->FlushICache(pgd);
    }

}

void vcpu_tb_trans_cb(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    auto n_insns = qemu_plugin_tb_n_insns(tb);
    qemu_plugin_insn *insn = NULL;
    uint64_t vaddr2;
#if VERBOSE > 0
    uint8_t buf[64];
#endif

    switch (current_mode) {
        case Sift::ModeIcount:
            qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_count_insns,
                                                 QEMU_PLUGIN_CB_NO_REGS,
                                                 (void *)n_insns);
            break;
        case Sift::ModeMemory:
            for (size_t i = 0; i < n_insns; i ++) {
                insn = qemu_plugin_tb_get_insn(tb, i);
                /* count instructions. */
                qemu_plugin_register_vcpu_insn_exec_cb(insn,
                                                       insn_exec_count_insn,
                                                       QEMU_PLUGIN_CB_NO_REGS,
                                                       NULL);

                /* update pc for mem instructions. */
                /* TODO: mem_update_pc is currently executed once per memop,
                 * instead it should be enough to execute once per mem inst. */
                qemu_plugin_register_vcpu_mem_cb(
                        insn, mem_update_pc, QEMU_PLUGIN_CB_NO_REGS,
                        QEMU_PLUGIN_MEM_RW,
                        (void *)qemu_plugin_insn_vaddr(insn));

                /* send mem ops. */
                qemu_plugin_register_vcpu_mem_cb(
                        insn, mem_send_cacheonly, QEMU_PLUGIN_CB_NO_REGS,
                        QEMU_PLUGIN_MEM_RW,
                        (void *)qemu_plugin_insn_ram_addr(insn));
            }

            /* update branch target pc. */
            qemu_plugin_register_vcpu_tb_exec_cb(
                    tb, tb_exec_update_target, QEMU_PLUGIN_CB_NO_REGS,
                    (void *)qemu_plugin_tb_vaddr(tb));

            /* send branch ops. */
            qemu_plugin_register_vcpu_tb_exec_cb(
                    tb, tb_exec_branch_cacheonly, QEMU_PLUGIN_CB_NO_REGS,
                    (void *)qemu_plugin_insn_ram_addr(insn));

            if (qemu_plugin_tb_is_branch(tb)) {
                assert(insn);
                /* record pc of branch instruction. */
                qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, insn_exec_branch, QEMU_PLUGIN_CB_NO_REGS,
                        (void *)qemu_plugin_insn_vaddr(insn));

                /* translate branch inst vaddr to paddr. */
                qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, insn_exec_branch_ram_addr,
                        QEMU_PLUGIN_CB_NO_REGS,
                        (void *)qemu_plugin_insn_ram_addr(insn));

                /* record fall through address. */
                qemu_plugin_register_vcpu_tb_exec_cb(
                        tb, tb_exec_update_fallthrough,
                        QEMU_PLUGIN_CB_NO_REGS,
                        (void *)qemu_plugin_tb_next_pc(tb));
            }
            break;
        case Sift::ModeDetailed:
            for (size_t i = 0; i < n_insns; i ++) {
                insn = qemu_plugin_tb_get_insn(tb, i);
                /* update pc */
                qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, insn_exec_update_pc, QEMU_PLUGIN_CB_NO_REGS,
                        (void *)qemu_plugin_insn_vaddr(insn));

                /* next instruction pc */
                qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, insn_exec_pc_next, QEMU_PLUGIN_CB_NO_REGS,
                        (void *)qemu_plugin_insn_next(insn));

                /* send mem ops. */
                qemu_plugin_register_vcpu_mem_cb(
                        insn, mem_send_detailed, QEMU_PLUGIN_CB_NO_REGS,
                        QEMU_PLUGIN_MEM_RW,
                        (void *)qemu_plugin_insn_ram_addr(insn));

#if VERBOSE > 0
                fprintf(stdout, "%lx: ", qemu_plugin_insn_vaddr(insn));
                qemu_plugin_insn_bytes(insn, buf);
                for (uint8_t *b = buf;
                     b < buf + (qemu_plugin_insn_next(insn) -
                                qemu_plugin_insn_vaddr(insn));
                     b ++)
                    fprintf(stdout, "%x ", (unsigned int)*b);
                fprintf(stdout, "\n");
#endif
            }

            /* update branch target pc. */
            qemu_plugin_register_vcpu_tb_exec_cb(
                    tb, tb_exec_update_target, QEMU_PLUGIN_CB_NO_REGS,
                    (void *)qemu_plugin_tb_vaddr(tb));

            /* send branch ops. */
            qemu_plugin_register_vcpu_tb_exec_cb(
                    tb, tb_exec_branch_detailed, QEMU_PLUGIN_CB_NO_REGS, NULL);

            /* flush icache if necessary. */
            // qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_flush_icache,
            //                                      QEMU_PLUGIN_CB_NO_REGS,
            //                                      NULL);

            qemu_plugin_register_vcpu_tb_exec_cb(
                    tb, tb_exec_send_icache, QEMU_PLUGIN_CB_NO_REGS,
                    (void *)qemu_plugin_tb_haddr(tb));

            vaddr2 = qemu_plugin_tb_vaddr2(tb);
            if (vaddr2 != (uint64_t)-1) {
                assert(qemu_plugin_tb_haddr2(tb) != NULL);
                qemu_plugin_register_vcpu_tb_exec_cb(
                        tb, tb_exec_vaddr2, QEMU_PLUGIN_CB_NO_REGS,
                        (void *)vaddr2);

                qemu_plugin_register_vcpu_tb_exec_cb(
                        tb, tb_exec_send_icache2, QEMU_PLUGIN_CB_NO_REGS,
                        (void *)qemu_plugin_tb_haddr2(tb));
            }

            if (qemu_plugin_tb_is_branch(tb)) {
                assert(insn);
                /* record pc of branch instruction. */
                qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, insn_exec_branch, QEMU_PLUGIN_CB_NO_REGS,
                        (void *)qemu_plugin_insn_vaddr(insn));

                /* record fall through address. */
                qemu_plugin_register_vcpu_tb_exec_cb(
                        tb, tb_exec_update_fallthrough,
                        QEMU_PLUGIN_CB_NO_REGS,
                        (void *)qemu_plugin_tb_next_pc(tb));
            }
            break;

        default:
            break;
    }
}

void vcpu_idle_cb(qemu_plugin_id_t id, unsigned int threadid)
{
    control_mtx.lock_shared();
    thread_data[threadid].output->VCPUIdle();
    control_mtx.unlock_shared();
}

void vcpu_resume_cb(qemu_plugin_id_t id, unsigned int threadid)
{
    control_mtx.lock_shared();
    thread_data[threadid].output->VCPUResume();
    control_mtx.unlock_shared();
}
