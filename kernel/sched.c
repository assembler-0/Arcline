#include <kernel/task.h>
#include <string.h>

#include "kernel/printk.h"

extern void switch_to(cpu_context_t *prev, cpu_context_t *next);
extern task_t* task_get_next_ready(void);

void schedule(void) {
    task_t *prev = task_current();
    if (!prev) return;
    
    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
    }
    
    task_t *next = task_get_next_ready();
    if (!next) {
        if (prev->state == TASK_READY) prev->state = TASK_RUNNING;
        return;
    }
    
    if (next == prev) {
        if (prev->state == TASK_READY) prev->state = TASK_RUNNING;
        return;
    }
    
    next->state = TASK_RUNNING;
    task_set_current(next);
    
    switch_to(&prev->context, &next->context);
}

void schedule_preempt(cpu_context_t *regs) {
    task_t *prev = task_current();
    if (!prev) return;
    
    memcpy(&prev->context, regs, sizeof(cpu_context_t));
    
    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
    }
    
    task_t *next = task_get_next_ready();
    if (!next) next = prev;
    
    if (next == prev) {
        if (prev->state == TASK_READY) prev->state = TASK_RUNNING;
        return;
    }
    
    next->state = TASK_RUNNING;
    task_set_current(next);
    
    memcpy(regs, &next->context, sizeof(cpu_context_t));
}
