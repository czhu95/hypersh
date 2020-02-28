#define __STDC_FORMAT_MACROS

#include <stdlib.h>
#include <map>
#include <shared_mutex>
#include <atomic>
#include <string.h>
#include <vector>

#include "mcount.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

enum mcount_mode
{
    MCOUNT_ON,
    MCOUNT_OFF
};

struct mcount_config_t
{
    mcount_mode mode = MCOUNT_OFF;
    FILE *report;
    uint32_t mem_bin, acc_bin;
    uint64_t total_mem;
};

using std::vector;
using std::map;
using std::shared_mutex;

static mcount_config_t cfg;

static vector<vector<uint32_t>> kmaps, umaps;
static map<std::string, uint64_t> tags;
static std::atomic_uint64_t acc_cnt, acc_io_cnt;

static uint32_t ncores;

static shared_mutex flush_mtx;

static void flush(vector<vector<uint32_t>> &maps);

static bool hypercall_mcount_init(char *report, uint64_t total_mem,
                                  uint32_t mem_bin, uint32_t acc_bin)
{
    cfg.report = report ? fopen(report, "w") : stdout;
    cfg.total_mem = total_mem;
    cfg.mem_bin = mem_bin;
    cfg.acc_bin = acc_bin;

    cfg.mode = MCOUNT_ON;

    fprintf(cfg.report, "cfg = {\"report\": %s, \"total_mem\": %" PRIu64 ", "
            "\"mem_bin\": %u, \"acc_bin\": %u, \"ncores\": %u}\n",
            report ? report : "stdout", cfg.total_mem, cfg.mem_bin,
            cfg.acc_bin, ncores);

    kmaps.resize(ncores);
    umaps.resize(ncores);

    for (auto& maps: kmaps) {
        maps.resize(total_mem / mem_bin);
        std::fill(maps.begin(), maps.end(), 0);
    }

    for (auto& maps: umaps) {
        maps.resize(total_mem / mem_bin);
        std::fill(maps.begin(), maps.end(), 0);
    }

    tags.clear();
    acc_cnt = 0;
    acc_io_cnt = 0;

    return true;
}

static void hypercall_mcount_fini()
{
    cfg.mode = MCOUNT_OFF;
    if (cfg.report) {
        std::shared_lock lock(flush_mtx);
        fprintf(cfg.report, "tags = {");
        for (const auto &tag : tags)
            fprintf(cfg.report, "\"%s\": %lu, ", tag.first.c_str(),
                    tag.second);
        fprintf(cfg.report, "}\n");
        fprintf(cfg.report, "acc_cnt = %lu; acc_io_cnt = %lu\n",
                uint64_t(acc_cnt), uint64_t(acc_io_cnt));
        if (cfg.report != stdout)
            fclose(cfg.report);
        cfg.report = NULL;
    }
}

static void hypercall_mcount_cb(unsigned int cpu_id,
                                qemu_plugin_meminfo_t meminfo, uint64_t vaddr)
{
    if (cfg.mode == MCOUNT_OFF)
        return;

    auto hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);
    uint64_t paddr = qemu_plugin_hwaddr_device_offset(hwaddr);

    // auto is_store = qemu_plugin_mem_is_store(meminfo);
    auto is_io = qemu_plugin_hwaddr_is_io(hwaddr);
    auto is_kernel = qemu_plugin_in_kernel();

    if (++acc_cnt % cfg.acc_bin == 0) {
        std::unique_lock lock(flush_mtx);
        if (cfg.mode == MCOUNT_OFF)
            return;

        fprintf(cfg.report, "K = {");
        flush(kmaps);
        fprintf(cfg.report, "}; U = {");
        flush(umaps);
        fprintf(cfg.report, "}\n");
    }

    if (is_io) {
        acc_io_cnt ++;
    } else if (is_kernel) {
        std::shared_lock lock(flush_mtx);
        kmaps[cpu_id][paddr / cfg.mem_bin] ++;
    } else {
        std::shared_lock lock(flush_mtx);
        umaps[cpu_id][paddr / cfg.mem_bin] ++;
    }
}

static void vcpu_mem(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
                     uint64_t vaddr, void *udata)
{
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
                                         QEMU_PLUGIN_MEM_RW, NULL);
    }
}

static void hypercall_mcount_tag(const char *tag)
{
    tags[tag] = uint64_t(acc_cnt);
}

void flush(vector<vector<uint32_t>> &maps)
{
    for (unsigned int i = 0; i < ncores; i ++) {
        fprintf(cfg.report, "%d = {", i);
        auto &map = maps[i];
        for (uint32_t j = 0; j < map.size(); j ++) {
            if (map[j] != 0)
                fprintf(cfg.report, "%u: %u, ", j, map[j]);
            map[j] = 0;
        }
        fprintf(cfg.report, "}, ");
    }
    fflush(cfg.report);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_control(qemu_plugin_id_t id,
                                           unsigned int vcpu_index,
                                           int argc, char **argv)
{
    fprintf(stdout, "mcount control\n");
    for (auto i = 0; i < argc; i ++)
        fprintf(stdout, "%s\n", argv[i]);

    if (argc == 0 || strcmp(argv[0], "mcount")) {
        fprintf(stderr, "Misdirected control for %s\n", argv[0]);
        return -1;
    }

    uint64_t total_mem = qemu_plugin_ram_size();
    uint32_t mem_bin = 16 << 20;
    uint32_t acc_bin = 100;
    char *filename = NULL;
    char *tag = NULL;
    int c;
    while ((c = getopt(argc, argv, "f:a:m:")) != -1) {
        switch (c) {
            case 'm':
                mem_bin = strtol(optarg, NULL, 0) << 20;
                break;
            case 'a':
                acc_bin = strtol(optarg, NULL, 0);
                break;
            case 'f':
                filename = optarg;
                break;
            case 't':
                tag = optarg;
                break;
        }
    }
    if (optind >= argc || !strcmp(argv[optind], "start")) {
        if (!hypercall_mcount_init(filename, total_mem, mem_bin, acc_bin)) {
            fprintf(stderr, "mcount init failed.\n");
            goto fail;
        }
        qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
        qemu_plugin_tb_flush();
    } else if (!strcmp(argv[optind], "tag")) {
        if (!tag) {
            fprintf(stderr, "mcount tag missing argument.\n");
            goto fail;
        }
        hypercall_mcount_tag(tag);
    } else if (!strcmp(argv[optind], "stop")) {
        hypercall_mcount_fini();
        qemu_plugin_reset(id, NULL);
    } else {
        goto fail;
    }
    return 0;

fail:
    optind = 0;
    return -1;
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    ncores = info->system.smp_vcpus;
    return 0;
}

