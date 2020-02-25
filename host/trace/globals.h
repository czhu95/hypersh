#ifndef HYPERSH_TRACE_GLOBALS_H_
#define HYPERSH_TRACE_GLOBALS_H_

#include "sift_format.h"

#define LINE_SIZE_BYTES 64

extern uint32_t smp_vcpus;
extern const uint32_t FlowControlFF;
extern Sift::Mode current_mode;
extern bool any_thread_in_detail;

#endif
