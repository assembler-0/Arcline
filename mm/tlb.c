// TLB and cache maintenance operations for ARM64

#include <mm/mmu.h>
#include <stdint.h>

void tlb_flush_all(void) {
    __asm__ volatile(
        "dsb ishst\n"
        "tlbi vmalle1is\n"
        "dsb ish\n"
        "isb\n"
        ::: "memory"
    );
}

void tlb_flush_page(uint64_t va) {
    __asm__ volatile(
        "dsb ishst\n"
        "lsr x0, %0, #12\n"
        "tlbi vaae1is, x0\n"
        "dsb ish\n"
        "isb\n"
        :: "r"(va) : "x0", "memory"
    );
}

void tlb_flush_range(uint64_t va, uint64_t size) {
    uint64_t end = va + size;
    for (uint64_t addr = va; addr < end; addr += MMU_PAGE_SIZE) {
        __asm__ volatile(
            "lsr x0, %0, #12\n"
            "tlbi vaae1is, x0\n"
            :: "r"(addr) : "x0"
        );
    }
    __asm__ volatile("dsb ish\nisb\n" ::: "memory");
}

void cache_flush_range(uint64_t va, uint64_t size) {
    uint64_t line_size = 64;
    uint64_t start = va & ~(line_size - 1);
    uint64_t end = va + size;
    
    for (uint64_t addr = start; addr < end; addr += line_size) {
        __asm__ volatile("dc civac, %0" :: "r"(addr) : "memory");
    }
    __asm__ volatile("dsb ish\n" ::: "memory");
}

void icache_invalidate_range(uint64_t va, uint64_t size) {
    uint64_t line_size = 64;
    uint64_t start = va & ~(line_size - 1);
    uint64_t end = va + size;
    
    for (uint64_t addr = start; addr < end; addr += line_size) {
        __asm__ volatile("ic ivau, %0" :: "r"(addr));
    }
    __asm__ volatile("dsb ish\nisb\n" ::: "memory");
}
