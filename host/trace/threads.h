#ifndef HYPERSH_TRACE_H_
#define HYPERSH_TRACE_H_

#include <vector>

#include "sift_writer.h"
#include "bbv_count.h"

typedef uint32_t threadid_t;

#if defined(TARGET_X86_64)
typedef uint64_t ADDRINT;
#else
typedef uint32_t ADDRINT;
#endif

typedef struct {
   Sift::Writer *output;
   Bbv *bbv;
   uint64_t thread_num;
   ADDRINT bbv_base;
   uint64_t bbv_count;
   ADDRINT bbv_last;
   bool bbv_end;
   ADDRINT pc;
   ADDRINT br_addr;
   ADDRINT br_target;
   ADDRINT br_fallthrough;
   ADDRINT tb_vaddr1;
   ADDRINT tb_vaddr2;
   ADDRINT rd_vaddr1;
   ADDRINT rd_vaddr2;
   ADDRINT wr_vaddr;
   uint64_t wr_size;
   uint64_t blocknum;
   uint64_t icount;
   uint64_t icount_cacheonly;
   uint64_t icount_cacheonly_pending;
   uint64_t icount_detailed;
   uint64_t icount_reported;
   uint64_t flowcontrol_target;
   ADDRINT tid_ptr;
   ADDRINT last_routine;
   ADDRINT last_call_site;
   bool running;
   ADDRINT pgd;
} __attribute__((packed,aligned(LINE_SIZE_BYTES))) thread_data_t;

extern std::vector<thread_data_t> thread_data;

#endif
