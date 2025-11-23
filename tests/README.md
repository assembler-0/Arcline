# Integration Tests

This directory contains integration tests for the Arcline kernel, including scheduler and memory subsystems.

## Tests Included

## Scheduler Tests

### 1. Timer Preemption Test (`test_timer_preemption`)
- **Purpose**: Verify that timer interrupts trigger task preemption
- **Method**: Creates two tasks that increment separate counters
- **Success Criteria**: Both counters should increase, proving both tasks ran (preemption occurred)

### 2. Task Termination Test (`test_task_termination`)
- **Purpose**: Verify that tasks properly exit and become zombies
- **Method**: Creates a task that exits after incrementing a counter 5 times
- **Success Criteria**: 
  - Task state becomes TASK_ZOMBIE
  - Counter stops at exactly 5 (task doesn't run after exit)

### 3. Killing Current Task Test (`test_killing_current_task`)
- **Purpose**: Verify that a task can kill itself and the system continues
- **Method**: Creates a task that kills itself via sys_kill, plus another task
- **Success Criteria**:
  - Kill-self task stops executing after sys_kill
  - Other task continues running (system doesn't hang)
  - Kill-self task becomes zombie

### 4. Stress Test (`test_stress`)
- **Purpose**: Verify scheduler stability under load
- **Method**: Creates 10 tasks, kills them in random order
- **Success Criteria**:
  - All tasks make some progress
  - All tasks eventually become zombies
  - No system hangs or crashes

## Memory Tests

### 1. PMM Basic Allocation
- **Purpose**: Verify physical memory manager can allocate and free pages
- **Method**: Allocates 3 pages, verifies they're unique, frees them
- **Success Criteria**: All allocations succeed and addresses are unique

### 2. PMM Write/Read Patterns
- **Purpose**: Verify memory can be written and read correctly
- **Method**: Writes different patterns (0xAA, 0x55) to a page and verifies
- **Success Criteria**: All bytes match expected patterns

### 3. PMM Stress Test
- **Purpose**: Verify PMM handles many allocations correctly
- **Method**: Allocates 128 pages, writes unique patterns, verifies all
- **Success Criteria**: All pages allocated, patterns verified, no corruption

### 4. VMM Basic Mapping
- **Purpose**: Verify virtual memory mapping works
- **Method**: Maps a physical page to virtual address, writes and reads
- **Success Criteria**: Data written can be read back correctly

### 5. VMM Permission Changes
- **Purpose**: Verify memory protection attributes can be changed
- **Method**: Maps page with R/W, writes data, changes to R-only, reads
- **Success Criteria**: Data persists after permission change

### 6. vmalloc Basic
- **Purpose**: Verify kernel heap allocator works
- **Method**: Allocates 8KB, writes pattern, verifies
- **Success Criteria**: Allocation succeeds and data is correct

### 7. vmalloc Fragmentation
- **Purpose**: Verify allocator handles fragmentation
- **Method**: Allocates 3 blocks, frees middle one, reallocates
- **Success Criteria**: Reallocation succeeds (ideally reuses freed space)

### 8. Memory Isolation
- **Purpose**: Verify separate allocations don't interfere
- **Method**: Allocates 2 blocks with different patterns, verifies both
- **Success Criteria**: Each block maintains its own pattern

### 9. Large Allocation
- **Purpose**: Verify large allocations work (64KB)
- **Method**: Allocates 64KB, writes complex pattern, verifies
- **Success Criteria**: Large allocation succeeds and data is correct

### 10. Concurrent Allocation Pattern
- **Purpose**: Verify allocator handles mixed-size allocations
- **Method**: Allocates 32 blocks of varying sizes with unique patterns
- **Success Criteria**: All allocations succeed, patterns verified, no leaks

## Building with Tests

By default, the tests are compiled but not run. When `RUN_INTEGRATION_TESTS` is enabled:
- Memory integration tests replace the basic memtest_run()
- Scheduler integration tests run after initialization
- Both test suites provide comprehensive validation

To enable test execution:

```bash
# Configure with test flag
cmake -B cmake-build-debug -S . -DRUN_INTEGRATION_TESTS=ON

# Build
cmake --build cmake-build-debug

# Run in QEMU
cmake --build cmake-build-debug --target run
```

Or manually:

```bash
qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -serial stdio \
    -kernel cmake-build-debug/arcline.bin \
    -m 1024
```

## Test Output

When tests run, you'll see output like:

```
========================================
  SCHEDULER INTEGRATION TESTS
========================================

=== TEST: Timer Preemption ===
[TEST] Created tasks: A (PID 1), B (PID 2)
[TEST] Task A started (PID 1)
[TEST] Task B started (PID 2)
...
[TEST] PASSED: Both tasks made progress (preemption working)
=== END TEST ===

...
```

## Requirements Validated

These integration tests validate the following requirements from the scheduler-fixes spec:

- **Requirement 1**: IRQ handlers correctly switch task contexts during preemption
- **Requirement 2**: Timer interrupts trigger task preemption
- **Requirement 3**: Task termination is safe and immediate
- **Requirement 4**: Syscalls that terminate tasks trigger immediate rescheduling
- **Requirement 5**: Idle task is always available as fallback
- **Requirement 6**: Task state transitions are atomic and consistent
- **Requirement 7**: Scheduler handles edge cases gracefully

## Notes

- Tests run in kernel mode and use printk for output
- Tests are designed to be observable through serial console output
- Some tests use busy-wait loops for timing (not ideal but sufficient for testing)
- Tests assume timer interrupts are enabled and working
- The stress test creates 10 tasks by default (configurable via STRESS_TEST_TASKS)

## Implementation Details

The tests explicitly call `schedule()` to yield control from the idle task to the created test tasks. This is necessary because:
- Tests run in the context of the idle task (PID 0)
- Without explicit yielding, created tasks would only run on timer interrupts
- Explicit scheduling ensures tests complete in reasonable time

## Bug Fixes During Testing

During the implementation of these tests, a critical bug was discovered and fixed:

**Bug**: The idle task (PID 0) was being enqueued in the runqueue when it transitioned from RUNNING to READY state in both `schedule()` and `schedule_preempt()`.

**Fix**: Added checks to prevent the idle task from being enqueued:
- In `schedule()`: Only enqueue `prev` if `prev->pid != 0`
- In `schedule_preempt()`: Only enqueue `prev` if `prev->pid != 0`

This ensures the idle task is never in the runqueue, maintaining the invariant that the idle task is a special fallback task that exists outside the normal scheduling queue.
