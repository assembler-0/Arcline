#include <kernel/sched/task.h>
#include <kernel/sched/eevdf.h>
#include <string.h>
#include <drivers/timer.h>

extern void switch_to(cpu_context_t *prev, cpu_context_t *next);

void schedule(void) {
    task_t *prev = task_current();
    if (!prev) return;
    
    uint64_t now = get_ns();
    
    if (prev->state == TASK_RUNNING) {
        eevdf_update_curr(prev, now);
        prev->state = TASK_READY;
        eevdf_enqueue(prev);
    }
    
    task_t *next = eevdf_pick_next();
    if (!next) {
        if (prev->state == TASK_READY) {
            prev->state = TASK_RUNNING;
            eevdf_dequeue(prev);
        }
        return;
    }
    
    if (next == prev) {
        if (prev->state == TASK_READY) {
            prev->state = TASK_RUNNING;
            eevdf_dequeue(prev);
        }
        return;
    }
    
    eevdf_dequeue(next);
    next->state = TASK_RUNNING;
    next->context.x23 = now;
    next->time_slice = eevdf_calc_slice(next);
    task_set_current(next);
    
    switch_to(&prev->context, &next->context);
}

void schedule_preempt(cpu_context_t *regs) {
    task_t *prev = task_current();
    if (!prev) return;
    
    memcpy(&prev->context, regs, sizeof(cpu_context_t));
    
    uint64_t now = get_ns();
    
    if (prev->state == TASK_RUNNING) {
        eevdf_update_curr(prev, now);
        prev->state = TASK_READY;
        eevdf_enqueue(prev);
    }
    
    task_t *next = eevdf_pick_next();
    if (!next) next = prev;
    
    if (next == prev) {
        if (prev->state == TASK_READY) {
            prev->state = TASK_RUNNING;
            eevdf_dequeue(prev);
        }
        return;
    }
    
    eevdf_dequeue(next);
    next->state = TASK_RUNNING;
    next->context.x23 = now;
    next->time_slice = eevdf_calc_slice(next);
    task_set_current(next);
    
    memcpy(regs, &next->context, sizeof(cpu_context_t));
}
