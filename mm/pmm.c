// Physical Memory Manager (PMM) — simple bitmap allocator

#include <mm/pmm.h>
#include <dtb.h>
#include <kernel/printk.h>

#define PMM_MAX_PAGES 1048576ULL /* supports up to 4 GiB at 4 KiB pages */

static uint8_t pmm_bitmap[PMM_MAX_PAGES / 8];
static uint64_t pmm_mem_base = 0;   // Base of managed RAM region
static uint64_t pmm_mem_size = 0;   // Size of managed RAM region
static size_t   pmm_pages_total = 0;
static size_t   pmm_pages_free = 0;

// Symbols from linker/boot for reserved ranges
extern char _kernel_start[];
extern char _kernel_end[];
extern char stack_bottom[];
extern char _stack_top[];

static inline void set_bit(size_t idx)   { pmm_bitmap[idx >> 3] |=  (uint8_t)(1u << (idx & 7)); }
static inline void clear_bit(size_t idx) { pmm_bitmap[idx >> 3] &= (uint8_t)~(1u << (idx & 7)); }
static inline int  test_bit(size_t idx)  { return (pmm_bitmap[idx >> 3] >> (idx & 7)) & 1u; }

static inline size_t addr_to_page(uint64_t addr) {
    return (size_t)((addr - pmm_mem_base) / PMM_PAGE_SIZE);
}

static inline uint64_t page_to_addr(size_t page) {
    return pmm_mem_base + (uint64_t)page * PMM_PAGE_SIZE;
}

static void reserve_range(uint64_t start, uint64_t size) {
    if (size == 0) return;
    if (start + size <= pmm_mem_base) return;
    if (start >= pmm_mem_base + pmm_mem_size) return;

    uint64_t rstart = (start < pmm_mem_base) ? pmm_mem_base : start;
    uint64_t rend = start + size;
    uint64_t rlimit = pmm_mem_base + pmm_mem_size;
    if (rend > rlimit) rend = rlimit;

    size_t first = addr_to_page(rstart & ~(PMM_PAGE_SIZE - 1));
    size_t last = addr_to_page((rend + PMM_PAGE_SIZE - 1) & ~(PMM_PAGE_SIZE - 1));
    if (last > pmm_pages_total) last = pmm_pages_total;
    for (size_t i = first; i < last; ++i) {
        if (!test_bit(i)) {
            set_bit(i);
            if (pmm_pages_free) pmm_pages_free--;
        }
    }
}

