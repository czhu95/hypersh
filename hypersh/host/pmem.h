#ifndef HYPERSH_PMEM_H_
#define HYPERSH_PMEM_H_

#include <stdio.h>

#include "common.h"

#define HS_PMEM_READ        0x1
#define HS_PMEM_WRITE       0x2
#define HS_PMEM_KERNEL      0x4
#define HS_PMEM_USER        0x8
typedef uint8_t hc_pmem_attr_t;

bool hypercall_pmem_init(char *filename, uint32_t hc_pmem_int,
                         hc_pmem_attr_t hc_pmem_attr);

void hypercall_pmem_fini();

void hypercall_pmem_cb(unsigned int cpu_id, qemu_plugin_meminfo_t meminfo,
                       uint64_t vaddr);

#endif
