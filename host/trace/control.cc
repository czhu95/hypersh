#define __STDC_FORMAT_MACROS
#include <mutex>
#include <string.h>
#include <vector>
#include <assert.h>
#include <map>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <atomic>
#include <iostream>
#include <iomanip>

#include "callbacks.h"
#include "threads.h"
#include "globals.h"
#include "common.h"
#include "sim_api.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define QEMU_VCPU_FOREACH(expr)                                        \
    qemu_plugin_vcpu_for_each(id,                                      \
            [](qemu_plugin_id_t id, unsigned int threadid) { expr })

using std::vector;
using std::string;

vector<thread_data_t> thread_data;
uint32_t smp_vcpus;

enum trace_mode
{
    TRACE_ON,
    TRACE_OFF
};

static vector<string> trace_files;
static char output_dir_template[] = "/tmp/tmpXXXXXX";
std::mutex access_memory_mtx;

static int openFile(threadid_t threadid, const char *dir);
static int closeFile(threadid_t threadid);

static bool recording = false;

static void thread_data_reset()
{
    for (threadid_t threadid = 0; threadid < smp_vcpus; threadid ++) {
        thread_data[threadid].thread_num = threadid;
        thread_data[threadid].output = NULL;
        thread_data[threadid].pc = 0UL;
        thread_data[threadid].br_addr = 0UL;
        thread_data[threadid].br_target = 0UL;
        thread_data[threadid].br_fallthrough = 0UL;
        thread_data[threadid].tb_vaddr1 = 0UL;
        thread_data[threadid].tb_vaddr2 = 0UL;
        thread_data[threadid].icount = 0UL;
        thread_data[threadid].icount_cacheonly = 0UL;
        thread_data[threadid].icount_cacheonly_pending = 0UL;
        thread_data[threadid].icount_detailed = 0UL;
        thread_data[threadid].icount_reported = 0UL;
        thread_data[threadid].flowcontrol_target = 0UL;
        thread_data[threadid].loads = 0UL;
        thread_data[threadid].stores = 0UL;
        thread_data[threadid].icount_user = 0UL;
        thread_data[threadid].icount_other = 0UL;
        thread_data[threadid].enabled = true;
        thread_data[threadid].idle = true;
        thread_data[threadid].capturing = false;
        thread_data[threadid].capture_next_addr1 = false;
        thread_data[threadid].capture_next_addr2 = false;
        thread_data[threadid].captured_addr1 = 0UL;
        thread_data[threadid].captured_addr2 = 0UL;

        block_cnt[threadid].clear();
    }
}

static void recorder_start(qemu_plugin_id_t id, char *dir)
{
    if (recording) {
        PLUGIN_PRINT_ERROR("already recording.");
        return;
    }

    if (dir == NULL) {
        dir = output_dir_template;
        strcpy(dir + strlen(dir) - 6, "XXXXXX");
        dir = mkdtemp(dir);
        assert(dir != NULL);
    }

    thread_data_reset();

    /* Opening a fifo file blocks on waiting for a reader.
     * We Create fifos for all vcpus before open them, so we can invoke
     * the simulator process at some time in between. */
    for (threadid_t i = 0; i < smp_vcpus; i ++) {
        auto filename = string(dir) + "/vcpu"
                        + std::to_string(i) + ".sift";
        auto response = string(dir) + "/response.vcpu"
                        + std::to_string(i) + ".sift";

        mkfifo(filename.c_str(), 0600);
        mkfifo(response.c_str(), 0600);

        trace_files.push_back(filename);
        trace_files.push_back(response);
    }

    PLUGIN_PRINT_INFO("recording to %s", dir);
    for (threadid_t i = 0; i < smp_vcpus; i ++)
        openFile(i, dir);

    recording = true;
    current_mode = Sift::ModeIcount;
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans_cb);
    qemu_plugin_register_vcpu_idle_cb(id, vcpu_idle_cb);
    qemu_plugin_register_vcpu_resume_cb(id, vcpu_resume_cb);
    qemu_plugin_register_vcpu_interrupt_cb(id, vcpu_interrupt_cb);
    qemu_plugin_register_vcpu_interrupt_ret_cb(id, vcpu_interrupt_ret_cb);
    qemu_plugin_register_vcpu_syscall_cb(id, vcpu_syscall_cb);
    qemu_plugin_register_vcpu_syscall_ret_cb(id, vcpu_syscall_ret_cb);
    qemu_plugin_tb_flush();
}

