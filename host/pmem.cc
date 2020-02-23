#define __STDC_FORMAT_MACROS
#include <mutex>
#include <string.h>

#include "pmem.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

enum pmem_mode
{
    PMEM_ON,
    PMEM_OFF
};

static uint32_t pmem_int;
static hc_pmem_attr_t pmem_attr;
static FILE *pmem_file;
static uint32_t pmem_count;
static pmem_mode mode;

static std::mutex file_lock, count_lock;

static bool hypercall_pmem_init(char *filename, uint32_t hc_pmem_int,
                                hc_pmem_attr_t hc_pmem_attr)
{
    pmem_file = filename ? fopen(filename, "w") : stdout;
    pmem_int = hc_pmem_int;
    pmem_attr = hc_pmem_attr ? hc_pmem_attr : 0xF;
    pmem_count = 0;
    mode = PMEM_ON;
    return true;
}

static void hypercall_pmem_fini()
{
    if (pmem_file) {
        if (pmem_file != stdout)
            fclose(pmem_file);
        pmem_file = NULL;
    }

    mode = PMEM_OFF;
}

static void hypercall_pmem_cb(unsigned int cpu_id, qemu_plugin_meminfo_t meminfo,
                              uint64_t vaddr)
{
    if (mode == PMEM_OFF)
        return;

    struct qemu_plugin_hwaddr *hwaddr;
    uint64_t paddr;
    bool is_store;
    bool is_kernel;
    hc_pmem_attr_t attr;

    count_lock.lock();
    if (pmem_count++ % pmem_int == 0) {
        count_lock.unlock();
        hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);
        paddr = qemu_plugin_hwaddr_device_offset(hwaddr);

        is_store = qemu_plugin_mem_is_store(meminfo);
        is_kernel = qemu_plugin_in_kernel();

        attr = (is_store ? HS_PMEM_WRITE : HS_PMEM_READ) |
               (is_kernel ? HS_PMEM_KERNEL : HS_PMEM_USER);

        if ((pmem_attr & attr) == attr) {
            const std::lock_guard<std::mutex> lock(file_lock);
            fprintf(pmem_file,
                    "%u %c %c 0x%016" PRIx64 " 0x%08" PRIx64 "\n", cpu_id,
                    is_kernel ? 'k' : 'u', is_store ? 'w' : 'r', vaddr, paddr);
        }
    } else {
        count_lock.unlock();
    }
}

static void vcpu_mem(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
                     uint64_t vaddr, void *udata)
{
    hypercall_pmem_cb(cpu_index, meminfo, vaddr);
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    size_t i;

    for (i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         QEMU_PLUGIN_MEM_RW, NULL);
    }
}

QEMU_PLUGIN_EXPORT int qemu_plugin_control(qemu_plugin_id_t id,
                                           int argc, char **argv)
{
    fprintf(stdout, "pmem control\n");
    for (auto i = 0; i < argc; i ++)
        fprintf(stdout, "%s\n", argv[i]);

    if (argc == 0 || strcmp(argv[0], "pmem")) {
        fprintf(stderr, "Misdirected control for %s\n", argv[0]);
        return -1;
    }

    argc --;
    argv ++;

    if (argc == 0 || !strcmp(argv[0], "start")) {
        uint32_t interval = 10000;
        uint32_t attr = 0;
        char *filename = NULL;
        int c;
        while ((c = getopt(argc, argv, "f:kurw")) != -1) {
            switch (c) {
                case 'k':
                    attr |= HS_PMEM_KERNEL;
                    break;
                case 'u':
                    attr |= HS_PMEM_USER;
                    break;
                case 'r':
                    attr |= HS_PMEM_READ;
                    break;
                case 'w':
                    attr |= HS_PMEM_WRITE;
                    break;
                case 'f':
                    filename = optarg;
            }
        }
        optind = 0;
        fprintf(stderr, "pmem_mode: %x\n", attr);

        hypercall_pmem_init(filename, interval, attr);
        qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
        qemu_plugin_tb_flush();
    } else if (!strcmp(argv[0], "stop")) {
        hypercall_pmem_fini();
        qemu_plugin_reset(id, NULL);
    } else {
        return -1;
    }

    return 0;
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    fprintf(stdout, "pmem installed with id %lu\n", id);
    for (auto i = 0; i < argc; i ++)
        fprintf(stdout, "%s\n", argv[i]);

    return 0;
}
