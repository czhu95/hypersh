/*
 * Copyright (C) 2018, Emilio G. Cota <cota@braap.org>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */

#define __STDC_FORMAT_MACROS

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <glib.h>
#include <atomic>

#include "common.h"
#include "mcount.h"
#include "pmem.h"

#define HYPERCALL_MAGIC 712

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static enum qemu_plugin_mem_rw rw = QEMU_PLUGIN_MEM_RW;
static char buf[128] = "";
static std::atomic_uint64_t hypersh_mode = 0;

#define HS_MODE_PMEM        (1 << 0)
#define HS_MODE_MCOUNT      (1 << 1)

static void vcpu_syscall_cb(qemu_plugin_id_t id, unsigned int vcpu_index,
                            int64_t num, uint64_t a1, uint64_t a2,
                            uint64_t a3, uint64_t a4, uint64_t a5,
                            uint64_t a6, uint64_t a7, uint64_t a8)
{
    if (num == HYPERCALL_MAGIC) {
        if (!qemu_plugin_virt_mem_rw(a1, buf, 127, false, false)) {
            fprintf(stderr,
                    "Fail to read guest virtual memory 0x%" PRIx64 "\n", a1);
            return;
        }
        fprintf(stdout, "guest> %s", buf);


        int hyper_argc, c;
        char **hyper_argv;
        if (!g_shell_parse_argv(buf, &hyper_argc, &hyper_argv, NULL))
            return;

        if (!strcmp(hyper_argv[0], "pmem")) {
            if (hypersh_mode & HS_MODE_PMEM)
                return;

            uint32_t interval = 10000;
            uint32_t attr = 0;
            char *filename = NULL;
            while ((c = getopt(hyper_argc, hyper_argv, "f:kurw")) != -1) {
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
            hypersh_mode |= HS_MODE_PMEM;
        } else if (!strcmp(hyper_argv[0], "mcount")) {
            if (hypersh_mode & HS_MODE_MCOUNT)
                return;

            uint64_t total_mem = qemu_plugin_get_ram_size();
            uint32_t cpus = qemu_plugin_get_cpus();
            uint32_t mem_bin = 16 << 20;
            uint32_t acc_bin = 100;
            char *filename = NULL;
            while ((c = getopt(hyper_argc, hyper_argv, "f:a:m:")) != -1) {
                switch (c) {
                    case 'm':
                        mem_bin = strtol(optarg, NULL, 0) << 20;
                        break;
                    case 'a':
                        acc_bin = strtol(optarg, NULL, 0);
                        break;
                    case 'f':
                        filename = optarg;
                }
            }
            optind = 0;
            hypercall_mcount_init(filename, total_mem, cpus, mem_bin, acc_bin);
            hypersh_mode |= HS_MODE_MCOUNT;
        } else if (!strcmp(hyper_argv[0], "stop")) {
            if (hyper_argc == 1 || !strcmp(hyper_argv[1], "pmem")) {
                hypersh_mode &= ~HS_MODE_PMEM;
                hypercall_pmem_fini();
            }

            if (hyper_argc == 1 || !strcmp(hyper_argv[1], "mcount")) {
                hypersh_mode &= ~HS_MODE_MCOUNT;
                hypercall_mcount_fini();
            }
        }
    }
}

static void vcpu_mem(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
                     uint64_t vaddr, void *udata)
{
    if (hypersh_mode & HS_MODE_PMEM)
        hypercall_pmem_cb(cpu_index, meminfo, vaddr);

    if (hypersh_mode & HS_MODE_MCOUNT)
        hypercall_mcount_cb(cpu_index, meminfo, vaddr);
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
