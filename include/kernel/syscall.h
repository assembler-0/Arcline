#pragma once

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define SYS_KILL 129

uint64_t do_syscall(uint64_t syscall_num, uint64_t arg0, uint64_t arg1,
                    uint64_t arg2, uint64_t arg3, uint64_t arg4,
                    uint64_t arg5);
