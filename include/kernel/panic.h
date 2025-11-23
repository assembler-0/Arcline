#ifndef ARCLINE_KERNEL_PANIC_H
#define ARCLINE_KERNEL_PANIC_H

void __panic(const char *file, int line, const char *func, const char *fmt, ...)
    __attribute__((noreturn));

#define panic(fmt, ...)                                                        \
    __panic(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define BUG() panic("BUG at %s:%d", __FILE__, __LINE__)
#define BUG_ON(cond)                                                           \
    do {                                                                       \
        if (cond)                                                              \
            panic("BUG_ON(%s)", #cond);                                        \
    } while (0)

#endif // ARCLINE_KERNEL_PANIC_H
