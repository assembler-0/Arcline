#include <kernel/panic.h>
#include <kernel/pid.h>
#include <kernel/printk.h>
#include <kernel/sched/eevdf.h>
#include <kernel/sched/task.h>
#include <mm/mmu.h>
#include <mm/vmalloc.h>
#include <string.h>

static task_t *current_task = NULL;
static task_t *task_list = NULL;

extern void switch_to(cpu_context_t *prev, cpu_context_t *next);

static void idle_task_entry(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    // Idle task just waits for interrupts
    while (1) {
        __asm__ volatile("wfe");
    }
}

static void task_entry_wrapper(void) {
    task_t *task = task_current();
    void (*entry)(int, char **, char **) = (void *)task->context.x19;
    int argc = (int)task->context.x20;
    char **argv = (char **)task->context.x21;
    char **envp = (char **)task->context.x22;

    entry(argc, argv, envp);
    task_exit(0);
}

void task_init(void) {
    pid_init();
    eevdf_init();

    task_t *idle = task_create(idle_task_entry, 0, NULL);
    if (!idle)
        panic("Failed to create idle task");

    idle->pid = 0;
    // Remove idle from queue and set as current running task
    eevdf_dequeue(idle);
    idle->state = TASK_RUNNING;
    current_task = idle;

    printk("Task: idle task created (PID 0)\n");
    printk("EEVDF: scheduler initialized\n");
}

task_t *task_create(void (*entry)(int argc, char **argv, char **envp),
                    int priority, task_args *args) {
    task_t *task = (task_t *)vmalloc(sizeof(task_t));
    if (!task)
        return NULL;

    memset(task, 0, sizeof(task_t));

    task->pid = pid_alloc();
    if (task->pid < 0) {
        vfree(task, sizeof(task_t));
        return NULL;
    }
    task->state = TASK_READY;
    task->priority = (priority < EEVDF_MIN_NICE)   ? EEVDF_MIN_NICE
                     : (priority > EEVDF_MAX_NICE) ? EEVDF_MAX_NICE
                                                   : priority;
    task->time_slice = EEVDF_TIME_SLICE_NS;
    task->vruntime = 0;

    task->kernel_stack = vmalloc(KERNEL_STACK_SIZE);
    if (!task->kernel_stack) {
        pid_free(task->pid);
        vfree(task, sizeof(task_t));
        return NULL;
    }

    task->pgd = (uint64_t *)mmu_get_ttbr1();

    if (entry) {
        uint64_t stack_top = (uint64_t)task->kernel_stack + KERNEL_STACK_SIZE;
        stack_top &= ~15ULL;

        memset(&task->context, 0, sizeof(cpu_context_t));
        task->context.sp = stack_top;
        task->context.pc = (uint64_t)task_entry_wrapper;
        task->context.x19 = (uint64_t)entry;
        task->context.x20 = args ? args->argc : 0;
        task->context.x21 = args ? (uint64_t)args->argv : 0;
        task->context.x22 = args ? (uint64_t)args->envp : 0;
        task->context.x23 = 0;
        task->context.x29 = 0;
        task->context.x30 = (uint64_t)task_exit;
        task->context.pstate = 0x345;

        eevdf_enqueue(task);
    }

    task->next = task_list;
    if (task_list)
        task_list->prev = task;
    task_list = task;

    return task;
}

void task_exit(int code) {
    (void)code;
    if (!current_task)
        return;

    // Task is RUNNING, not in queue - don't dequeue
    current_task->state = TASK_ZOMBIE;
    pid_free(current_task->pid);

    // Clear current_task before scheduling
    // schedule() will not return - it will switch to another task
    task_set_current(NULL);
    
    extern void schedule(void);
    schedule();
    
    // This point should never be reached
    __builtin_unreachable();
}

task_t *task_current(void) { return current_task; }

void task_set_current(task_t *task) { current_task = task; }

task_t *task_find_by_pid(int pid) {
    task_t *p = task_list;
    while (p) {
        if (p->pid == pid)
            return p;
        p = p->next;
    }
    return NULL;
}

int task_kill(task_t *task) {
    if (!task)
        return -1;

    // Cannot kill the idle task
    if (task->pid == 0)
        return -1;

    // Idempotent: if task is already zombie, return success without re-processing
    if (task->state == TASK_ZOMBIE)
        return 0;

    printk("[KILL] Killing PID %d (state=%d)\n", task->pid, task->state);

    // Remove from scheduler queue ONLY if it's in the queue (READY state)
    if (task->state == TASK_READY) {
        eevdf_dequeue(task);
    }

    task->state = TASK_ZOMBIE;
    pid_free(task->pid);

    // Remove from task list
    if (task->prev)
        task->prev->next = task->next;
    if (task->next)
        task->next->prev = task->prev;
    if (task == task_list)
        task_list = task->next;

    // a more robust implementation would free the stack and other resources,
    // but for now, we just make it a zombie.

    // If killing current task, reschedule immediately
    if (task == task_current()) {
        // Clear current_task before scheduling
        // schedule() will not return - it will switch to another task
        task_set_current(NULL);
        
        extern void schedule(void);
        schedule();
        
        // This point should never be reached
        __builtin_unreachable();
    }

    return 0;
}
