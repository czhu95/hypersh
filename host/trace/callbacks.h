#ifndef HYPERSH_TRACE_CALLBACKS_H_
#define HYPERSH_TRACE_CALLBACKS_H_

#include "globals.h"
#include "common.h"

void vcpu_tb_trans_cb(qemu_plugin_id_t id, struct qemu_plugin_tb *tb);

void vcpu_idle_cb(qemu_plugin_id_t id, unsigned int threadid);

void vcpu_resume_cb(qemu_plugin_id_t id, unsigned int threadid);

void vcpu_interrupt_cb(qemu_plugin_id_t id, unsigned int threadid);

void vcpu_interrupt_ret_cb(qemu_plugin_id_t id, unsigned int threadid);

void vcpu_syscall_cb(qemu_plugin_id_t id, unsigned int vcpu_index,
                     int64_t num, uint64_t a1, uint64_t a2,
                     uint64_t a3, uint64_t a4, uint64_t a5,
                     uint64_t a6, uint64_t a7, uint64_t a8);

void vcpu_syscall_ret_cb(qemu_plugin_id_t id, unsigned int vcpu_idx,
                         int64_t num, int64_t ret);

#endif
