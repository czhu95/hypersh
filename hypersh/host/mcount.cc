#define __STDC_FORMAT_MACROS

#include <stdlib.h>
#include <map>

#include "mcount.h"

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
    uint32_t total_mem;
    uint32_t ncores;
};

using std::vector;
using std::map;

static mcount_config_t cfg;

static vector<vector<uint32_t>> kmaps, umaps;
static map<std::string, uint32_t> tags;
static uint32_t acc_cnt, acc_io;

static void flush(vector<vector<uint32_t>> &maps);

bool hypercall_mcount_init(char *report, uint32_t total_mem, uint32_t ncores,
                           uint32_t mem_bin, uint32_t acc_bin)
{
    if (report == NULL)
        cfg.report = stderr;

    cfg.report = fopen(report, "w");
    cfg.total_mem = total_mem;
    cfg.mem_bin = mem_bin;
    cfg.acc_bin = acc_bin;

    cfg.mode = MCOUNT_ON;
    cfg.ncores = ncores;

    kmaps.resize(ncores);
    umaps.resize(ncores);

    for (auto& maps: kmaps) {
        maps.clear();
        maps.resize(total_mem / mem_bin, 0);
    }

    for (auto& maps: umaps) {
        maps.clear();
        maps.resize(total_mem / mem_bin, 0);
    }

    tags.clear();
    acc_cnt = 0;
    acc_io = 0;

    return true;
}

void hypercall_mcount_fini()
{
    if (cfg.report) {
        fclose(cfg.report);
        cfg.report = NULL;
    }

    cfg.mode = MCOUNT_OFF;
}

void hypercall_mcount_cb(unsigned int cpu_id, qemu_plugin_meminfo_t meminfo,
                         uint64_t vaddr)
{
    if (cfg.mode == MCOUNT_OFF)
        return;

    acc_cnt ++;

    auto hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);
    auto paddr = qemu_plugin_hwaddr_device_offset(hwaddr);

    // auto is_store = qemu_plugin_mem_is_store(meminfo);
    auto is_io = qemu_plugin_hwaddr_is_io(hwaddr);
    auto is_kernel = qemu_plugin_in_kernel();

    if (acc_cnt % cfg.acc_bin == 0) {
        fprintf(cfg.report, "K = {");
        flush(kmaps);
        fprintf(cfg.report, "}; U = {");
        flush(umaps);
        fprintf(cfg.report, "}\n");
    }

    if (is_io) {
        acc_io ++;
    } else if (is_kernel) {
        kmaps[cpu_id][paddr / cfg.mem_bin] ++;
    } else {
        umaps[cpu_id][paddr / cfg.mem_bin] ++;
    }
}

void flush(vector<vector<uint32_t>> &maps)
{
    for (unsigned int i = 0; i < cfg.ncores; i ++) {
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
