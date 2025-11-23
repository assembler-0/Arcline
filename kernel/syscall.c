#include <kernel/printk.h>
#include <kernel/sched/task.h>
#include <kernel/syscall.h>

static int sys_kill(int pid) {
    task_t *task = task_find_by_pid(pid);
    if (!task)
        return -1; // No such process

    return task_kill(task);
}

static int sys_exit(int code) {
    task_exit(code);
    return 0;
}

static int sys_write(int fd, const char *buf, int count) {
    (void)count;
    switch (fd) {
        case STDOUT_FD:
            return fprintk(STDOUT_FD, buf);
        case STDERR_FD:
            return fprintk(STDERR_FD, buf);
        default:
            return -1;

    }
}

static uint64_t (*syscall_table[])(uint64_t, uint64_t, uint64_t, uint64_t,
                                   uint64_t, uint64_t) = {
    [SYS_KILL] = (void *)sys_kill,
    [SYS_EXIT] = (void *)sys_exit,
    [SYS_WRITE] = (void *)sys_write,
};

uint64_t do_syscall(uint64_t syscall_num, uint64_t arg0, uint64_t arg1,
                    uint64_t arg2, uint64_t arg3, uint64_t arg4,
                    uint64_t arg5) {
    if (syscall_num >= sizeof(syscall_table) / sizeof(syscall_table[0])) {
        printk("Invalid syscall number: %d\n", (int)syscall_num);
        return -1;
    }

    if (!syscall_table[syscall_num]) {
        printk("Unimplemented syscall: %d\n", (int)syscall_num);
        return -1;
    }

    return syscall_table[syscall_num](arg0, arg1, arg2, arg3, arg4, arg5);
}
