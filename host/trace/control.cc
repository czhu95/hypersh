#define __STDC_FORMAT_MACROS
#include <mutex>
#include <string.h>
#include <vector>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <atomic>

#include "callbacks.h"
#include "threads.h"
#include "globals.h"
#include "common.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

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

static std::atomic_bool reset_done;
// static void vcpu_mem(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
//                      uint64_t vaddr, void *udata)
// {
//     // hypercall_pmem_cb(cpu_index, meminfo, vaddr);
// }
// 
// static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
// {
//     size_t n = qemu_plugin_tb_n_insns(tb);
//     size_t i;
// 
//     for (i = 0; i < n; i++) {
//         struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
//         qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem,
//                                          QEMU_PLUGIN_CB_NO_REGS,
//                                          QEMU_PLUGIN_MEM_RW, NULL);
//     }
// }

QEMU_PLUGIN_EXPORT int qemu_plugin_control(qemu_plugin_id_t id,
                                           int argc, char **argv)
{
    fprintf(stdout, "trace control\n");
    for (auto i = 0; i < argc; i ++)
        fprintf(stdout, "%s\n", argv[i]);

    if (argc == 0 || strcmp(argv[0], "trace")) {
        fprintf(stderr, "Misdirected control for %s\n", argv[0]);
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
        if (dir == NULL) {
            dir = output_dir_template;
            strcpy(dir + strlen(dir) - 6, "XXXXXX");
            dir = mkdtemp(dir);
            assert(dir != NULL);
        }

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

        for (threadid_t i = 0; i < smp_vcpus; i ++)
            openFile(i, dir);

        qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
        qemu_plugin_tb_flush();

    } else if (!strcmp(argv[optind], "stop")) {
        reset_done = false;
        qemu_plugin_reset(id, [](qemu_plugin_id_t id) { reset_done = true; });
        while (!reset_done) ;

        fprintf(stderr, "Reset done.\n");

        for (threadid_t i = 0; i < smp_vcpus; i ++)
            closeFile(i);

        for (const auto& f : trace_files)
            unlink(f.c_str());

        trace_files.clear();
    }

    return 0;
}

static bool handleAccessMemory(void *arg, Sift::MemoryLockType lock_signal,
                               Sift::MemoryOpType mem_op, uint64_t d_addr,
                               uint8_t *data_buffer, uint32_t data_size)
{
    if (lock_signal == Sift::MemLock) {
        access_memory_mtx.lock();
    }

    if (mem_op == Sift::MemRead) {
        qemu_plugin_virt_mem_rw(d_addr, data_buffer, data_size, false,
                                qemu_plugin_in_kernel());
    } else if (mem_op == Sift::MemWrite) {
        qemu_plugin_virt_mem_rw(d_addr, data_buffer, data_size, true,
                                qemu_plugin_in_kernel());
    } else {
        fprintf(stderr, "Error: invalid memory operation type\n");
        return false;
    }

    if (lock_signal == Sift::MemLock) {
        access_memory_mtx.unlock();
    }

    return true;
}

static int closeFile(threadid_t threadid) {
    auto output = thread_data[threadid].output;
    thread_data[threadid].output = NULL;
    output->End();
    delete output;
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
            [](uint8_t *dst, const uint8_t *src, uint32_t size) {
                qemu_plugin_virt_mem_rw((uint64_t)src, dst, size, false,
                                        qemu_plugin_in_kernel());
            }, true, response.c_str(), threadid, false, false, false);

    if (!thread_data[threadid].output->IsOpen()) {
        fprintf(stderr, "[SIFT_RECORDER: %u] Error: "
                "Unable to open the output file %s\n",
                threadid, filename.c_str());
        return -1;
    }

    thread_data[threadid].output->setHandleAccessMemoryFunc(
            handleAccessMemory, reinterpret_cast<void *>(threadid));

    return 0;
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    fprintf(stdout, "trace installed with id %lu\n", id);
    for (auto i = 0; i < argc; i ++)
        fprintf(stdout, "%s\n", argv[i]);

    smp_vcpus = info->system.smp_vcpus;
    thread_data.resize(smp_vcpus);

    for (threadid_t threadid = 0; threadid < smp_vcpus; threadid ++) {
        thread_data[threadid].tid_ptr = 0;
        thread_data[threadid].thread_num = threadid;
        /* Since there is no cleanup function for plugins, bbv is never
         * deleted until qemu exits. We should be fine as bbv is reused and
         * allocated only once per vcpu. */
        thread_data[threadid].bbv = new Bbv();
        thread_data[threadid].blocknum = 0;
        thread_data[threadid].running = true;
    }

    return 0;
}