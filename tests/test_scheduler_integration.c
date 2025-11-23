#include <kernel/printk.h>
#include <kernel/sched/task.h>
#include <kernel/syscall.h>
#include <drivers/timer.h>
#include <string.h>

// Test 1: Timer Preemption Test
// Creates two tasks that increment counters and verifies both make progress

static volatile int counter_a = 0;
static volatile int counter_b = 0;
static volatile int test_complete = 0;

static void task_a_entry(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    
    printk("[TEST] Task A started (PID %d)\n", task_current()->pid);
    
    // Increment counter many times
    for (int i = 0; i < 1000; i++) {
        counter_a++;
        // Small delay to allow preemption
        for (volatile int j = 0; j < 100; j++);
    }
    
    printk("[TEST] Task A completed: counter_a=%d\n", counter_a);
}

static void task_b_entry(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    
    printk("[TEST] Task B started (PID %d)\n", task_current()->pid);
    
    // Increment counter many times
    for (int i = 0; i < 1000; i++) {
        counter_b++;
        // Small delay to allow preemption
        for (volatile int j = 0; j < 100; j++);
    }
    
    printk("[TEST] Task B completed: counter_b=%d\n", counter_b);
    test_complete = 1;
}

void test_timer_preemption(void) {
    printk("\n=== TEST: Timer Preemption ===\n");
    
    counter_a = 0;
    counter_b = 0;
    test_complete = 0;
    
    // Create two tasks
    task_t *task_a = task_create(task_a_entry, 0, NULL);
    task_t *task_b = task_create(task_b_entry, 0, NULL);
    
    if (!task_a || !task_b) {
        printk("[TEST] FAILED: Could not create tasks\n");
        return;
    }
    
    int pid_a = task_a->pid;
    int pid_b = task_b->pid;
    printk("[TEST] Created tasks: A (PID %d), B (PID %d)\n", pid_a, pid_b);
    
    // Wait for tasks to complete
    // Check if both tasks have become zombies
    extern void schedule(void);
    int wait_iterations = 0;
    while (wait_iterations < 1000) {
        wait_iterations++;
        
        // Check task states
        task_t *ta = task_find_by_pid(pid_a);
        task_t *tb = task_find_by_pid(pid_b);
        
        if (ta && tb && ta->state == TASK_ZOMBIE && tb->state == TASK_ZOMBIE) {
            break;
        }
        
        // Yield to scheduler to allow other tasks to run
        schedule();
        for (volatile int i = 0; i < 1000; i++);
    }
    
    printk("[TEST] After %d iterations:\n", wait_iterations);
    printk("[TEST]   counter_a = %d\n", counter_a);
    printk("[TEST]   counter_b = %d\n", counter_b);
    
    // Verify both tasks made progress (preemption occurred)
    if (counter_a > 0 && counter_b > 0) {
        printk("[TEST] PASSED: Both tasks made progress (preemption working)\n");
    } else {
        printk("[TEST] FAILED: counter_a=%d, counter_b=%d (no preemption?)\n", 
               counter_a, counter_b);
    }
    
    printk("=== END TEST ===\n\n");
}


// Test 2: Task Termination Test
// Creates a task that exits and verifies it becomes zombie and doesn't run again

static volatile int termination_counter = 0;

static void terminating_task_entry(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    
    int pid = task_current()->pid;
    printk("[TEST] Terminating task started (PID %d)\n", pid);
    
    // Increment counter a few times
    for (int i = 0; i < 5; i++) {
        termination_counter++;
        printk("[TEST] Terminating task: counter=%d\n", termination_counter);
    }
    
    printk("[TEST] Terminating task calling sys_exit (PID %d)\n", pid);
    // Exit will be called by task_exit wrapper
}

void test_task_termination(void) {
    printk("\n=== TEST: Task Termination ===\n");
    
    termination_counter = 0;
    
    // Create a task that will exit
    task_t *task = task_create(terminating_task_entry, 0, NULL);
    
    if (!task) {
        printk("[TEST] FAILED: Could not create task\n");
        return;
    }
    
    int pid = task->pid;
    printk("[TEST] Created terminating task (PID %d)\n", pid);
    
    // Wait for task to complete
    extern void schedule(void);
    for (int i = 0; i < 100; i++) {
        schedule();
        for (volatile int j = 0; j < 10000; j++);
    }
    
    // Check task state
    task_t *found = task_find_by_pid(pid);
    if (found) {
        printk("[TEST] Task state: %d (0=RUNNING, 1=READY, 2=BLOCKED, 3=ZOMBIE)\n", 
               found->state);
        
        if (found->state == TASK_ZOMBIE) {
            printk("[TEST] PASSED: Task became zombie\n");
        } else {
            printk("[TEST] FAILED: Task state is %d, expected ZOMBIE (3)\n", 
                   found->state);
        }
    } else {
        printk("[TEST] Task not found (may have been cleaned up)\n");
    }
    
    // Verify counter didn't increase beyond expected
    printk("[TEST] Final counter value: %d (expected 5)\n", termination_counter);
    
    if (termination_counter == 5) {
        printk("[TEST] PASSED: Task didn't run after exit\n");
    } else {
        printk("[TEST] WARNING: Counter is %d, expected 5\n", termination_counter);
    }
    
    printk("=== END TEST ===\n\n");
}