static void recorder_stop(qemu_plugin_id_t id, unsigned int threadid)
{
    if (!recording) {
        PLUGIN_PRINT_ERROR("not recording");
        return;
    }
    /* We should avoid current recorder/tracer thread blocking others during
     * qemu_plugin_reset, which has a critical section to synchronize.
     * We are safe to close our file first, so the remote tracing thread can
     * end and no other tracer threads will be blocked on us.
     * qemu_plugin_reset will then be executed on the current vcpu waiting
     * for other vcpus to finish its current block. */
    // for (threadid_t threadid = 0; threadid < smp_vcpus; threadid ++)
        thread_data[threadid].output->End();

    qemu_plugin_reset(id, [](qemu_plugin_id_t id) {
        /* We should be running exclusively in reset callback. */
        for (threadid_t threadid = 0; threadid < smp_vcpus; threadid ++) {
            closeFile(threadid);
            if (current_mode == Sift::ModeDetailed)
                PLUGIN_PRINT_VCPU_INFO(threadid, "user: %lu, other: %lu, loads: %lu, writes: %lu",
                                       thread_data[threadid].icount_user,
                                       thread_data[threadid].icount_other,
                                       thread_data[threadid].loads,
                                       thread_data[threadid].stores);
        }

        std::multimap<uint64_t, uint64_t, std::greater<uint64_t>> sort_map;
        for (threadid_t threadid = 0; threadid < 1; threadid ++) {
            sort_map.clear();
            std::transform(block_cnt[threadid].begin(), block_cnt[threadid].end(),
                           std::inserter(sort_map, sort_map.begin()),
                           [](const std::pair<uint64_t, uint64_t> &p) {
                               return std::pair<uint64_t, uint64_t>(p.second, p.first);
                           });

            PLUGIN_PRINT_VCPU_INFO(threadid, "Hot blocks:");
            int i = 0;
            for (const auto &it: sort_map) {
                std::cout << std::hex << it.second << ": "
                          << std::dec << it.first << ", ";
                if (++i == 10)
                    break;
            }
            std::cout << std::endl;
        }


        for (const auto &f : trace_files)
            unlink(f.c_str());

        trace_files.clear();
        recording = false;
        qemu_plugin_set_slomo_rate(1);
        PLUGIN_PRINT_INFO("Successfully reset trace.");
    });
}

static void recorder_gmm(qemu_plugin_id_t id, threadid_t threadid,
                         const char *cmd_str, uint64_t segment, uint64_t arg1)
{
    if (!strcmp(cmd_str, "create"))
        thread_data[threadid].output->GMMCommand(0, segment, arg1);
    else if (!strcmp(cmd_str, "assign"))
        thread_data[threadid].output->GMMCommand(1, segment, arg1);
    else if (!strcmp(cmd_str, "message")) {
        if (current_mode == Sift::ModeDetailed) {
            Sift::GMMUserMessage msg{10, segment, arg1};
            thread_data[threadid].output->SendGMMUserMessage(msg);
        }
    }
    else {
        PLUGIN_PRINT_ERROR("Unknown gmm command %s.", cmd_str);
        return;
    }

}

