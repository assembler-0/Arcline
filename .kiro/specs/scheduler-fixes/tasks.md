# Implementation Plan

- [x] 1. Fix IRQ handler context switching in exception.S
  - Modify `irq_handler_spx` to properly restore from modified context after schedule_preempt
  - Remove incorrect SP switching logic that uses old stack frame
  - Ensure all registers are restored from the (possibly modified) context structure
  - Update PC and pstate loading to use modified values
  - Fix SP restoration to happen after popping context frame
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5_

- [x] 2. Add post-syscall rescheduling to SVC handler
  - [x] 2.1 Implement should_reschedule() function in C
    - Create function that checks if current task is zombie
    - Return 1 if rescheduling needed, 0 otherwise
    - _Requirements: 4.1, 4.3_
  
  - [x] 2.2 Modify svc_handler in exception.S to check for rescheduling
    - Call should_reschedule after handle_svc returns
    - If result is non-zero, call schedule_preempt with context
    - Restore from (possibly modified) context before eret
    - _Requirements: 4.1, 4.2, 4.3, 4.4_

- [x] 3. Fix task termination to handle current task
  - [x] 3.1 Update task_exit() to clear current task before scheduling
    - Remove eevdf_dequeue call (task is RUNNING, not in queue)
    - Set current_task to NULL before calling schedule
    - Add comment explaining that schedule() will not return
    - _Requirements: 3.3_
  
  - [x] 3.2 Update task_kill() to immediately reschedule if killing current task
    - Check if task == task_current() after marking zombie
    - If yes, set current_task to NULL and call schedule()
    - Add comment that schedule() will not return in this case
    - _Requirements: 3.1_

- [x] 4. Fix idle task handling in scheduler
  - [x] 4.1 Update schedule() to never dequeue idle task
    - Add check: if (next->pid != 0) before calling eevdf_dequeue
    - Ensure idle task is selected when runqueue is empty
    - Add panic if idle task not found
    - _Requirements: 5.1, 5.2_
  
  - [x] 4.2 Update schedule_preempt() to never dequeue idle task
    - Add check: if (next->pid != 0) before calling eevdf_dequeue
    - Ensure idle task is selected when runqueue is empty and prev is zombie
    - Handle case where prev is idle and should stay idle
    - _Requirements: 5.1, 5.2, 5.3_

- [x] 5. Ensure zombie tasks are not re-enqueued
  - [x] 5.1 Update schedule() to skip enqueue for zombie tasks
    - Check prev->state before enqueuing
    - Only enqueue if state is TASK_RUNNING (which we just changed to READY)
    - Add comment explaining zombie tasks are not enqueued
    - _Requirements: 3.4, 6.3_
  
  - [x] 5.2 Update schedule_preempt() to skip enqueue for zombie tasks
    - Check prev->state before enqueuing
    - Only enqueue if state is TASK_RUNNING
    - Handle case where prev is zombie and queue is empty
    - _Requirements: 3.4, 6.3_

- [x] 6. Add state transition consistency checks
  - [x] 6.1 Verify state-queue invariants in schedule()
    - Ensure READY tasks are enqueued
    - Ensure RUNNING tasks are dequeued
    - Ensure ZOMBIE tasks are never in queue
    - _Requirements: 6.1, 6.2, 6.3, 6.4_
  
  - [x] 6.2 Verify state-queue invariants in schedule_preempt()
    - Ensure state transitions happen in correct order
    - Ensure dequeue happens before state change to RUNNING
    - Ensure enqueue happens after state change to READY
    - _Requirements: 6.1, 6.2, 6.4_

- [x] 7. Add edge case handling
  - [x] 7.1 Handle NULL prev in schedule()
    - Check if prev is NULL before accessing prev->state
    - Skip context save and enqueue if prev is NULL
    - Add comment explaining when this can happen
    - _Requirements: 7.1_
  
  - [x] 7.2 Add early return for same-task selection
    - Check if next == prev before context switching
    - Update state and dequeue if needed
    - Return without calling switch_to
    - _Requirements: 7.2_
  
  - [x] 7.3 Make eevdf_dequeue safe for absent tasks
    - Verify current implementation returns safely if task not found
    - Add comment documenting this behavior
    - _Requirements: 7.3_
  
  - [x] 7.4 Make task_kill idempotent for zombies
    - Check if task is already zombie at start
    - Return success without re-processing if already zombie
    - _Requirements: 7.5_

