#define __STDC_FORMAT_MACROS

#include <stdlib.h>
#include <map>
#include <string.h>
#include <vector>
#include <atomic>
#include <iostream>
#include "common.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static std::atomic_uint64_t icount;

static void vcpu_interrupt_cb(qemu_plugin_id_t id, unsigned int cpu_id)
{
    std::cout << "icount = " << icount << std::endl;
    icount = 0;
}

static void vcpu_tb_exec_cb(unsigned int cpu_id, void *udata)
{
    icount += (uint64_t)udata;
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    qemu_plugin_register_vcpu_tb_exec_cb(tb, vcpu_tb_exec_cb,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         (void *)n);
}



QEMU_PLUGIN_EXPORT int qemu_plugin_control(qemu_plugin_id_t id,
                                           unsigned int vcpu_index,
                                           int argc, char **argv)
{
    fprintf(stdout, "slomo control\n");
    for (auto i = 0; i < argc; i ++)
        fprintf(stdout, "%s\n", argv[i]);

    if (argc == 0 || strcmp(argv[0], "slomo")) {
        fprintf(stderr, "Misdirected control for %s\n", argv[0]);
        return -1;
    }

    int c;
    uint64_t rate = 1;
    while ((c = getopt(argc, argv, "r:")) != -1) {
        switch (c) {
            case 'r':
                rate = strtol(optarg, NULL, 0);
                break;
        }
    }
    if (optind >= argc || !strcmp(argv[optind], "start")) {
        qemu_plugin_set_slomo_rate(rate);
        qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
        qemu_plugin_register_vcpu_interrupt_cb(id, vcpu_interrupt_cb);
        qemu_plugin_tb_flush();
    } else if (!strcmp(argv[optind], "stop")) {
        qemu_plugin_reset(id, NULL);
    } else {
        goto fail;
    }

    optind = 0;
    return 0;

fail:
    optind = 0;
    return -1;
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    return 0;
}

