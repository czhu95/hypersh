#ifndef HYPERSH_TRACE_H_
#define HYPERSH_TRACE_H_

#include "sift_writer.h"
#include "bbv_count.h"
#include "common.h"

typedef uint32_t threadid_t;

#if defined(TARGET_X86_64)
typedef uint64_t ADDRINT;
#else
typedef uint32_t ADDRINT;
#endif

#define LINE_SIZE_BYTES 64

typedef struct {
   Sift::Writer *output;
   uint64_t dyn_addresses[Sift::MAX_DYNAMIC_ADDRESSES];
   uint32_t num_dyn_addresses;
   Bbv *bbv;
   uint64_t thread_num;
   ADDRINT bbv_base;
   uint64_t bbv_count;
   ADDRINT bbv_last;
   bool bbv_end;
   uint64_t blocknum;
   uint64_t icount;
   uint64_t icount_cacheonly;
   uint64_t icount_cacheonly_pending;
   uint64_t icount_detailed;
   uint64_t icount_reported;
   ADDRINT last_syscall_number;
   ADDRINT last_syscall_returnval;
   uint64_t flowcontrol_target;
   ADDRINT tid_ptr;
   ADDRINT last_routine;
   ADDRINT last_call_site;
   bool last_syscall_emulated;
   bool running;
   bool should_send_threadinfo;
} __attribute__((packed,aligned(LINE_SIZE_BYTES))) thread_data_t;

#endif