- [ ] 8. Verify timer integration
  - [ ] 8.1 Confirm timer handler calls schedule_preempt
    - Review timer_irq_handler implementation
    - Verify it passes context pointer to schedule_preempt
    - Verify timer hardware is acknowledged
    - _Requirements: 8.1, 8.3, 8.4_
  
  - [ ] 8.2 Verify timer IRQ is properly registered
    - Check that irq_install_handler is called with TIMER_IRQ
    - Verify timer_irq_handler is registered
    - Verify IRQ is enabled
    - _Requirements: 8.1, 8.5_

- [ ] 9. Add debug logging
  - [ ] 9.1 Add logging to schedule functions
    - Log task switches with PIDs
    - Log zombie task detection
    - Log idle task selection
    - Keep logging minimal to avoid performance impact
  
  - [ ] 9.2 Add logging to task termination
    - Log task_exit calls with PID
    - Log task_kill calls with PID and state
    - Log when current task is killed

- [ ] 10. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ]* 11. Write property-based tests for scheduler correctness
  - [ ]* 11.1 Write property test for context save/restore
    - **Property 1: Context save preserves all registers**
    - **Validates: Requirements 1.1**
  
  - [ ]* 11.2 Write property test for schedule_preempt context modification
    - **Property 2: schedule_preempt modifies context correctly**
    - **Validates: Requirements 1.2**
  
  - [ ]* 11.3 Write property test for time slice expiration
    - **Property 3: Time slice expiration triggers selection**
    - **Validates: Requirements 2.2, 2.3**
  
  - [ ]* 11.4 Write property test for unnecessary switches
    - **Property 4: No unnecessary task switches**
    - **Validates: Requirements 2.4**
  
  - [ ]* 11.5 Write property test for killing current task
    - **Property 5: Killing current task reschedules immediately**
    - **Validates: Requirements 3.1**
  
  - [ ]* 11.6 Write property test for killing non-current task
    - **Property 6: Killing non-current task removes from queue**
    - **Validates: Requirements 3.2**
  
  - [ ]* 11.7 Write property test for sys_exit
    - **Property 7: sys_exit makes task zombie**
    - **Validates: Requirements 3.3**
  
  - [ ]* 11.8 Write property test for zombie re-enqueue
    - **Property 8: Zombie tasks not re-enqueued**
    - **Validates: Requirements 3.4**
  
  - [ ]* 11.9 Write property test for empty queue
    - **Property 9: Empty queue selects idle task**
    - **Validates: Requirements 3.5, 5.1**
  
  - [ ]* 11.10 Write property test for non-terminating syscalls
    - **Property 10: Non-terminating syscalls preserve task**
    - **Validates: Requirements 4.5**
  
  - [ ]* 11.11 Write property test for idle task dequeue
    - **Property 11: Idle task never dequeued**
    - **Validates: Requirements 5.2**
  
  - [ ]* 11.12 Write property test for idle task preemption
    - **Property 12: Idle task can be preempted**
    - **Validates: Requirements 5.3**
  
  - [ ]* 11.13 Write property test for state transitions
    - **Property 13: State transitions maintain queue consistency**
    - **Validates: Requirements 6.1, 6.2**
  
  - [ ]* 11.14 Write property test for zombie queue membership
    - **Property 14: Zombie tasks not in queue**
    - **Validates: Requirements 6.3**
  
  - [ ]* 11.15 Write property test for state-queue invariant
    - **Property 15: State-queue invariant**
    - **Validates: Requirements 6.4**
  
  - [ ]* 11.16 Write property test for same task selection
    - **Property 16: Same task selection avoids context switch**
    - **Validates: Requirements 7.2**
  
  - [ ]* 11.17 Write property test for dequeue safety
    - **Property 17: Dequeue on absent task is safe**
    - **Validates: Requirements 7.3**
  
  - [ ]* 11.18 Write property test for kill idempotence
    - **Property 18: Killing zombie is idempotent**
    - **Validates: Requirements 7.5**

- [x] 12. Write integration tests
  - [x] 12.1 Write test for timer preemption
    - Create two tasks that increment counters
    - Verify both tasks make progress
    - Verify preemption occurs
  
  - [x] 12.2 Write test for task termination
    - Create task that exits
    - Verify task becomes zombie
    - Verify task doesn't run again
  
  - [x] 12.3 Write test for killing current task
    - Create task that kills itself
    - Verify system continues running
    - Verify other tasks still run
  
  - [x] 12.4 Write stress test
    - Create many tasks
    - Kill them in random order
    - Verify no hangs or crashes
