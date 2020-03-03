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
        Sift::Mode mode = thread_data[threadid].output->InstructionCount(
                thread_data[threadid].icount_reported);
        thread_data[threadid].icount_reported = 0;
        setInstrumentationMode(mode);
        control_mtx.unlock_shared();
    }
}

static void insn_exec_count_insn(unsigned int threadid, void *userdata)
{
    thread_data[threadid].icount_cacheonly_pending ++;
}

static void mem_set_pc(unsigned int threadid,
                       qemu_plugin_meminfo_t meminfo, uint64_t vaddr,
                       void *userdata)
{
    thread_data[threadid].pc_cacheonly = (uint64_t)userdata;
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

    uint64_t pc = thread_data[threadid].pc_cacheonly;

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

void vcpu_tb_trans_cb(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    auto n_insns = qemu_plugin_tb_n_insns(tb);
    if (!any_thread_in_detail) {
        qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_count_insns,
                                             QEMU_PLUGIN_CB_NO_REGS,
                                             (void *)n_insns);
    } else if (current_mode == Sift::ModeMemory) {
        for (size_t i = 0; i < n_insns; i ++) {
            auto insn = qemu_plugin_tb_get_insn(tb, i);
            qemu_plugin_register_vcpu_insn_exec_cb(insn, insn_exec_count_insn,
                                                   QEMU_PLUGIN_CB_NO_REGS,
                                                   NULL);

            qemu_plugin_register_vcpu_mem_cb(
                    insn, mem_set_pc, QEMU_PLUGIN_CB_NO_REGS,
                    QEMU_PLUGIN_MEM_RW,
                    (void *)qemu_plugin_insn_vaddr(insn));

            qemu_plugin_register_vcpu_mem_cb(
                    insn, mem_send_cacheonly, QEMU_PLUGIN_CB_NO_REGS,
                    QEMU_PLUGIN_MEM_RW,
                    (void *)qemu_plugin_insn_ram_addr(insn));
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
