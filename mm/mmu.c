// ARM64 MMU initialization and page table management

#include <mm/mmu.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <kernel/printk.h>
#include <string.h>

// 4-level page tables: L0 (PGD) -> L1 (PUD) -> L2 (PMD) -> L3 (PTE)
// 48-bit VA: [47:39]=L0, [38:30]=L1, [29:21]=L2, [20:12]=L3, [11:0]=offset

static uint64_t *ttbr0_pgd = NULL; // Identity mapping
static uint64_t *ttbr1_pgd = NULL; // Higher-half kernel

#define PGD_SHIFT 39
#define PUD_SHIFT 30
#define PMD_SHIFT 21
#define PTE_SHIFT 12

#define TABLE_ENTRIES 512

static inline uint64_t* alloc_table(void) {
    void *p = pmm_alloc_page();
    if (p) memset(p, 0, MMU_PAGE_SIZE);
    return (uint64_t*)p;
}

static inline int pgd_index(uint64_t va) { return (va >> PGD_SHIFT) & 0x1FF; }
static inline int pud_index(uint64_t va) { return (va >> PUD_SHIFT) & 0x1FF; }
static inline int pmd_index(uint64_t va) { return (va >> PMD_SHIFT) & 0x1FF; }
static inline int pte_index(uint64_t va) { return (va >> PTE_SHIFT) & 0x1FF; }

int mmu_map_page(uint64_t *pgd, uint64_t va, uint64_t pa, uint64_t attrs) {
    int idx;
    uint64_t *pud, *pmd, *pte;

    // L0 -> L1
    idx = pgd_index(va);
    if (!(pgd[idx] & PTE_VALID)) {
        pud = alloc_table();
        if (!pud) return -1;
        pgd[idx] = ((uint64_t)pud) | PTE_TABLE | PTE_VALID;
    }
    pud = (uint64_t*)(pgd[idx] & ~0xFFFULL);

    // L1 -> L2
    idx = pud_index(va);
    if (!(pud[idx] & PTE_VALID)) {
        pmd = alloc_table();
        if (!pmd) return -1;
        pud[idx] = ((uint64_t)pmd) | PTE_TABLE | PTE_VALID;
    }
    pmd = (uint64_t*)(pud[idx] & ~0xFFFULL);

    // L2 -> L3
    idx = pmd_index(va);
    if (!(pmd[idx] & PTE_VALID)) {
        pte = alloc_table();
        if (!pte) return -1;
        pmd[idx] = ((uint64_t)pte) | PTE_TABLE | PTE_VALID;
    }
    pte = (uint64_t*)(pmd[idx] & ~0xFFFULL);

    // L3 entry
    idx = pte_index(va);
    pte[idx] = (pa & ~MMU_PAGE_MASK) | attrs | PTE_AF | PTE_VALID;
    return 0;
}

void mmu_init(void) {
    extern char _kernel_start[], _kernel_end[];
    
    ttbr0_pgd = alloc_table();
    ttbr1_pgd = alloc_table();
    if (!ttbr0_pgd || !ttbr1_pgd) {
        printk("MMU: failed to allocate PGD\n");
        return;
    }

    uint64_t kstart = (uint64_t)_kernel_start & ~MMU_PAGE_MASK;
    uint64_t kend = ((uint64_t)_kernel_end + MMU_PAGE_MASK) & ~MMU_PAGE_MASK;
    uint64_t attrs = PTE_PAGE | PTE_SH_INNER | PTE_ATTR_IDX(MAIR_IDX_NORMAL);
    
    // TTBR0: Identity map first 2GB
    for (uint64_t pa = 0; pa < 0x80000000ULL; pa += MMU_PAGE_SIZE) {
        if (mmu_map_page(ttbr0_pgd, pa, pa, attrs) < 0) break;
    }

    // TTBR1: Map kernel to higher-half
    uint64_t virt_base = vmm_kernel_base();
    for (uint64_t pa = kstart; pa < kend; pa += MMU_PAGE_SIZE) {
        uint64_t va = virt_base + (pa - kstart);
        if (mmu_map_page(ttbr1_pgd, va, pa, attrs) < 0) {
            printk("MMU: failed to map kernel page %p\n", (void*)pa);
            return;
        }
    }

    printk("MMU: TTBR0=%p TTBR1=%p\n", ttbr0_pgd, ttbr1_pgd);
    printk("MMU: kernel mapped %p-%p -> %p-%p\n", 
           (void*)virt_base, (void*)(virt_base + (kend - kstart)),
           (void*)kstart, (void*)kend);
}

void mmu_enable(void) {
    uint64_t mair = (MAIR_DEVICE_nGnRnE << (8 * MAIR_IDX_DEVICE)) |
                    (MAIR_NORMAL_NC << (8 * MAIR_IDX_NORMAL_NC)) |
                    (MAIR_NORMAL << (8 * MAIR_IDX_NORMAL));
    
    uint64_t tcr = (16ULL << 0) |  // T0SZ = 16 (48-bit)
                   (16ULL << 16) | // T1SZ = 16
                   (0ULL << 14) |  // TG0 = 4KB
                   (2ULL << 30);   // TG1 = 4KB

    __asm__ volatile(
        "msr mair_el1, %0\n"
        "msr tcr_el1, %1\n"
        "msr ttbr0_el1, %2\n"
        "msr ttbr1_el1, %3\n"
        "isb\n"
        :: "r"(mair), "r"(tcr), "r"(ttbr0_pgd), "r"(ttbr1_pgd)
    );

    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1 << 0) | (1 << 2) | (1 << 12);
    __asm__ volatile(
        "msr sctlr_el1, %0\n"
        "isb\n"
        :: "r"(sctlr)
    );

    printk("MMU: enabled\n");
}

uint64_t mmu_get_ttbr0(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(val));
    return val;
}

uint64_t mmu_get_ttbr1(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, ttbr1_el1" : "=r"(val));
    return val;
}

void mmu_switch_to_higher_half(void) {
    extern char _kernel_start[], _stack_top[];
    uint64_t offset = vmm_kernel_base() - (uint64_t)_kernel_start;
    
    __asm__ volatile(
        "adr x0, 1f\n"
        "add x0, x0, %0\n"
        "br x0\n"
        "1:\n"
        "mov x1, sp\n"
        "add x1, x1, %0\n"
        "mov sp, x1\n"
        :: "r"(offset) : "x0", "x1"
    );
    
    printk("MMU: switched to higher-half\n");
}

int mmu_map_region(uint64_t pa, uint64_t size, uint64_t attrs) {
    if (!ttbr1_pgd) return -1;
    
    uint64_t pa_aligned = pa & ~MMU_PAGE_MASK;
    uint64_t size_aligned = (size + MMU_PAGE_MASK) & ~MMU_PAGE_MASK;
    uint64_t va_base = vmm_kernel_base();
    
    for (uint64_t off = 0; off < size_aligned; off += MMU_PAGE_SIZE) {
        uint64_t va = va_base + pa_aligned + off;
        if (mmu_map_page(ttbr1_pgd, va, pa_aligned + off, attrs) < 0) {
            return -1;
        }
    }
    return 0;
}
