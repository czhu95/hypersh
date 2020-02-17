#include <mutex>

#include "pmem.h"

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

bool hypercall_pmem_init(char *filename, uint32_t hc_pmem_int,
                         hc_pmem_attr_t hc_pmem_attr)
{
    pmem_file = filename ? fopen(filename, "w") : stdout;
    pmem_int = hc_pmem_int;
    pmem_attr = hc_pmem_attr ? hc_pmem_attr : 0xF;
    pmem_count = 0;
    mode = PMEM_ON;
    return true;
}

void hypercall_pmem_fini()
{
    if (pmem_file) {
        if (pmem_file != stdout)
            fclose(pmem_file);
        pmem_file = NULL;
    }

    mode = PMEM_OFF;
}

void hypercall_pmem_cb(unsigned int cpu_id, qemu_plugin_meminfo_t meminfo,
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


