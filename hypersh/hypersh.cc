/*
 * Copyright (C) 2018, Emilio G. Cota <cota@braap.org>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */

#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <map>

#include <qemu-plugin.h>

#define HYPERCALL_MAGIC 712

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static enum qemu_plugin_mem_rw rw = QEMU_PLUGIN_MEM_RW;
static char buf[128] = "";

/* ------------------------------------------------------------------------- */
/* pmem hypercall. */
#define HC_PMEM_READ        0x1
#define HC_PMEM_WRITE       0x2
#define HC_PMEM_KERNEL      0x4
#define HC_PMEM_USER        0x8
typedef uint8_t hc_pmem_attr_t;

static bool hc_pmem_on = false;
static uint32_t hc_pmem_int;
static hc_pmem_attr_t hc_pmem_attr;
static FILE *hc_pmem_file;
static uint32_t hc_pmem_count;

static void hypercall_pmem(unsigned int cpu_id, qemu_plugin_meminfo_t meminfo,
                           uint64_t vaddr)
{
    struct qemu_plugin_hwaddr *hwaddr;
    uint64_t paddr;
    bool is_store;
    bool is_kernel;
    hc_pmem_attr_t attr;

    if (hc_pmem_count++ % hc_pmem_int == 0) {
        hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);
        paddr = qemu_plugin_hwaddr_device_offset(hwaddr);

        is_store = qemu_plugin_mem_is_store(meminfo);
        is_kernel = qemu_plugin_in_kernel();

        attr = (is_store ? HC_PMEM_WRITE : HC_PMEM_READ) |
               (is_kernel ? HC_PMEM_KERNEL : HC_PMEM_USER);

        if (hc_pmem_attr & attr)
            fprintf(hc_pmem_file,
                    "%u %c %c 0x%016" PRIx64 " 0x%08" PRIx64 "\n", cpu_id,
                    is_kernel ? 'k' : 'u', is_store ? 'w' : 'r', vaddr, paddr);
    }

}
/* ------------------------------------------------------------------------- */

// static FILE *hc_mcount_file;
static std::vector<uint32_t> kmap, umap;

static void hypercall_mcount(unsigned int cpu_id, qemu_plugin_meminfo_t meminfo,
                             uint64_t vaddr)
{
    kmap.clear();
    umap.clear();
}

static void vcpu_syscall_cb(qemu_plugin_id_t id, unsigned int vcpu_index,
                            int64_t num, uint64_t a1, uint64_t a2,
                            uint64_t a3, uint64_t a4, uint64_t a5,
                            uint64_t a6, uint64_t a7, uint64_t a8)
{
    char *hc, *arg;

    if (num == HYPERCALL_MAGIC) {
        if (!qemu_plugin_virt_mem_rw(a1, buf, 127, false, false)) {
            fprintf(stderr,
                    "Fail to read guest virtual memory 0x%" PRIx64 "\n", a1);
            return;
        }
        fprintf(stderr, "guest> %s", buf);

        hc = strtok(buf, " \n");
        if (hc == NULL)
            return;

        if (!strcmp(hc, "pmem")) {
            arg = strtok(NULL, " \n");
            if (arg && !strcmp(arg, "on")) {
                hc_pmem_on = true;
                hc_pmem_int = 10000;
                hc_pmem_attr = 0xF;
                hc_pmem_file = stderr;
                hc_pmem_count = 0;
            } else if (arg && !strcmp(arg, "off")) {
                hc_pmem_on = false;
            } else {
                fprintf(stderr, "Unknown argument for pmem\n");
            }
        }
    }
}

static void vcpu_mem(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
                     uint64_t vaddr, void *udata)
{
    if (hc_pmem_on)
        hypercall_pmem(cpu_index, meminfo, vaddr);

    if (false)
        hypercall_mcount(cpu_index, meminfo, vaddr);
}



static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    size_t i;

    for (i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         rw, NULL);
    }
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    qemu_plugin_register_vcpu_syscall_cb(id, vcpu_syscall_cb);
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    // qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    return 0;
}
