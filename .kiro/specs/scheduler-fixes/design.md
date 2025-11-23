# Design Document

## Overview

This design addresses critical bugs in the Arcline aarch64 kernel's task scheduling system. The primary issues are:

1. **IRQ context switching bug**: The IRQ handler saves context to stack but doesn't properly restore from the modified context after `schedule_preempt()` changes tasks
2. **Missing preemption trigger**: Timer handler exists but the integration with scheduler is incomplete
3. **Unsafe task termination**: Killing the current task doesn't immediately reschedule
4. **Missing post-syscall rescheduling**: SVC handler doesn't check if the current task became a zombie
5. **Idle task availability**: Idle task state management is inconsistent

The solution involves fixing the IRQ handler assembly code, ensuring proper timer-scheduler integration, adding rescheduling checks after syscalls, and making idle task handling robust.

## Architecture

### Component Interaction Flow

```
Timer Hardware → IRQ → exception.S:irq_handler_spx
                         ↓
                    handle_irq() → timer_irq_handler()
                         ↓
                    schedule_preempt(ctx)
                         ↓
                    [modifies ctx to point to next task]
                         ↓
                    return to exception.S
                         ↓
                    restore from modified ctx
                         ↓
                    eret to new task
```

### Syscall Flow with Rescheduling

```
Task → SVC instruction → exception.S:svc_handler
                            ↓
                       handle_svc(ctx)
                            ↓
                       do_syscall() → sys_exit/sys_kill
                            ↓
                       [task may become zombie]
                            ↓
                       return to svc_handler
                            ↓
                       check if current task is zombie
                            ↓
                       if zombie: call schedule_preempt(ctx)
                            ↓
                       restore from (possibly modified) ctx
                            ↓
                       eret to (possibly different) task
```

## Components and Interfaces

### 1. IRQ Handler (exception.S)

**Current Bug:**
```assembly
irq_handler_spx:
    sub sp, sp, #272
    # Save context to stack
    stp x0, x1, [sp, #0]
    ...
    mov x0, sp
    bl handle_irq          # May call schedule_preempt which modifies context
    
    # BUG: Still using old stack pointer!
    mov x9, sp             # x9 points to OLD task's stack
    ldr x10, [x9, #248]    # Loading OLD task's SP
    mov sp, x10            # Switching to OLD task's SP
    ldp x0, x1, [x9, #0]   # Restoring OLD task's registers
```

**Fixed Design:**
The IRQ handler must NOT restore from the stack frame. Instead, `schedule_preempt()` modifies the context structure in-place, and the handler must simply return, allowing the exception return mechanism to use the modified context.

**New Approach:**
```assembly
irq_handler_spx:
    sub sp, sp, #272
    # Save all registers to stack (creating cpu_context_t)
    stp x0, x1, [sp, #0]
    ...
    str x9, [sp, #264]     # pstate
    
    mov x0, sp             # Pass pointer to context
    bl handle_irq          # May modify context in-place
    
    # Restore from SAME stack location (which may have been modified)
    ldr x9, [sp, #256]     # Load (possibly new) PC
    msr elr_el1, x9
    ldr x9, [sp, #264]     # Load (possibly new) pstate
    msr spsr_el1, x9
    
    # Restore all registers from stack
    ldp x0, x1, [sp, #0]
    ...
    ldp x29, x30, [sp, #232]
    
    # Restore SP last
    ldr x9, [sp, #248]
    add sp, sp, #272       # Pop context frame first
    mov sp, x9             # Then switch to new SP
    
    eret
```

### 2. schedule_preempt() Function

**Interface:**
```c
void schedule_preempt(cpu_context_t *regs);
```

**Behavior:**
- Receives pointer to saved context on stack
- If current task should continue: returns without modification
- If switching tasks: modifies `*regs` in-place to contain next task's context
- Updates `current_task` global
- Handles zombie tasks by not re-enqueuing them

