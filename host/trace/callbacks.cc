#include "callbacks.h"

#include "threads.h"

// static const char * SiftModeStr[] = {
//     "ModeUnknown", "ModeIcount", "ModeMemory", "ModeDetailed", "ModeStop" };

void setInstrumentationMode(Sift::Mode mode)
{
    // fprintf(stderr, "Setting instrumentation mode to %s.\n",
    //         SiftModeStr[mode]);
    if (false && current_mode != mode && mode != Sift::ModeUnknown) {
        current_mode = mode;
        switch (mode) {
            case Sift::ModeIcount:
            case Sift::ModeMemory:
            case Sift::ModeDetailed:
                any_thread_in_detail = false;
                break;
//             case Sift::ModeMemory:
//             case Sift::ModeDetailed:
//                 any_thread_in_detail = true;
//                 break;
            case Sift::ModeStop:
            case Sift::ModeUnknown:
            default:
                assert(false);
        }
        qemu_plugin_tb_flush();
    }
}

static void countInsns(unsigned int threadid, void *userdata)
{
    auto count = (uint64_t)userdata;
    thread_data[threadid].icount += count;
    // fprintf(stderr, "Counting instructions: +%lu.\n", count);

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

void vcpu_tb_trans_cb(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    if (!any_thread_in_detail) {
        unsigned long n_insns = qemu_plugin_tb_n_insns(tb);
        qemu_plugin_register_vcpu_tb_exec_cb(tb, countInsns,
                                             QEMU_PLUGIN_CB_NO_REGS,
                                             (void *)n_insns);
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
