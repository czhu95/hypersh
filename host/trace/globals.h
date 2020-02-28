#ifndef HYPERSH_TRACE_GLOBALS_H_
#define HYPERSH_TRACE_GLOBALS_H_

#include <shared_mutex>

#include "sift_format.h"

#define LINE_SIZE_BYTES 64

#define PLUGIN_NAME "SIFT_RECORDER"

#define PLUGIN_PRINT_ERROR(...)                                        \
    fprintf(stderr, "[" PLUGIN_NAME "] " __VA_ARGS__);                 \
    fprintf(stderr, "\n");

#define PLUGIN_PRINT_VCPU_ERROR(vcpu, ...)                             \
    fprintf(stderr, "[" PLUGIN_NAME ": %u] ", vcpu);                   \
    fprintf(stderr, __VA_ARGS__);                                      \
    fprintf(stderr, "\n");

#define PLUGIN_PRINT_INFO(...)                                         \
    fprintf(stdout, "[" PLUGIN_NAME "] " __VA_ARGS__);                 \
    fprintf(stdout, "\n");

extern uint32_t smp_vcpus;
extern const uint32_t FlowControlFF;
extern Sift::Mode current_mode;
extern bool any_thread_in_detail;
extern std::shared_mutex control_mtx;

#endif