static void recorder_mode(qemu_plugin_id_t id, threadid_t threadid,
                          const char *mode_str)
{
    if (!recording) {
        PLUGIN_PRINT_ERROR("not recording");
        return;
    }

    Sift::Mode mode = Sift::ModeUnknown;
    if (!strcmp(mode_str, "fastforward"))
        mode = Sift::ModeIcount;
    else if (!strcmp(mode_str, "cacheonly"))
        mode = Sift::ModeMemory;
    else if (!strcmp(mode_str, "detailed"))
        mode = Sift::ModeDetailed;
    else {
        PLUGIN_PRINT_ERROR("Unknown mode %s.", mode_str);
        return;
    }

    if (current_mode == mode)
        return;

    qemu_plugin_vcpu_simple_cb_t do_mode_switch;

    switch (mode) {
        case Sift::ModeIcount:
            do_mode_switch = [](qemu_plugin_id_t id, unsigned int cpu_index) {
                thread_data[cpu_index].output->Magic(
                        SIM_CMD_INSTRUMENT_MODE,
                        SIM_OPT_INSTRUMENT_FASTFORWARD, 0);
                qemu_plugin_set_slomo_rate(1);
                current_mode = Sift::ModeIcount;
            };
            roi_cr3 = qemu_plugin_page_directory();
            break;
        case Sift::ModeMemory:
            do_mode_switch = [](qemu_plugin_id_t id, unsigned int cpu_index) {
                thread_data[cpu_index].output->Magic(
                        SIM_CMD_INSTRUMENT_MODE,
                        SIM_OPT_INSTRUMENT_WARMUP, 0);
                qemu_plugin_set_slomo_rate(1);
                current_mode = Sift::ModeMemory;
            };
            roi_cr3 = qemu_plugin_page_directory();
            break;
        case Sift::ModeDetailed:
            do_mode_switch = [](qemu_plugin_id_t id, unsigned int cpu_index) {
                thread_data[cpu_index].output->Magic(
                        SIM_CMD_INSTRUMENT_MODE,
                        SIM_OPT_INSTRUMENT_DETAILED, 0);
                qemu_plugin_set_slomo_rate(100000);
                current_mode = Sift::ModeDetailed;
            };
            roi_cr3 = qemu_plugin_page_directory();
            PLUGIN_PRINT_INFO("CR3: %lx", roi_cr3);
            break;
        default:
            PLUGIN_PRINT_ERROR("Mode unsupported.");
            return;
    }

    /* tb_flush runs asychronously when current cpu is in exclusive mode.
     * On acquiring the exclusivity, current cpu thread waits other vcpus to
     * finish their current blocks. This implicit dependency is unknown to
     * sniper backend, and can easily cause deadlock when performance model
     * is set (i.e. when simulate in detailed mode). Therefore we only do
     * tb_flush when we are not in detailed mode. So we flush tb before
     * entering/after leaving detailed mode. In the latter case this has to
     * be done via asynchronous tb_flush callback. */
    if (mode == Sift::ModeDetailed) {
        do_mode_switch(id, threadid);
        qemu_plugin_tb_flush();
    } else {
        qemu_plugin_vcpu_tb_flush(id, do_mode_switch);
    }
}

static void recorder_reset(qemu_plugin_id_t id, threadid_t threadid)
{
    if (!recording) {
        PLUGIN_PRINT_ERROR("not recording.");
        return;
    }

    // recorder_mode(id, threadid, "fastforward");
    roi_cr3 = qemu_plugin_page_directory();
    qemu_plugin_vcpu_tb_flush(id,
        [](qemu_plugin_id_t id, unsigned int cpu_index) {
            for (threadid_t threadid = 0; threadid < smp_vcpus; threadid ++) {
                thread_data[threadid].icount = 0UL;
                thread_data[threadid].icount_cacheonly = 0UL;
                thread_data[threadid].icount_cacheonly_pending = 0UL;
                thread_data[threadid].icount_detailed = 0UL;
                thread_data[threadid].icount_reported = 0UL;
                thread_data[threadid].loads = 0UL;
                thread_data[threadid].stores = 0UL;
                thread_data[threadid].icount_user = 0UL;
                thread_data[threadid].icount_other = 0UL;
            }
    });
}

