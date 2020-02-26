#ifndef HYPERSH_TRACE_CALLBACKS_H_
#define HYPERSH_TRACE_CALLBACKS_H_

#include "common.h"

void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb);

#endif
