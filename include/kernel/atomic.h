#ifndef ARCLINE_ATOMIC_H
#define ARCLINE_ATOMIC_H

#include <stdint.h>

static inline uint32_t atomic_read(volatile uint32_t *ptr) {
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

static inline void atomic_write(volatile uint32_t *ptr, uint32_t val) {
    __atomic_store_n(ptr, val, __ATOMIC_RELEASE);
}

static inline uint32_t atomic_inc(volatile uint32_t *ptr) {
    return __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST);
}

static inline uint32_t atomic_dec(volatile uint32_t *ptr) {
    return __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST);
}

static inline uint32_t atomic_cmpxchg(volatile uint32_t *ptr, uint32_t old, uint32_t new) {
    __atomic_compare_exchange_n(ptr, &old, new, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old;
}

static inline uint64_t atomic_read64(volatile uint64_t *ptr) {
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

static inline void atomic_write64(volatile uint64_t *ptr, uint64_t val) {
    __atomic_store_n(ptr, val, __ATOMIC_RELEASE);
}

static inline uint64_t atomic_inc64(volatile uint64_t *ptr) {
    return __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST);
}

static inline uint64_t atomic_fetch_and64(volatile uint64_t *ptr, uint64_t val) {
    return __atomic_fetch_and(ptr, val, __ATOMIC_SEQ_CST);
}

static inline uint64_t atomic_fetch_or64(volatile uint64_t *ptr, uint64_t val) {
    return __atomic_fetch_or(ptr, val, __ATOMIC_SEQ_CST);
}

#endif
