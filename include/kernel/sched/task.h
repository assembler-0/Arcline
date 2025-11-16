#ifndef ARCLINE_KERNEL_TASK_H
#define ARCLINE_KERNEL_TASK_H

#include <stdint.h>

#define TASK_RUNNING     0
#define TASK_READY       1
#define TASK_BLOCKED     2
#define TASK_ZOMBIE      3

#define KERNEL_STACK_SIZE 16384

typedef struct cpu_context {
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7;
    uint64_t x8, x9, x10, x11, x12, x13, x14, x15;
    uint64_t x16, x17, x18, x19, x20, x21, x22, x23;
    uint64_t x24, x25, x26, x27, x28, x29, x30;
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
} cpu_context_t;

typedef struct task task_t;

typedef struct {
    int argc;
    char **argv;
    char **envp;
} task_args;

struct task {
    int pid;
    int state;
    int priority;
    uint64_t time_slice;
    uint64_t vruntime;
    
    cpu_context_t context;
    void *kernel_stack;
    uint64_t *pgd;
    
    task_t *next;
    task_t *prev;
};

void task_init(void);
task_t* task_create(void (*entry)(int argc, char** argv, char** envp),
    int priority,
    task_args* args
);
void task_exit(int code);
task_t* task_current(void);
void task_set_current(task_t *task);
void schedule(void);
void schedule_preempt(cpu_context_t *regs);

#endif // ARCLINE_KERNEL_TASK_H
