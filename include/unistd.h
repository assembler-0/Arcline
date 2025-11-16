#pragma once

#include <stdint.h>

static inline uint64_t syscall(uint64_t syscall_num, uint64_t arg0,
                               uint64_t arg1, uint64_t arg2, uint64_t arg3,
                               uint64_t arg4, uint64_t arg5) {
    register uint64_t x8 __asm__("x8") = syscall_num;
    register uint64_t x0 __asm__("x0") = arg0;
    register uint64_t x1 __asm__("x1") = arg1;
    register uint64_t x2 __asm__("x2") = arg2;
    register uint64_t x3 __asm__("x3") = arg3;
    register uint64_t x4 __asm__("x4") = arg4;
    register uint64_t x5 __asm__("x5") = arg5;

    __asm__ volatile("svc #0"
                     : "=r"(x0)
                     : "r"(x8), "r"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4),
                       "r"(x5)
                     : "memory");

    return x0;
}
