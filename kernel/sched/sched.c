#include <drivers/timer.h>
#include <kernel/sched/eevdf.h>
#include <kernel/sched/task.h>
#include <string.h>
#include <kernel/printk.h>

extern void switch_to(cpu_context_t *prev, cpu_context_t *next);

void schedule(void) {
    task_t *prev = task_current();
    uint64_t now = get_ns();

    if (prev && prev->state == TASK_RUNNING) {
        eevdf_update_curr(prev, now);
        prev->state = TASK_READY;
        eevdf_enqueue(prev);
    }

    task_t *next = eevdf_pick_next();
    if (!next) {
        // If no other tasks are runnable, pick the idle task (PID 0)
        next = task_find_by_pid(0);
        if (!next) {
            // This should ideally not happen if idle task is always enqueued
            return;
        }
    }

    if (next == prev && prev) {
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

    if (prev) {
        switch_to(&prev->context, &next->context);
    } else {
        // No previous task (it was killed), jump directly to next task
        __asm__ volatile(
            "mov sp, %0\n"
            "mov x29, %1\n"
            "mov x30, %2\n"
            "br %3\n"
            :
            : "r"(next->context.sp), "r"(next->context.x29),
              "r"(next->context.x30), "r"(next->context.pc)
            : "memory");
        __builtin_unreachable();
    }
}

void schedule_preempt(cpu_context_t *regs) {
    task_t *prev = task_current();
    if (!prev)
        return;

    // Save context and enqueue if task is still running normally
    if (prev->state == TASK_RUNNING) {
        memcpy(&prev->context, regs, sizeof(cpu_context_t));
        uint64_t now = get_ns();
        eevdf_update_curr(prev, now);
        prev->state = TASK_READY;
        eevdf_enqueue(prev);
    } else if (prev->state == TASK_ZOMBIE) {
        printk("[SCHED] prev PID %d is ZOMBIE\n", prev->pid);
    }

    task_t *next = eevdf_pick_next();
    if (!next) {
        // No runnable tasks in queue, try to use prev or idle
        if (prev->state == TASK_READY) {
            next = prev;
        } else {
            // Prev is zombie/blocked, fall back to idle
            next = task_find_by_pid(0);
            if (!next) {
                printk("[SCHED] FATAL: No idle task found!\n");
                return;
            }
            printk("[SCHED] No tasks in queue, using idle\n");
        }
    }

    if (next == prev) {
        // Staying with same task
        if (prev->state == TASK_READY) {
            prev->state = TASK_RUNNING;
            eevdf_dequeue(prev);
        }
        return;
    }

    // Switch to next task
    uint64_t now = get_ns();
    eevdf_dequeue(next);
    next->state = TASK_RUNNING;
    next->context.x23 = now;
    next->time_slice = eevdf_calc_slice(next);
    task_set_current(next);

    memcpy(regs, &next->context, sizeof(cpu_context_t));
}
