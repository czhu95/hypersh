#define __STDC_FORMAT_MACROS

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <glib.h>
#include <atomic>
#include <string>

#include "common.h"
#include "mcount.h"
#include "pmem.h"

#define HYPERCALL_MAGIC 712

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static char buf[128] = "";

static void vcpu_syscall_cb(qemu_plugin_id_t id, unsigned int vcpu_index,
                            int64_t num, uint64_t a1, uint64_t a2,
                            uint64_t a3, uint64_t a4, uint64_t a5,
                            uint64_t a6, uint64_t a7, uint64_t a8)
{
    if (num != HYPERCALL_MAGIC)
        return;

    if (!qemu_plugin_virt_mem_rw(a1, buf, 127, false)) {
        fprintf(stderr,
                "Fail to read guest virtual memory 0x%" PRIx64 "\n", a1);
        return;
    }

    int hyper_argc;
    char **hyper_argv;
    std::string soname;
    qemu_plugin_id_t target_id;

    if (!g_shell_parse_argv(buf, &hyper_argc, &hyper_argv, NULL) ||
        hyper_argc == 0)
        return;

    if (!strcmp(hyper_argv[0], "p")) {
        fprintf(stdout, "[%u] %s", vcpu_index, buf);
        goto freev;
    }

    fprintf(stdout, "[%u] guest> %s\n", vcpu_index, buf);

    if (!strcmp(hyper_argv[0], "debug")) {
        qemu_plugin_id_t pmem_id = qemu_plugin_find_id("/libpmem.so");
        fprintf(stdout, "%lu\n", pmem_id);
        qemu_plugin_send_control(pmem_id, vcpu_index, hyper_argc - 1,
                                 hyper_argv + 1);
        goto freev;
    }

#ifdef DEBUG
    soname = std::string("/lib") + hyper_argv[0] + ".sod";
#else
    soname = std::string("/lib") + hyper_argv[0] + ".so";
#endif
    target_id = qemu_plugin_find_id(soname.c_str());
    if (target_id == QEMU_PLUGIN_ID_NULL) {
        fprintf(stderr, "Unknown/Unloaded plugin %s\n", hyper_argv[0]);
        goto freev;
    }

    if (qemu_plugin_send_control(target_id, vcpu_index, hyper_argc,
                                 hyper_argv) == -1) {
        fprintf(stderr, "Failed sending control to %s\n", hyper_argv[0]);
        goto freev;
    }

freev:
    g_strfreev(hyper_argv);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    qemu_plugin_register_vcpu_syscall_cb(id, vcpu_syscall_cb);
    return 0;
}
