#ifndef HYPERSH_TRACE_GLOBALS_H_
#define HYPERSH_TRACE_GLOBALS_H_

#include <shared_mutex>

#include "sift_format.h"

#define LINE_SIZE_BYTES 64

#define PLUGIN_NAME "SIFT_RECORDER"

#define PLUGIN_PRINT_ERROR(...)                                        \
do {                                                                   \
    fprintf(stderr, "[" PLUGIN_NAME "] " __VA_ARGS__);                 \
    fprintf(stderr, "\n");                                             \
} while (0)

#define PLUGIN_PRINT_VCPU_ERROR(vcpu, ...)                             \
do {                                                                   \
    fprintf(stderr, "[" PLUGIN_NAME ": %u] ", vcpu);                   \
    fprintf(stderr, __VA_ARGS__);                                      \
    fprintf(stderr, "\n");                                             \
} while (0)

#define PLUGIN_PRINT_INFO(...)                                         \
do {                                                                   \
    fprintf(stdout, "[" PLUGIN_NAME "] " __VA_ARGS__);                 \
    fprintf(stdout, "\n");                                             \
} while (0)

extern uint32_t smp_vcpus;
extern const uint32_t FlowControlFF;
extern const uint32_t FlowControl;
extern Sift::Mode current_mode;
extern bool any_thread_in_detail;
extern std::shared_mutex control_mtx;

#endif
