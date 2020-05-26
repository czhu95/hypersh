#ifndef HYPERSH_H_
#define HYPERSH_H_

#include <unistd.h>
#include <stdarg.h>

#define HYPERCALL_MAGIC 712

inline void hypersh_exec(const char *cmd) {
    volatile char c = *cmd;
    volatile int rc = syscall(HYPERCALL_MAGIC, cmd);
}

static char pbuf[128] = "p ";

void hypersh_printf(const char *format, ... ) {

    va_list(args);
    va_start(args, format);
    vsnprintf(pbuf + 2, 126, format, args);
    va_end(args);

    hypersh_exec(pbuf);
}

#endif
