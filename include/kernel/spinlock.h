#ifndef ARCLINE_SPINLOCK_H
#define ARCLINE_SPINLOCK_H

#include <stdint.h>

typedef struct {
    volatile uint32_t lock;
} spinlock_t;

#define SPINLOCK_INIT {0}

static inline void spinlock_init(spinlock_t *lock) {
    lock->lock = 0;
}

static inline void spinlock_lock(spinlock_t *lock) {
    uint32_t tmp;
    __asm__ volatile(
        "1: ldaxr %w0, [%1]\n"
        "   cbnz %w0, 1b\n"
        "   stxr %w0, %w2, [%1]\n"
        "   cbnz %w0, 1b\n"
        : "=&r"(tmp)
        : "r"(&lock->lock), "r"(1)
        : "memory"
    );
}

static inline void spinlock_unlock(spinlock_t *lock) {
    __asm__ volatile(
        "stlr %w1, [%0]\n"
        :
        : "r"(&lock->lock), "r"(0)
        : "memory"
    );
}

static inline uint64_t spinlock_lock_irqsave(spinlock_t *lock) {
    uint64_t flags;
    __asm__ volatile("mrs %0, daif" : "=r"(flags));
    __asm__ volatile("msr daifset, #2" ::: "memory");
    spinlock_lock(lock);
    return flags;
}

static inline void spinlock_unlock_irqrestore(spinlock_t *lock, uint64_t flags) {
    spinlock_unlock(lock);
    __asm__ volatile("msr daif, %0" :: "r"(flags) : "memory");
}

#endif
