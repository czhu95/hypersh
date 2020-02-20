#ifndef HYPERSH_H_
#define HYPERSH_H_

#include <unistd.h>

#define HYPERCALL_MAGIC 712

inline void hypersh_exec(const char *cmd) {
    volatile int rc = syscall(HYPERCALL_MAGIC, cmd);
}

#endif