// Test 3: Killing Current Task Test
// Creates a task that kills itself and verifies system continues running

static volatile int kill_self_counter = 0;
static volatile int other_task_counter = 0;

static void kill_self_task_entry(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    
    int pid = task_current()->pid;
    printk("[TEST] Kill-self task started (PID %d)\n", pid);
    
    // Increment counter a few times
    for (int i = 0; i < 3; i++) {
        kill_self_counter++;
        printk("[TEST] Kill-self task: counter=%d\n", kill_self_counter);
    }
    
    printk("[TEST] Kill-self task calling sys_kill on itself (PID %d)\n", pid);
    
    // Kill ourselves via syscall
    __asm__ volatile(
        "mov x8, %0\n"      // SYS_KILL
        "mov x0, %1\n"      // pid
        "svc #0\n"
        :
        : "i"(SYS_KILL), "r"((uint64_t)pid)
        : "x8", "x0"
    );
    
    // Should never reach here
    printk("[TEST] ERROR: Kill-self task still running after sys_kill!\n");
    kill_self_counter = 9999;
}

static void other_task_entry(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    
    printk("[TEST] Other task started (PID %d)\n", task_current()->pid);
    
    // Run for a while to verify system continues
    for (int i = 0; i < 10; i++) {
        other_task_counter++;
        for (volatile int j = 0; j < 100000; j++);
    }
    
    printk("[TEST] Other task completed: counter=%d\n", other_task_counter);
}

void test_killing_current_task(void) {
    printk("\n=== TEST: Killing Current Task ===\n");
    
    kill_self_counter = 0;
    other_task_counter = 0;
    
    // Create task that will kill itself
    task_t *kill_task = task_create(kill_self_task_entry, 0, NULL);
    // Create another task to verify system continues
    task_t *other_task = task_create(other_task_entry, 0, NULL);
    
    if (!kill_task || !other_task) {
        printk("[TEST] FAILED: Could not create tasks\n");
        return;
    }
    
    int kill_pid = kill_task->pid;
    printk("[TEST] Created tasks: Kill-self (PID %d), Other (PID %d)\n", 
           kill_pid, other_task->pid);
    
    // Wait for tasks to complete
    extern void schedule(void);
    for (int i = 0; i < 200; i++) {
        schedule();
        for (volatile int j = 0; j < 10000; j++);
    }
    
    printk("[TEST] After execution:\n");
    printk("[TEST]   kill_self_counter = %d\n", kill_self_counter);
    printk("[TEST]   other_task_counter = %d\n", other_task_counter);
    
    // Verify kill-self task stopped at expected count
    if (kill_self_counter == 3) {
        printk("[TEST] PASSED: Kill-self task stopped after sys_kill\n");
    } else {
        printk("[TEST] FAILED: kill_self_counter=%d, expected 3\n", kill_self_counter);
    }
    
    // Verify other task continued running
    if (other_task_counter > 0) {
        printk("[TEST] PASSED: Other task continued running\n");
    } else {
        printk("[TEST] FAILED: Other task didn't run (system hung?)\n");
    }
    
    // Check task state
    task_t *found = task_find_by_pid(kill_pid);
    if (found && found->state == TASK_ZOMBIE) {
        printk("[TEST] PASSED: Kill-self task is zombie\n");
    } else if (found) {
        printk("[TEST] FAILED: Kill-self task state is %d, expected ZOMBIE (3)\n", 
               found->state);
    } else {
        printk("[TEST] Kill-self task not found (may have been cleaned up)\n");
    }
    
    printk("=== END TEST ===\n\n");
}


// Test 4: Stress Test
// Creates many tasks and kills them in various orders

#define STRESS_TEST_TASKS 10

static volatile int stress_counters[STRESS_TEST_TASKS];

static void stress_task_entry(int argc, char **argv, char **envp) {
    (void)argv;
    (void)envp;
    
    int task_id = argc;  // Use argc to pass task ID
    int pid = task_current()->pid;
    
    printk("[TEST] Stress task %d started (PID %d)\n", task_id, pid);
    
    // Do some work
    for (int i = 0; i < 100; i++) {
        stress_counters[task_id]++;
        for (volatile int j = 0; j < 1000; j++);
    }
    
    printk("[TEST] Stress task %d completed (PID %d): counter=%d\n", 
           task_id, pid, stress_counters[task_id]);
}

