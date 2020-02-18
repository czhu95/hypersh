#ifndef HYPERSH_MCOUNT_H_
#define HYPERSH_MCOUNT_H_

#include <stdio.h>
#include <vector>

#include "common.h"

using std::vector;
bool hypercall_mcount_init(char *filename, uint64_t total_mem, uint32_t ncores,
                           uint32_t mem_bin, uint32_t acc_bin);

void hypercall_mcount_fini();

void hypercall_mcount_cb(unsigned int cpu_id, qemu_plugin_meminfo_t meminfo,
                         uint64_t vaddr);

#endif