**Key Change:**
Instead of using `memcpy()` to copy context, directly modify the structure pointed to by `regs`.

### 3. Timer IRQ Handler

**Current Implementation:**
```c
static void timer_irq_handler(cpu_context_t *ctx, int irq, void *dev) {
    jiffies++;
    schedule_preempt(ctx);  // Already correct!
    write_cntp_tval(timer_freq / 100);
}
```

**Status:** Already correctly implemented. The timer handler properly calls `schedule_preempt()` with the context. The bug is in the assembly code that calls it.

### 4. SVC Handler (exception.S)

**Required Changes:**
After `handle_svc()` returns, check if the current task is now a zombie and needs rescheduling.

**New Design:**
```assembly
svc_handler:
    sub sp, sp, #272
    # Save context
    ...
    mov x0, sp
    bl handle_svc          # May call sys_exit or sys_kill
    
    # NEW: Check if we need to reschedule
    bl should_reschedule   # Returns 1 if current task is zombie
    cmp x0, #0
    b.eq .Lsvc_no_resched
    
    # Reschedule needed
    mov x0, sp
    bl schedule_preempt    # Will modify context
    
.Lsvc_no_resched:
    # Restore from (possibly modified) context
    ldr x9, [sp, #256]
    msr elr_el1, x9
    ...
    eret
```

**New C Function:**
```c
int should_reschedule(void) {
    task_t *curr = task_current();
    return (curr && curr->state == TASK_ZOMBIE) ? 1 : 0;
}
```

### 5. Task Termination

**task_kill() Changes:**
```c
int task_kill(task_t *task) {
    if (!task || task->pid == 0)
        return -1;
    
    // Remove from scheduler if in queue
    if (task->state == TASK_READY) {
        eevdf_dequeue(task);
    }
    
    task->state = TASK_ZOMBIE;
    pid_free(task->pid);
    
    // Remove from task list
    // ... existing code ...
    
    // NEW: If killing current task, schedule immediately
    if (task == task_current()) {
        task_set_current(NULL);  // Clear current before scheduling
        schedule();              // Will not return
    }
    
    return 0;
}
```

**task_exit() Changes:**
```c
void task_exit(int code) {
    if (!current_task)
        return;
    
    // Don't dequeue - we're RUNNING, not in queue
    current_task->state = TASK_ZOMBIE;
    pid_free(current_task->pid);
    
    task_set_current(NULL);  // Clear current
    schedule();              // Will not return
}
```

### 6. Idle Task Management

**Design Decision:**
The idle task should NEVER be in the runqueue. It's a special fallback task.

**schedule() Changes:**
```c
void schedule(void) {
    task_t *prev = task_current();
    uint64_t now = get_ns();
    
    // Handle previous task
    if (prev && prev->state == TASK_RUNNING) {
        eevdf_update_curr(prev, now);
        prev->state = TASK_READY;
        eevdf_enqueue(prev);
    }
    // If prev is zombie, don't enqueue
    
    // Pick next task
    task_t *next = eevdf_pick_next();
    if (!next) {
        // No tasks in queue, use idle
        next = task_find_by_pid(0);
        if (!next) {
            panic("No idle task!");
        }
    }
    
    // If staying with same task, just update state
    if (next == prev && prev) {
        if (prev->state == TASK_READY) {
            prev->state = TASK_RUNNING;
            eevdf_dequeue(prev);
        }
        return;
    }
    
    // Dequeue next task (unless it's idle)
    if (next->pid != 0) {
        eevdf_dequeue(next);
    }
    
    next->state = TASK_RUNNING;
    next->context.x23 = now;
    next->time_slice = eevdf_calc_slice(next);
    task_set_current(next);
    
    if (prev) {
        switch_to(&prev->context, &next->context);
    } else {
        // Jump to next task
        // ... existing code ...
    }
}
```

