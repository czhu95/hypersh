#ifndef HYPERSH_TRACE_GLOBALS_H_
#define HYPERSH_TRACE_GLOBALS_H_

#include <shared_mutex>
#include <vector>
#include <unordered_map>
#include <mutex>

#include "sift_format.h"

#define LINE_SIZE_BYTES 64
#define PLUGIN_NAME "SIFT_RECORDER"

static std::mutex io_mtx;

#define PLUGIN_PRINT_ERROR(...)                                        \
do {                                                                   \
    std::lock_guard<std::mutex> guard(io_mtx);                         \
    fprintf(stderr, "[" PLUGIN_NAME "] " __VA_ARGS__);                 \
    fprintf(stderr, "\n");                                             \
} while (0)

#define PLUGIN_PRINT_VCPU_ERROR(vcpu, ...)                             \
do {                                                                   \
    std::lock_guard<std::mutex> guard(io_mtx);                         \
    fprintf(stderr, "[" PLUGIN_NAME ": %u] ", vcpu);                   \
    fprintf(stderr, __VA_ARGS__);                                      \
    fprintf(stderr, "\n");                                             \
} while (0)

#define PLUGIN_PRINT_VCPU_INFO(vcpu, ...)                             \
do {                                                                   \
    std::lock_guard<std::mutex> guard(io_mtx);                         \
    fprintf(stdout, "[" PLUGIN_NAME ": %u] ", vcpu);                   \
    fprintf(stdout, __VA_ARGS__);                                      \
    fprintf(stdout, "\n");                                             \
} while (0)

#define PLUGIN_PRINT_INFO(...)                                         \
do {                                                                   \
    std::lock_guard<std::mutex> guard(io_mtx);                         \
    fprintf(stdout, "[" PLUGIN_NAME "] " __VA_ARGS__);                 \
    fprintf(stdout, "\n");                                             \
} while (0)

extern uint32_t smp_vcpus;
extern const uint32_t FlowControlFF;
extern const uint32_t FlowControl;
extern Sift::Mode current_mode;
extern bool any_thread_in_detail;
extern std::shared_mutex control_mtx;

extern uint64_t roi_cr3;

extern std::vector<std::unordered_map<uint64_t, uint64_t>> block_cnt;

#endif
