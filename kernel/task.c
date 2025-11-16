#include <kernel/task.h>
#include <kernel/printk.h>
#include <kernel/panic.h>
#include <mm/vmalloc.h>
#include <mm/mmu.h>
#include <string.h>

static task_t *current_task = NULL;
static task_t *task_list = NULL;
static int next_pid = 1;

extern void switch_to(cpu_context_t *prev, cpu_context_t *next);

static void task_entry_wrapper(void) {
    task_t *task = task_current();
    void (*entry)(int, char**, char**) = (void*)task->context.x19;
    int argc = (int)task->context.x20;
    char **argv = (char**)task->context.x21;
    char **envp = (char**)task->context.x22;
    
    entry(argc, argv, envp);
    task_exit(0);
}

void task_init(void) {
    task_t *idle = task_create(NULL, 0, NULL);
    if (!idle) panic("Failed to create idle task");
    
    idle->pid = 0;
    idle->state = TASK_RUNNING;
    current_task = idle;
    
    printk("Task: idle task created (PID 0)\n");
}

task_t* task_create(void (*entry)(int argc, char** argv, char** envp),
        int priority,
        task_args* args
    ) {
    task_t *task = (task_t*)vmalloc(sizeof(task_t));
    if (!task) return NULL;
    
    memset(task, 0, sizeof(task_t));
    
    task->pid = next_pid++;
    task->state = TASK_READY;
    task->priority = priority;
    task->time_slice = 100;
    task->vruntime = 0;
    
    task->kernel_stack = vmalloc(KERNEL_STACK_SIZE);
    if (!task->kernel_stack) {
        vfree(task, sizeof(task_t));
        return NULL;
    }
    
    task->pgd = (uint64_t*)mmu_get_ttbr1();
    
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
        task->context.x29 = 0;
        task->context.x30 = (uint64_t)task_exit;
        task->context.pstate = 0x3C5;
    }
    
    task->next = task_list;
    if (task_list) task_list->prev = task;
    task_list = task;
    
    return task;
}

void task_exit(int code) {
    (void)code;
    if (!current_task) return;
    
    current_task->state = TASK_ZOMBIE;
    
    extern void schedule(void);
    schedule();
}

task_t* task_current(void) {
    return current_task;
}

void task_set_current(task_t *task) {
    current_task = task;
}

task_t* task_get_next_ready(void) {
    task_t *t = task_list;
    while (t) {
        if (t->state == TASK_READY) return t;
        t = t->next;
    }
    return NULL;
}
