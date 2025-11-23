#pragma once

#include <kernel/sched/task.h>

extern void exception_init(void);
void handle_svc(cpu_context_t *ctx);