#include <drivers/timer.h>
#include <kernel/sched/eevdf.h>
#include <kernel/sched/task.h>
#include <string.h>
#include <kernel/printk.h>
#include <kernel/panic.h>

extern void switch_to(cpu_context_t *prev, cpu_context_t *next);

void schedule(void) {
    task_t *prev = task_current();
    uint64_t now = get_ns();

    // Handle NULL prev case - can happen when current task was killed
    // and task_set_current(NULL) was called before schedule()
    // In this case, skip context save and enqueue operations
    if (prev) {
        // Only enqueue prev if it was running normally
        // Zombie tasks are not re-enqueued
        // Idle task (PID 0) is never enqueued
        if (prev->state == TASK_RUNNING) {
            eevdf_update_curr(prev, now);
            prev->state = TASK_READY;
            
            // Only enqueue if not the idle task
            if (prev->pid != 0) {
                eevdf_enqueue(prev);
                
                // Verify state-queue invariant: READY tasks should be enqueued
                if (!eevdf_is_queued(prev)) {
                    panic("schedule: READY task PID %d not in queue after enqueue", prev->pid);
                }
            }
        }
        
        // Verify zombie tasks are not in queue
        if (prev->state == TASK_ZOMBIE && eevdf_is_queued(prev)) {
            panic("schedule: ZOMBIE task PID %d is in queue", prev->pid);
        }
    }

    task_t *next = eevdf_pick_next();
    if (!next) {
        // If no other tasks are runnable, pick the idle task (PID 0)
        next = task_find_by_pid(0);
        if (!next) {
            panic("No idle task found!");
        }
    }

    // Early return if staying with same task (only if prev is not NULL)
    if (next == prev && prev) {
        if (prev->state == TASK_READY) {
            prev->state = TASK_RUNNING;
            eevdf_dequeue(prev);
            
            // Verify state-queue invariant: RUNNING tasks should be dequeued
            if (eevdf_is_queued(prev)) {
                panic("schedule: RUNNING task PID %d still in queue after dequeue", prev->pid);
            }
        }
        return;
    }

    // Only dequeue if not the idle task (PID 0)
    if (next->pid != 0) {
        eevdf_dequeue(next);
        
        // Verify state-queue invariant: task should be removed from queue
        if (eevdf_is_queued(next)) {
            panic("schedule: task PID %d still in queue after dequeue", next->pid);
        }
    }
    
    next->state = TASK_RUNNING;
    
    // Verify state-queue invariant: RUNNING tasks should not be in queue
    if (eevdf_is_queued(next)) {
        panic("schedule: RUNNING task PID %d is in queue", next->pid);
    }
    
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

    // Only enqueue prev if it was running normally
    // Zombie tasks are not re-enqueued
    // Idle task (PID 0) is never enqueued
    if (prev->state == TASK_RUNNING) {
        memcpy(&prev->context, regs, sizeof(cpu_context_t));
        uint64_t now = get_ns();
        eevdf_update_curr(prev, now);
        
        // State transition: RUNNING -> READY (must happen before enqueue)
        prev->state = TASK_READY;
        
        // Enqueue happens after state change to READY (but not for idle task)
        if (prev->pid != 0) {
            eevdf_enqueue(prev);
            
            // Verify state-queue invariant: READY tasks should be enqueued
            if (!eevdf_is_queued(prev)) {
                panic("schedule_preempt: READY task PID %d not in queue after enqueue", prev->pid);
            }
        }
    } else if (prev->state == TASK_ZOMBIE) {
        // Zombie task - don't save context or enqueue
        printk("[SCHED] prev PID %d is ZOMBIE\n", prev->pid);
        
        // Verify zombie tasks are not in queue
        if (eevdf_is_queued(prev)) {
            panic("schedule_preempt: ZOMBIE task PID %d is in queue", prev->pid);
        }
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
                panic("No idle task found!");
            }
            printk("[SCHED] No tasks in queue, using idle\n");
        }
    }

    if (next == prev) {
        // Staying with same task
        if (prev->state == TASK_READY) {
            // Dequeue happens before state change to RUNNING
            eevdf_dequeue(prev);
            
            // Verify task was removed from queue
            if (eevdf_is_queued(prev)) {
                panic("schedule_preempt: task PID %d still in queue after dequeue", prev->pid);
            }
            
            // State transition: READY -> RUNNING (must happen after dequeue)
            prev->state = TASK_RUNNING;
            
            // Verify state-queue invariant: RUNNING tasks should not be in queue
            if (eevdf_is_queued(prev)) {
                panic("schedule_preempt: RUNNING task PID %d is in queue", prev->pid);
            }
        }
        return;
    }

    // Switch to next task
    uint64_t now = get_ns();
    
    // Only dequeue if not the idle task (PID 0)
    // Dequeue happens before state change to RUNNING
    if (next->pid != 0) {
        eevdf_dequeue(next);
        
        // Verify task was removed from queue
        if (eevdf_is_queued(next)) {
            panic("schedule_preempt: task PID %d still in queue after dequeue", next->pid);
        }
    }
    
    // State transition: READY -> RUNNING (must happen after dequeue)
    next->state = TASK_RUNNING;
    
    // Verify state-queue invariant: RUNNING tasks should not be in queue
    if (eevdf_is_queued(next)) {
        panic("schedule_preempt: RUNNING task PID %d is in queue", next->pid);
    }
    
    next->context.x23 = now;
    next->time_slice = eevdf_calc_slice(next);
    task_set_current(next);

    memcpy(regs, &next->context, sizeof(cpu_context_t));
}