// Minimal DTB parsing helpers for memory node and reg property
static inline uint32_t be32_to_cpu_u32(uint32_t v) {
    return ((v & 0xFFu) << 24) | ((v & 0xFF00u) << 8) | ((v & 0xFF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}

static int dtb_find_memory_region(uint64_t *base_out, uint64_t *size_out) {
    struct dtb_header *hdr = dtb_get();
    if (!hdr) return -1;
    if (be32_to_cpu_u32(hdr->magic) != 0xd00dfeed) return -1;

    const uint8_t *fdt = (const uint8_t *)hdr;
    uint32_t off_struct = be32_to_cpu_u32(hdr->off_dt_struct);
    uint32_t off_strings = be32_to_cpu_u32(hdr->off_dt_strings);
    const char *strings = (const char *)(fdt + off_strings);

    uint32_t p = off_struct;
    int depth = 0;
    int in_memory = 0;

    while (1) {
        uint32_t token = be32_to_cpu_u32(*(const uint32_t *)(fdt + p)); p += 4;
        if (token == DTB_BEGIN_NODE) {
            const char *name = (const char *)(fdt + p);
            size_t name_len = 0; while (name[name_len] != '\0') name_len++;
            p += (name_len + 4) & ~3u;
            depth++;
            // Memory nodes often named "memory" or have device_type=memory; we handle simple name match here.
            in_memory = (name_len == 6 && name[0]=='m'&&name[1]=='e'&&name[2]=='m'&&name[3]=='o'&&name[4]=='r'&&name[5]=='y');
        } else if (token == DTB_PROP) {
            uint32_t len = be32_to_cpu_u32(*(const uint32_t *)(fdt + p)); p += 4;
            uint32_t nameoff = be32_to_cpu_u32(*(const uint32_t *)(fdt + p)); p += 4;
            const char *pname = strings + nameoff;
            const uint8_t *pdata = fdt + p;
            if (in_memory && pname[0]=='r'&&pname[1]=='e'&&pname[2]=='g'&&pname[3]=='\0') {
                // Assume 64-bit address/size pairs if len >= 16; else 32-bit.
                uint64_t base = 0, size = 0;
                if (len >= 16) {
                    uint32_t hi = be32_to_cpu_u32(*(const uint32_t *)(pdata + 0));
                    uint32_t lo = be32_to_cpu_u32(*(const uint32_t *)(pdata + 4));
                    base = (((uint64_t)hi) << 32) | lo;
                    hi = be32_to_cpu_u32(*(const uint32_t *)(pdata + 8));
                    lo = be32_to_cpu_u32(*(const uint32_t *)(pdata + 12));
                    size = (((uint64_t)hi) << 32) | lo;
                } else if (len >= 8) {
                    base = be32_to_cpu_u32(*(const uint32_t *)(pdata + 0));
                    size = be32_to_cpu_u32(*(const uint32_t *)(pdata + 4));
                }
                if (base_out) *base_out = base;
                if (size_out) *size_out = size;
                return 0;
            }
            p += (len + 3) & ~3u;
        } else if (token == DTB_END_NODE) {
            if (depth > 0) depth--;
            in_memory = 0;
        } else if (token == DTB_NOP) {
            // skip
        } else if (token == DTB_END) {
            break;
        } else {
            break;
        }
    }
    return -1;
}

static void reserve_dtb_blob(void) {
    struct dtb_header *hdr = dtb_get();
    if (!hdr) return;
    uint64_t addr = dtb_ptr;
    uint64_t size = be32_to_cpu_u32(hdr->totalsize);
    reserve_range(addr, size);
}

void pmm_init_from_dtb(void) {
    // Clear bitmap to all allocated, will mark free later
    for (size_t i = 0; i < sizeof(pmm_bitmap); ++i) pmm_bitmap[i] = 0xFFu;

    uint64_t base = 0, size = 0;
    if (dtb_find_memory_region(&base, &size) != 0 || size == 0) {
        // Fallback: if DTB not available, assume 1 GiB at 0x40000000 (QEMU virt)
        base = 0x40000000ULL;
        size = 0x40000000ULL;
        printk("PMM: DTB memory not found, using fallback 1GiB@%p\n", (void*)base);
    }

    pmm_mem_base = base;
    pmm_mem_size = size;

    // Limit total pages by bitmap capacity
    pmm_pages_total = (size_t)(pmm_mem_size / PMM_PAGE_SIZE);
    if (pmm_pages_total > PMM_MAX_PAGES) pmm_pages_total = PMM_MAX_PAGES;

    // Mark all pages in managed range as free initially
    for (size_t i = 0; i < pmm_pages_total; ++i) {
        clear_bit(i);
    }
    pmm_pages_free = pmm_pages_total;

    // Reserve critical regions: kernel image, boot stack, DTB blob, and memory below base if misaligned
    reserve_range((uint64_t)_kernel_start, (uint64_t)(_kernel_end - _kernel_start));
    reserve_range((uint64_t)stack_bottom, (uint64_t)(_stack_top - stack_bottom));
    reserve_dtb_blob();

    // Optionally reserve the first 1 MiB of RAM for safety (firmware/BIOS style)
    if (pmm_mem_base < (pmm_mem_base + pmm_mem_size)) {
        reserve_range(pmm_mem_base, 0x100000ULL);
    }

    printk("PMM: managing %d pages (base=%p size=%p)\n",
           (int)pmm_pages_total, (void*)pmm_mem_base, (void*)pmm_mem_size);
}

static void* pmm_alloc_run(size_t count) {
    if (count == 0 || count > pmm_pages_free) return NULL;

    size_t run = 0;
    size_t run_start = 0;
    for (size_t i = 0; i < pmm_pages_total; ++i) {
        if (!test_bit(i)) {
            if (run == 0) run_start = i;
            run++;
            if (run == count) {
                // mark allocated
                for (size_t j = 0; j < count; ++j) set_bit(run_start + j);
                pmm_pages_free -= count;
                return (void*)(page_to_addr(run_start));
            }
        } else {
            run = 0;
        }
    }
    return NULL;
}

void* pmm_alloc_pages(size_t count) { return pmm_alloc_run(count); }

void* pmm_alloc_page(void) { return pmm_alloc_run(1); }

void pmm_free_pages(void* addr, size_t count) {
    if (!addr || count == 0) return;
    uint64_t a = (uint64_t)addr;
    if (a < pmm_mem_base || a >= pmm_mem_base + pmm_mem_size) return;
    if (a & (PMM_PAGE_SIZE - 1)) return; // must be page aligned
    size_t first = addr_to_page(a);
    for (size_t i = 0; i < count && (first + i) < pmm_pages_total; ++i) {
        if (test_bit(first + i)) {
            clear_bit(first + i);
            pmm_pages_free++;
        }
    }
}

void pmm_free_page(void* addr) { pmm_free_pages(addr, 1); }

size_t pmm_total_pages(void) { return pmm_pages_total; }
size_t pmm_free_pages_count(void) { return pmm_pages_free; }
