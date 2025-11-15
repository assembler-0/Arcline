#ifndef ARCLINE_MM_MMU_H
#define ARCLINE_MM_MMU_H

#include <stdint.h>

// ARM64 page table entry bits
#define PTE_VALID       (1ULL << 0)
#define PTE_TABLE       (1ULL << 1)  // for L0-L2
#define PTE_PAGE        (1ULL << 1)  // for L3
#define PTE_AF          (1ULL << 10) // Access flag
#define PTE_USER        (1ULL << 6)  // AP[1]
#define PTE_RO          (1ULL << 7)  // AP[2] - read-only
#define PTE_SH_INNER    (3ULL << 8)  // Inner shareable
#define PTE_ATTR_IDX(x) ((uint64_t)(x) << 2) // MAIR index
#define PTE_PXN         (1ULL << 53) // Privileged eXecute-Never
#define PTE_UXN         (1ULL << 54) // Unprivileged eXecute-Never

// Memory attribute indices (for MAIR_EL1)
#define MAIR_DEVICE_nGnRnE  0x00ULL
#define MAIR_NORMAL_NC      0x44ULL  // Normal, non-cacheable
#define MAIR_NORMAL         0xFFULL  // Normal, write-back cacheable

#define MAIR_IDX_DEVICE     0
#define MAIR_IDX_NORMAL_NC  1
#define MAIR_IDX_NORMAL     2

// Helper to convert PA to higher-half VA
#define PA_TO_VA(pa) ((pa) + VMM_KERNEL_VIRT_BASE)

// Page sizes
#define MMU_PAGE_SHIFT  12
#define MMU_PAGE_SIZE   (1ULL << MMU_PAGE_SHIFT)
#define MMU_PAGE_MASK   (MMU_PAGE_SIZE - 1)

// Initialize MMU with identity mapping for kernel
void mmu_init(void);

// Map a virtual address to physical with attributes
int mmu_map_page(uint64_t *pgd, uint64_t va, uint64_t pa, uint64_t attrs);

// Enable MMU (called from assembly or C after page tables ready)
void mmu_enable(void);

// Switch to higher-half kernel execution
void mmu_switch_to_higher_half(void);

// Get current page table base
uint64_t mmu_get_ttbr0(void);
uint64_t mmu_get_ttbr1(void);

// Map physical memory region to higher-half
int mmu_map_region(uint64_t pa, uint64_t size, uint64_t attrs);

// Unmap a page from page tables
int mmu_unmap_page(uint64_t *pgd, uint64_t va);

// Update page permissions
int mmu_update_page_attrs(uint64_t *pgd, uint64_t va, uint64_t attrs);

// TLB maintenance
void tlb_flush_all(void);
void tlb_flush_page(uint64_t va);
void tlb_flush_range(uint64_t va, uint64_t size);

// Cache maintenance
void cache_flush_range(uint64_t va, uint64_t size);
void icache_invalidate_range(uint64_t va, uint64_t size);

#endif // ARCLINE_MM_MMU_H