QEMU_PLUGIN_EXPORT int qemu_plugin_control(qemu_plugin_id_t id,
                                           unsigned int vcpu_index,
                                           int argc, char **argv)
{
    if (argc == 0 || strcmp(argv[0], "trace")) {
        PLUGIN_PRINT_ERROR("Misdirected control for %s", argv[0]);
        return -1;
    }

    int c;
    char *dir = NULL;
    while ((c = getopt(argc, argv, "d:")) != -1) {
        switch (c) {
            case 'd':
                dir = optarg;
                break;
        }
    }

    if (optind >= argc || !strcmp(argv[optind], "start")) {
        recorder_start(id, dir);
    } else if (!strcmp(argv[optind], "mode")) {
        if (++optind >= argc) {
            PLUGIN_PRINT_ERROR("Missing mode argument.");
        } else {
            recorder_mode(id, vcpu_index, argv[optind]);
        }
    } else if (!strcmp(argv[optind], "stop")) {
        recorder_stop(id, vcpu_index);
    } else if (!strcmp(argv[optind], "reset")) {
        recorder_reset(id, vcpu_index);
    } else if (!strcmp(argv[optind], "gmm")) {
        if (optind + 3 >= argc) {
            PLUGIN_PRINT_ERROR("Missing gmm argument.");
        } else {
            recorder_gmm(id, vcpu_index, argv[optind + 1],
                         strtol(argv[optind + 2], NULL, 0),
                         strtol(argv[optind + 3], NULL, 0));
        }
    } else if (!strcmp(argv[optind], "magic")) {
        magic = strtol(argv[optind + 1], NULL, 0);
        PLUGIN_PRINT_INFO("magic = %lu", magic);
    }

    optind = 0;
    return 0;
}

// static bool handleAccessMemory(void *arg, Sift::MemoryLockType lock_signal,
//                                Sift::MemoryOpType mem_op, uint64_t d_addr,
//                                uint8_t *data_buffer, uint32_t data_size)
// {
//     if (lock_signal == Sift::MemLock) {
//         access_memory_mtx.lock();
//     }
//
//     if (mem_op == Sift::MemRead) {
//         qemu_plugin_virt_mem_rw(d_addr, data_buffer, data_size, false,
//                                 qemu_plugin_in_kernel());
//     } else if (mem_op == Sift::MemWrite) {
//         qemu_plugin_virt_mem_rw(d_addr, data_buffer, data_size, true,
//                                 qemu_plugin_in_kernel());
//     } else {
//         PLUGIN_PRINT_ERROR("Error: invalid memory operation type.");
//         return false;
//     }
//
//     if (lock_signal == Sift::MemLock) {
//         access_memory_mtx.unlock();
//     }
//
//     return true;
// }

static int closeFile(threadid_t threadid) {
    auto output = thread_data[threadid].output;
    thread_data[threadid].output = NULL;
    /* closeFile can be called twice for the vcpu thread that
     * called recorder_stop. */
    if (output) {
        output->Abort();
        delete output;
    }
    return 0;
}

static int openFile(threadid_t threadid, const char *dir)
{
    if (thread_data[threadid].output) {
        closeFile(threadid);
        ++thread_data[threadid].blocknum;
    }

    auto filename = string(dir) + "/vcpu"
                    + std::to_string(threadid) + ".sift";
    auto response = string(dir) + "/response.vcpu"
                    + std::to_string(threadid) + ".sift";

    thread_data[threadid].output = new Sift::Writer(filename.c_str(),
            NULL, false, response.c_str(), threadid, false, false,
            Sift::Writer::EXPLICIT);

    if (!thread_data[threadid].output->IsOpen()) {
        PLUGIN_PRINT_VCPU_ERROR(threadid, "Error: "
                "Unable to open the output file %s",
                filename.c_str());
        return -1;
    }

    return 0;
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    PLUGIN_PRINT_INFO("trace installed with id %lu", id);

    smp_vcpus = info->system.smp_vcpus;
    thread_data.resize(smp_vcpus);
    block_cnt.resize(smp_vcpus);

    return 0;
}
