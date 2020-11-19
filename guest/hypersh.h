#ifndef HYPERSH_H_
#define HYPERSH_H_

#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdio.h>

#define HYPERCALL_MAGIC 712

extern int magic;

// static pthread_mutex_t m;

static inline void hypersh_exec(const char *cmd) {
    // pthread_mutex_lock(&m);
    // volatile char c = *cmd;
    syscall(HYPERCALL_MAGIC, cmd);
    // pthread_mutex_unlock(&m);
}

static inline void hypersh_printf(const char *format, ... ) {

    va_list(args);
    va_start(args, format);
    char pbuf[128] = "p ";
    vsnprintf(pbuf + 2, 126, format, args);
    va_end(args);

    hypersh_exec(pbuf);
}

static inline void trace_start()
{
    hypersh_exec("trace start");
}

static inline void trace_stop()
{
    hypersh_exec("trace stop");
}

static inline void trace_detailed()
{
    hypersh_exec("trace mode detailed");
}

static inline void set_magic() {
    char cmd[50] = "";
    snprintf(cmd, 50, "trace magic %p", &magic);
    hypersh_exec(cmd);
}

static inline void hypercall() {
    int src = 0;
    // volatile int v = magic;
    __asm__ __volatile__ (
    "cmpxchgl %1, %0;\t"
    : "=m" (magic)
    : "r" (src)
    : "memory");
}

static inline void seg_create(void *addr, size_t size)
{
    char cmd[50] = "";
    snprintf(cmd, 50, "trace gmm create %p 0x%lx", addr, size);
    hypersh_exec(cmd);
}

static inline void seg_assign(void *addr, int policy)
{
    char cmd[50] = "";
    snprintf(cmd, 50, "trace gmm assign %p %d", addr, policy);
    hypersh_exec(cmd);
}

static inline void seg_delete(void *addr)
{
    char cmd[50] = "";
    snprintf(cmd, 50, "trace gmm delete %p 0", addr);
    hypersh_exec(cmd);
}

static inline void user_msg(void *payload1, void *payload2)
{
    char cmd[50] = "";
    snprintf(cmd, 50, "trace gmm message %p %p", payload1, payload2);
    hypersh_exec(cmd);
}

static inline void gmm_sync()
{
    char cmd[50] = "trace gmm sync 0 0";
    hypersh_exec(cmd);
}

#endif
