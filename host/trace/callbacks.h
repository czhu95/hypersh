#ifndef HYPERSH_TRACE_CALLBACKS_H_
#define HYPERSH_TRACE_CALLBACKS_H_

#include "globals.h"
#include "common.h"

void vcpu_tb_trans_cb(qemu_plugin_id_t id, struct qemu_plugin_tb *tb);

void vcpu_idle_cb(qemu_plugin_id_t id, unsigned int threadid);

void vcpu_resume_cb(qemu_plugin_id_t id, unsigned int threadid);

#endif