void test_stress(void) {
    printk("\n=== TEST: Stress Test ===\n");
    
    // Initialize counters
    for (int i = 0; i < STRESS_TEST_TASKS; i++) {
        stress_counters[i] = 0;
    }
    
    // Create many tasks
    task_t *tasks[STRESS_TEST_TASKS];
    int pids[STRESS_TEST_TASKS];
    
    printk("[TEST] Creating %d tasks...\n", STRESS_TEST_TASKS);
    
    for (int i = 0; i < STRESS_TEST_TASKS; i++) {
        task_args args = { .argc = i, .argv = NULL, .envp = NULL };
        tasks[i] = task_create(stress_task_entry, 0, &args);
        
        if (!tasks[i]) {
            printk("[TEST] FAILED: Could not create task %d\n", i);
            return;
        }
        
        pids[i] = tasks[i]->pid;
        printk("[TEST] Created task %d (PID %d)\n", i, pids[i]);
    }
    
    // Let tasks run for a bit
    printk("[TEST] Letting tasks run...\n");
    
    // Yield to scheduler to let tasks run
    extern void schedule(void);
    for (int i = 0; i < 100; i++) {
        schedule();
        for (volatile int j = 0; j < 10000; j++);
    }
    
    // Kill tasks in various orders (every other task first)
    printk("[TEST] Killing every other task...\n");
    for (int i = 0; i < STRESS_TEST_TASKS; i += 2) {
        task_t *task = task_find_by_pid(pids[i]);
        if (task) {
            printk("[TEST] Killing task %d (PID %d)\n", i, pids[i]);
            task_kill(task);
        }
    }
    
    // Let remaining tasks run
    for (int i = 0; i < 100; i++) {
        schedule();
        for (volatile int j = 0; j < 10000; j++);
    }
    
    // Kill remaining tasks
    printk("[TEST] Killing remaining tasks...\n");
    for (int i = 1; i < STRESS_TEST_TASKS; i += 2) {
        task_t *task = task_find_by_pid(pids[i]);
        if (task) {
            printk("[TEST] Killing task %d (PID %d)\n", i, pids[i]);
            task_kill(task);
        }
    }
    
    // Wait a bit more
    for (int i = 0; i < 100; i++) {
        schedule();
        for (volatile int j = 0; j < 10000; j++);
    }
    
    // Check results
    printk("[TEST] Final counter values:\n");
    int all_made_progress = 1;
    for (int i = 0; i < STRESS_TEST_TASKS; i++) {
        printk("[TEST]   Task %d: counter=%d\n", i, stress_counters[i]);
        if (stress_counters[i] == 0) {
            all_made_progress = 0;
        }
    }
    
    if (all_made_progress) {
        printk("[TEST] PASSED: All tasks made progress\n");
    } else {
        printk("[TEST] WARNING: Some tasks didn't make progress\n");
    }
    
    // Verify all tasks are zombies
    int all_zombies = 1;
    for (int i = 0; i < STRESS_TEST_TASKS; i++) {
        task_t *task = task_find_by_pid(pids[i]);
        if (task && task->state != TASK_ZOMBIE) {
            printk("[TEST] Task %d (PID %d) is not zombie (state=%d)\n", 
                   i, pids[i], task->state);
            all_zombies = 0;
        }
    }
    
    if (all_zombies) {
        printk("[TEST] PASSED: All tasks are zombies\n");
    } else {
        printk("[TEST] FAILED: Some tasks are not zombies\n");
    }
    
    printk("[TEST] PASSED: No hangs or crashes detected\n");
    printk("=== END TEST ===\n\n");
}


// Main test runner
void run_scheduler_integration_tests(void) {
    printk("\n");
    printk("========================================\n");
    printk("  SCHEDULER INTEGRATION TESTS\n");
    printk("========================================\n");
    printk("\n");
    
    test_timer_preemption();
    test_task_termination();
    test_killing_current_task();
    test_stress();
    
    printk("\n");
    printk("========================================\n");
    printk("  ALL TESTS COMPLETED\n");
    printk("========================================\n");
    printk("\n");
    
    // Verify we're back in idle task
    task_t *current = task_current();
    if (current) {
        printk("[TEST] Current task: PID %d (should be 0 for idle)\n", current->pid);
        if (current->pid != 0) {
            printk("[TEST] WARNING: Not in idle task! Forcing schedule...\n");
            extern void schedule(void);
            schedule();
        }
    } else {
        printk("[TEST] WARNING: current task is NULL!\n");
    }
    
    printk("[TEST] Returning to caller (should be idle task)...\n");
}
