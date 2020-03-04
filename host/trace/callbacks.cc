#include "callbacks.h"

#include "threads.h"

// static const char * SiftModeStr[] = {
//     "ModeUnknown", "ModeIcount", "ModeMemory", "ModeDetailed", "ModeStop" };

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

static void tb_exec_update_pc(unsigned int threadid, void *userdata)
{
    thread_data[threadid].pc = (uint64_t)userdata;
}

static void tb_exec_update_fallthrough(unsigned int threadid, void *userdata)
{
    thread_data[threadid].br_fallthrough = (uint64_t)userdata;
}

static void tb_exec_branch(unsigned int threadid, void *userdata)
{
    if (thread_data[threadid].br_addr == 0)
        return;

    uint64_t pc_ram_addr = (uint64_t)userdata;
    auto icount_pending = thread_data[threadid].icount_cacheonly_pending;

    assert(thread_data[threadid].br_fallthrough != 0);
    auto type = thread_data[threadid].pc ==
                thread_data[threadid].br_fallthrough ?
                Sift::CacheOnlyBranchNotTaken : Sift::CacheOnlyBranchTaken;

    thread_data[threadid].output->Translate(thread_data[threadid].pc,
                                            pc_ram_addr);
    /* br_addr: pc of branch instruction
     * pc: pc of branch target. */
    thread_data[threadid].output->CacheOnly(icount_pending, type,
                                            thread_data[threadid].br_addr,
                                            thread_data[threadid].pc);

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

void vcpu_tb_trans_cb(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    auto n_insns = qemu_plugin_tb_n_insns(tb);
    if (current_mode == Sift::ModeIcount) {
        qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_count_insns,
                                             QEMU_PLUGIN_CB_NO_REGS,
                                             (void *)n_insns);
    } else if (current_mode == Sift::ModeMemory) {
        qemu_plugin_insn *insn = NULL;
        for (size_t i = 0; i < n_insns; i ++) {
            insn = qemu_plugin_tb_get_insn(tb, i);
            /* count instructions. */
            qemu_plugin_register_vcpu_insn_exec_cb(insn, insn_exec_count_insn,
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
                tb, tb_exec_update_pc, QEMU_PLUGIN_CB_NO_REGS,
                (void *)qemu_plugin_tb_next_pc(tb));


        /* send branch ops. */
        qemu_plugin_register_vcpu_tb_exec_cb(
                tb, tb_exec_branch, QEMU_PLUGIN_CB_NO_REGS,
                (void *)qemu_plugin_tb_next_pc(tb));

        if (qemu_plugin_tb_is_branch(tb)) {
            assert(insn);
            /* record pc of branch instruction. */
            qemu_plugin_register_vcpu_insn_exec_cb(
                    insn, insn_exec_branch, QEMU_PLUGIN_CB_NO_REGS,
                    (void *)qemu_plugin_insn_vaddr(insn));

            /* translate branch inst vaddr to paddr. */
            qemu_plugin_register_vcpu_insn_exec_cb(
                    insn, insn_exec_branch_ram_addr, QEMU_PLUGIN_CB_NO_REGS,
                    (void *)qemu_plugin_insn_ram_addr(insn));

            /* record fall through address. */
            qemu_plugin_register_vcpu_tb_exec_cb(
                    tb, tb_exec_update_fallthrough,
                    QEMU_PLUGIN_CB_NO_REGS,
                    (void *)qemu_plugin_tb_next_pc(tb));
        }
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