**schedule_preempt() Changes:**
```c
void schedule_preempt(cpu_context_t *regs) {
    task_t *prev = task_current();
    if (!prev)
        return;
    
    // Save context
    if (prev->state == TASK_RUNNING) {
        memcpy(&prev->context, regs, sizeof(cpu_context_t));
        uint64_t now = get_ns();
        eevdf_update_curr(prev, now);
        prev->state = TASK_READY;
        eevdf_enqueue(prev);
    }
    // If zombie, don't enqueue
    
    // Pick next
    task_t *next = eevdf_pick_next();
    if (!next) {
        if (prev->state == TASK_READY) {
            // Stay with prev
            next = prev;
        } else {
            // Prev is zombie, use idle
            next = task_find_by_pid(0);
            if (!next) {
                panic("No idle task!");
            }
        }
    }
    
    // If staying with same task
    if (next == prev) {
        if (prev->state == TASK_READY) {
            prev->state = TASK_RUNNING;
            eevdf_dequeue(prev);
        }
        return;  // Context unchanged
    }
    
    // Switch to next
    uint64_t now = get_ns();
    if (next->pid != 0) {
        eevdf_dequeue(next);
    }
    next->state = TASK_RUNNING;
    next->context.x23 = now;
    next->time_slice = eevdf_calc_slice(next);
    task_set_current(next);
    
    // Modify context in-place
    memcpy(regs, &next->context, sizeof(cpu_context_t));
}
```

## Data Models

### Task State Machine

```
[NEW] → TASK_READY → TASK_RUNNING → TASK_READY
           ↑              ↓              ↑
           └──────────────┘              │
                                         │
                    TASK_ZOMBIE ←────────┘
                         ↓
                    [REMOVED]
```

**State Invariants:**
- `TASK_RUNNING`: Exactly one task (current_task), NOT in runqueue
- `TASK_READY`: Zero or more tasks, ALL in runqueue
- `TASK_ZOMBIE`: Zero or more tasks, NOT in runqueue, will be cleaned up
- Idle task (PID 0): Special case, NEVER in runqueue

### Context Structure Layout

```c
typedef struct cpu_context {
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7;      // Offset 0-56
    uint64_t x8, x9, x10, x11, x12, x13, x14, x15; // Offset 64-120
    uint64_t x16, x17, x18, x19, x20, x21, x22, x23; // Offset 128-184
    uint64_t x24, x25, x26, x27, x28, x29, x30;    // Offset 192-240
    uint64_t sp;                                    // Offset 248
    uint64_t pc;                                    // Offset 256
    uint64_t pstate;                                // Offset 264
} cpu_context_t;  // Total size: 272 bytes
```

**Critical Note:** The assembly code must match these offsets exactly.

## Error Handling

### Panic Conditions

1. **No idle task found**: `panic("No idle task!")` - Should never happen if initialization is correct
2. **NULL current task in critical section**: Indicates corruption
3. **Runqueue corruption**: Detected by EEVDF red-black tree invariants

### Graceful Degradation

1. **All tasks zombie**: System falls back to idle task, continues running
2. **Task kill on already-zombie task**: Idempotent, returns success
3. **Dequeue on task not in queue**: Safe no-op, returns without error

### Debug Logging

Add strategic printk statements:
- Task state transitions
- Scheduler decisions (which task selected)
- Context switch events
- Zombie task detection

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Context save preserves all registers

*For any* CPU state at the time of interrupt, saving the context to a cpu_context_t structure and then loading from that structure should produce the same CPU state.
**Validates: Requirements 1.1**

### Property 2: schedule_preempt modifies context correctly

*For any* valid task set and current task, calling schedule_preempt with a context pointer should result in the context containing the selected next task's register state.
**Validates: Requirements 1.2**

### Property 3: Time slice expiration triggers selection

*For any* task whose vruntime indicates it should be preempted according to EEVDF policy, calling schedule_preempt should select a different task from the runqueue.
**Validates: Requirements 2.2, 2.3**

