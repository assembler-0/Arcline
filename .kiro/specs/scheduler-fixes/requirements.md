# Requirements Document

## Introduction

This specification addresses critical bugs in the Arcline aarch64 kernel's task scheduling, interrupt handling, and syscall subsystems. The current implementation has race conditions, incorrect context switching in IRQ handlers, missing preemption logic, and unsafe task termination that causes system hangs and unpredictable behavior.

## Glossary

- **Task**: A schedulable execution unit with its own context, stack, and state
- **Context**: The CPU register state (x0-x30, sp, pc, pstate) saved/restored during task switches
- **IRQ Handler**: Interrupt request handler that processes hardware interrupts
- **Preemption**: Forcibly switching from one task to another via timer interrupt
- **Zombie Task**: A task that has exited but whose resources haven't been fully cleaned up
- **Idle Task**: The special task (PID 0) that runs when no other tasks are runnable
- **EEVDF Scheduler**: Earliest Eligible Virtual Deadline First scheduling algorithm
- **Runqueue**: Red-black tree data structure holding ready-to-run tasks
- **SVC Handler**: Supervisor Call handler that processes system calls
- **Current Task**: The task currently executing on the CPU
- **Schedule Point**: A location in code where task switching may occur

## Requirements

### Requirement 1

**User Story:** As a kernel developer, I want IRQ handlers to correctly switch task contexts during preemption, so that tasks resume execution with the correct register state.

#### Acceptance Criteria

1. WHEN a timer interrupt occurs during task execution THEN the IRQ handler SHALL save the complete CPU context to the interrupted task's context structure
2. WHEN the IRQ handler calls schedule_preempt THEN the scheduler SHALL update the context pointer to point to the next task's context
3. WHEN the IRQ handler returns from schedule_preempt THEN the handler SHALL restore CPU state from the newly selected task's context structure
4. WHEN restoring context after preemption THEN the handler SHALL load all registers (x0-x30, sp, pc, pstate) from the correct context location
5. WHEN returning from IRQ THEN the CPU SHALL resume execution at the new task's saved PC with the new task's saved register state

### Requirement 2

**User Story:** As a kernel developer, I want timer interrupts to trigger task preemption, so that the scheduler can enforce fair CPU time distribution.

#### Acceptance Criteria

1. WHEN the timer IRQ fires THEN the timer handler SHALL call schedule_preempt with the saved context
2. WHEN schedule_preempt is called THEN the scheduler SHALL evaluate whether the current task's time slice has expired
3. WHEN a task's time slice expires THEN the scheduler SHALL select a new task according to EEVDF policy
4. WHEN no time slice has expired THEN schedule_preempt SHALL return without switching tasks
5. WHEN the timer handler completes THEN execution SHALL resume with the selected task

### Requirement 3

**User Story:** As a kernel developer, I want task termination to be safe and immediate, so that killed tasks cannot continue executing or block the system.

#### Acceptance Criteria

1. WHEN task_kill is called on the current task THEN the function SHALL mark the task as zombie and immediately invoke the scheduler
2. WHEN task_kill is called on a non-current task THEN the function SHALL remove it from the runqueue and mark it as zombie
3. WHEN a task calls sys_exit THEN the syscall SHALL mark the task as zombie and trigger rescheduling
4. WHEN the scheduler runs and the current task is zombie THEN the scheduler SHALL NOT re-enqueue the zombie task
5. WHEN all non-idle tasks are zombies THEN the scheduler SHALL select the idle task

### Requirement 4

**User Story:** As a kernel developer, I want syscalls that terminate tasks to trigger immediate rescheduling, so that zombie tasks do not resume execution.

#### Acceptance Criteria

1. WHEN sys_exit completes THEN the SVC handler SHALL check if rescheduling is needed before returning
2. WHEN sys_kill terminates the current task THEN the SVC handler SHALL trigger rescheduling
3. WHEN the SVC handler detects the current task is zombie THEN the handler SHALL call schedule_preempt to switch tasks
4. WHEN rescheduling occurs in SVC handler THEN the new task's context SHALL be loaded before eret
5. WHEN a non-terminating syscall completes THEN the SVC handler SHALL return to the same task without rescheduling

### Requirement 5

**User Story:** As a kernel developer, I want the idle task to always be available as a fallback, so that the system never has no runnable task.

#### Acceptance Criteria

1. WHEN the scheduler finds no tasks in the runqueue THEN the scheduler SHALL select the idle task (PID 0)
2. WHEN the idle task is selected THEN the scheduler SHALL NOT attempt to dequeue it from the runqueue
3. WHEN the idle task is running and a new task becomes ready THEN the scheduler SHALL preempt the idle task
4. WHEN all user tasks exit THEN the system SHALL continue running the idle task
5. WHEN the idle task is running THEN the task SHALL execute WFE instructions to save power

### Requirement 6

**User Story:** As a kernel developer, I want task state transitions to be atomic and consistent, so that race conditions do not corrupt scheduler state.

#### Acceptance Criteria

1. WHEN a task transitions from RUNNING to READY THEN the task SHALL be enqueued before any other scheduler operation
2. WHEN a task transitions from READY to RUNNING THEN the task SHALL be dequeued before context switch
3. WHEN a task transitions to ZOMBIE THEN the task SHALL be removed from all scheduler data structures
4. WHEN checking task state during scheduling THEN the state SHALL be consistent with runqueue membership
5. WHEN multiple state transitions occur THEN each transition SHALL complete atomically

### Requirement 7

**User Story:** As a kernel developer, I want the scheduler to handle edge cases gracefully, so that the system remains stable under all conditions.

#### Acceptance Criteria

1. WHEN schedule is called with no previous task THEN the scheduler SHALL handle the NULL prev pointer safely
2. WHEN the same task is selected again THEN the scheduler SHALL avoid unnecessary context switching
3. WHEN eevdf_dequeue is called on a task not in the queue THEN the function SHALL return safely without corruption
4. WHEN the runqueue is empty THEN eevdf_pick_next SHALL return NULL
5. WHEN a task is already zombie THEN attempts to kill it again SHALL be idempotent

### Requirement 8

**User Story:** As a kernel developer, I want proper integration between timer hardware and the scheduler, so that preemptive multitasking works correctly.

#### Acceptance Criteria

1. WHEN timer_init is called THEN the timer driver SHALL register an IRQ handler for the timer interrupt
2. WHEN the timer IRQ handler is invoked THEN the handler SHALL receive the saved CPU context as a parameter
3. WHEN the timer handler processes the interrupt THEN the handler SHALL acknowledge the timer hardware
4. WHEN the timer handler completes THEN the handler SHALL call schedule_preempt with the context
5. WHEN timer interrupts are enabled THEN preemption SHALL occur at regular intervals