### Property 4: No unnecessary task switches

*For any* task with remaining time slice and no higher-priority tasks, calling schedule_preempt should leave the context unchanged.
**Validates: Requirements 2.4**

### Property 5: Killing current task reschedules immediately

*For any* task that is the current task, calling task_kill on it should set its state to ZOMBIE and result in a different task becoming current.
**Validates: Requirements 3.1**

### Property 6: Killing non-current task removes from queue

*For any* task in READY state that is not the current task, calling task_kill should remove it from the runqueue and set its state to ZOMBIE.
**Validates: Requirements 3.2**

### Property 7: sys_exit makes task zombie

*For any* task calling sys_exit, the task's state should become ZOMBIE and the task should not be in the runqueue afterward.
**Validates: Requirements 3.3**

### Property 8: Zombie tasks not re-enqueued

*For any* task in ZOMBIE state, calling schedule or schedule_preempt should not add the task to the runqueue.
**Validates: Requirements 3.4**

### Property 9: Empty queue selects idle task

*For any* scheduler state where the runqueue is empty, calling schedule or schedule_preempt should select the task with PID 0.
**Validates: Requirements 3.5, 5.1**

### Property 10: Non-terminating syscalls preserve task

*For any* syscall that does not terminate the task (not sys_exit or sys_kill on self), should_reschedule should return 0 and the current task should remain unchanged.
**Validates: Requirements 4.5**

### Property 11: Idle task never dequeued

*For any* task with PID 0, when it is selected by the scheduler, eevdf_dequeue should not be called on it.
**Validates: Requirements 5.2**

### Property 12: Idle task can be preempted

*For any* scheduler state where the idle task (PID 0) is current and the runqueue is non-empty, calling schedule_preempt should select a task from the runqueue.
**Validates: Requirements 5.3**

### Property 13: State transitions maintain queue consistency

*For any* task transitioning from RUNNING to READY, the task should be in the runqueue after the transition; for any task transitioning from READY to RUNNING, the task should not be in the runqueue after the transition.
**Validates: Requirements 6.1, 6.2**

### Property 14: Zombie tasks not in queue

*For any* task in ZOMBIE state, the task should not be present in the runqueue.
**Validates: Requirements 6.3**

### Property 15: State-queue invariant

*For any* task, if its state is READY then it is in the runqueue (unless it's PID 0), and if its state is RUNNING or ZOMBIE then it is not in the runqueue.
**Validates: Requirements 6.4**

### Property 16: Same task selection avoids context switch

*For any* scheduler invocation where the selected next task equals the current task, the switch_to function should not be called.
**Validates: Requirements 7.2**

### Property 17: Dequeue on absent task is safe

*For any* task not in the runqueue, calling eevdf_dequeue on it should not corrupt the runqueue structure or cause a crash.
**Validates: Requirements 7.3**

### Property 18: Killing zombie is idempotent

*For any* task already in ZOMBIE state, calling task_kill on it should return success and the task should remain in ZOMBIE state.
**Validates: Requirements 7.5**

## Testing Strategy

### Unit Tests

1. **Test idle task fallback**: Kill all user tasks, verify idle task runs
2. **Test task_kill on current task**: Verify immediate rescheduling
3. **Test sys_exit**: Verify task becomes zombie and doesn't resume
4. **Test context structure offsets**: Verify assembly matches C structure

### Property-Based Tests

Properties will be defined after prework analysis.

### Integration Tests

1. **Timer preemption test**: Create two tasks that print different characters, verify interleaving
2. **Task termination test**: Create task that exits, verify it doesn't run again
3. **Kill current task test**: Task kills itself via syscall, verify system continues
4. **Stress test**: Create many tasks, kill them randomly, verify no hangs

### Manual Testing

1. Boot kernel with timer enabled
2. Create multiple tasks
3. Observe preemption via printk output
4. Kill tasks via syscall
5. Verify system remains responsive
