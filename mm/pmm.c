// Physical Memory Manager (PMM) — simple bitmap allocator

#include <mm/pmm.h>
#include <dtb.h>
#include <kernel/printk.h>
#include <string.h>

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
    int in_memory_node = 0;   // true when current node is a memory node
    int device_type_memory = 0; // true if device_type=="memory" seen for current node
    // Read #address-cells and #size-cells from the parent (root) by default.
    int parent_addr_cells = 2; // default for 64-bit
    int parent_size_cells = 2; // default for 64-bit

    while (1) {
        uint32_t token = be32_to_cpu_u32(*(const uint32_t *)(fdt + p)); p += 4;
        if (token == DTB_BEGIN_NODE) {
            const char *name = (const char *)(fdt + p);
            size_t name_len = 0; while (name[name_len] != '\0') name_len++;
            p += (name_len + 4) & ~3u;
            depth++;
            // Memory nodes are typically named "memory" or "memory@..."
            in_memory_node = (name_len >= 6 && strncmp(name, "memory", 6) == 0);
            device_type_memory = 0; // reset when entering a node
        } else if (token == DTB_PROP) {
            uint32_t len = be32_to_cpu_u32(*(const uint32_t *)(fdt + p)); p += 4;
            uint32_t nameoff = be32_to_cpu_u32(*(const uint32_t *)(fdt + p)); p += 4;
            const char *pname = strings + nameoff;
            const uint8_t *pdata = fdt + p;
            // Track device_type property
            if (in_memory_node && pname[0]=='d'&&
                pname[1]=='e'&&
                pname[2]=='v'&&
                pname[3]=='i'&&
                pname[4]=='c'&&
                pname[5]=='e'&&
                pname[6]=='_'&&
                pname[7]=='t'&&
                pname[8]=='y'&&pname[9]=='p'&&
                pname[10]=='e'&&pname[11]=='\0'
                ) {
                // pdata is a string like "memory\0"
                if (len >= 6 && strncmp((const char*)pdata, "memory", 6) == 0) {
                    device_type_memory = 1;
                }
            }
            // Capture root-level #address-cells and #size-cells (depth==1 means root's properties)
            if (depth == 1) {
                if (strcmp(pname, "#address-cells") == 0 && len >= 4) {
                    parent_addr_cells = (int)be32_to_cpu_u32(*(const uint32_t*)pdata);
                } else if (strcmp(pname, "#size-cells") == 0 && len >= 4) {
                    parent_size_cells = (int)be32_to_cpu_u32(*(const uint32_t*)pdata);
                }
            }
            if (in_memory_node && pname[0]=='r'&&pname[1]=='e'&&pname[2]=='g'&&pname[3]=='\0') {
                // Assume 64-bit address/size pairs if len >= 16; else 32-bit.
                uint64_t base = 0, size = 0;
                int ac = parent_addr_cells > 0 ? parent_addr_cells : 2;
                int sc = parent_size_cells > 0 ? parent_size_cells : 2;
                const uint8_t *q = pdata;
                // Only read the first tuple
                for (int c = 0; c < ac && (q + 4) <= (pdata + len); ++c) {
                    uint32_t cell = be32_to_cpu_u32(*(const uint32_t*)q); q += 4;
                    base = (base << 32) | cell;
                }
                for (int c = 0; c < sc && (q + 4) <= (pdata + len); ++c) {
                    uint32_t cell = be32_to_cpu_u32(*(const uint32_t*)q); q += 4;
                    size = (size << 32) | cell;
                }
                // Only accept if either name matched (memory or memory@) or device_type was explicitly memory
                if (in_memory_node || device_type_memory) {
                    if (base_out) *base_out = base;
                    if (size_out) *size_out = size;
                    return 0;
                }
            }
            p += (len + 3) & ~3u;
        } else if (token == DTB_END_NODE) {
            if (depth > 0) depth--;
            in_memory_node = 0;
            device_type_memory = 0;
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

// Reserve regions from /reserved-memory
static void reserve_reserved_memory(void) {
    struct dtb_header *hdr = dtb_get();
    if (!hdr) return;
    if (be32_to_cpu_u32(hdr->magic) != 0xd00dfeed) return;

    const uint8_t *fdt = (const uint8_t *)hdr;
    uint32_t off_struct = be32_to_cpu_u32(hdr->off_dt_struct);
    uint32_t off_strings = be32_to_cpu_u32(hdr->off_dt_strings);
    const char *strings = (const char *)(fdt + off_strings);

    uint32_t p = off_struct;
    int depth = 0;
    int in_reserved = 0; // inside /reserved-memory
    int addr_cells = 2;  // defaults for 64-bit
    int size_cells = 2;

    while (1) {
        uint32_t token = be32_to_cpu_u32(*(const uint32_t *)(fdt + p)); p += 4;
        if (token == DTB_BEGIN_NODE) {
            const char *name = (const char *)(fdt + p);
            size_t name_len = 0; while (name[name_len] != '\0') name_len++;
            p += (name_len + 4) & ~3u;
            depth++;
            if (depth == 2 && strncmp(name, "reserved-memory", 15) == 0) {
                in_reserved = 1;
                // reset to defaults at the reserved-memory node
                addr_cells = 2; size_cells = 2;
            }
        } else if (token == DTB_PROP) {
            uint32_t len = be32_to_cpu_u32(*(const uint32_t *)(fdt + p)); p += 4;
            uint32_t nameoff = be32_to_cpu_u32(*(const uint32_t *)(fdt + p)); p += 4;
            const char *pname = strings + nameoff;
            const uint8_t *pdata = fdt + p;
            if (in_reserved) {
                if (strcmp(pname, "#address-cells") == 0 && len >= 4) {
                    addr_cells = (int)be32_to_cpu_u32(*(const uint32_t*)pdata);
                } else if (strcmp(pname, "#size-cells") == 0 && len >= 4) {
                    size_cells = (int)be32_to_cpu_u32(*(const uint32_t*)pdata);
                } else if (strcmp(pname, "reg") == 0) {
                    // reg on the reserved-memory node itself is uncommon; children have reg. Ignore here.
                }
            } else if (in_reserved == 2 && strcmp(pname, "reg") == 0) {
                // Child under reserved-memory: parse reg entries
                int tuple_cells = addr_cells + size_cells;
                if (tuple_cells <= 0) tuple_cells = 4;
                int tuples = (int)len / (4 * tuple_cells);
                const uint8_t *q = pdata;
                for (int t = 0; t < tuples; ++t) {
                    uint64_t base = 0, size = 0;
                    // read address
                    for (int c = 0; c < addr_cells; ++c) {
                        uint32_t cell = be32_to_cpu_u32(*(const uint32_t*)q); q += 4;
                        base = (base << 32) | cell;
                    }
                    // read size
                    for (int c = 0; c < size_cells; ++c) {
                        uint32_t cell = be32_to_cpu_u32(*(const uint32_t*)q); q += 4;
                        size = (size << 32) | cell;
                    }
                    if (size) {
                        // Align to page boundaries for safety
                        uint64_t aligned_base = base & ~(PMM_PAGE_SIZE - 1);
                        uint64_t end = base + size;
                        uint64_t aligned_end = (end + PMM_PAGE_SIZE - 1) & ~(PMM_PAGE_SIZE - 1);
                        reserve_range(aligned_base, (aligned_end > aligned_base) ? (aligned_end - aligned_base) : 0);
                        printk("PMM: reserved DTB region %p - %p\n", (void*)aligned_base, (void*)aligned_end);
                    }
                }
            }
            p += (len + 3) & ~3u;
        } else if (token == DTB_END_NODE) {
            if (in_reserved == 2) {
                // leaving a child under reserved-memory
                in_reserved = 1;
            } else if (in_reserved == 1) {
                // leaving reserved-memory node
                in_reserved = 0;
            }
            if (depth > 0) depth--;
        } else if (token == DTB_NOP) {
        } else if (token == DTB_END) {
            break;
        } else {
            break;
        }

        // Detect entering a child of reserved-memory: this is when we see BEGIN_NODE after in_reserved==1
        if (token == DTB_BEGIN_NODE && in_reserved == 1 && depth >= 3) {
            in_reserved = 2; // inside a child node
        }
    }
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

    // Ensure 4 KiB alignment of the managed range
    uint64_t end = base + size;
    uint64_t aligned_base = (base + PMM_PAGE_SIZE - 1) & ~(PMM_PAGE_SIZE - 1);
    uint64_t aligned_end  = end & ~(PMM_PAGE_SIZE - 1);
    if (aligned_end <= aligned_base) {
        printk("PMM: invalid RAM range after alignment, falling back\n");
        aligned_base = 0x40000000ULL; aligned_end = aligned_base + 0x40000000ULL;
    }
    base = aligned_base; size = aligned_end - aligned_base;

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
    reserve_reserved_memory();
    // Reserve UART MMIO page if available to avoid PMM handing it out
    uint64_t uart_base = 0;
    if (dtb_get_stdout_uart_base(&uart_base) == 0 && uart_base) {
        // Reserve one page around the UART base (PL011 fits in 4KB)
        uint64_t mmio_base = uart_base & ~(PMM_PAGE_SIZE - 1);
        reserve_range(mmio_base, PMM_PAGE_SIZE);
        printk("PMM: reserved UART MMIO at %p\n", (void*)mmio_base);
    }

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
        size_t idx = first + i;
        if (!test_bit(idx)) {
            // double free detection
            printk("PMM: warning: double-free page %d at %p ignored\n", (int)idx, (void*)page_to_addr(idx));
            continue;
        }
        clear_bit(idx);
        pmm_pages_free++;
    }
}

void pmm_free_page(void* addr) { pmm_free_pages(addr, 1); }

size_t pmm_total_pages(void) { return pmm_pages_total; }
size_t pmm_free_pages_count(void) { return pmm_pages_free; }

int pmm_check(void) {
    // Count allocated pages by scanning bitmap
    size_t set_count = 0;
    for (size_t i = 0; i < pmm_pages_total; ++i) {
        if (test_bit(i)) set_count++;
    }
    size_t expected_set = pmm_pages_total - pmm_pages_free;
    if (set_count != expected_set) {
        printk("PMM: check failed, set=%d expected=%d\n", (int)set_count, (int)expected_set);
        return -1;
    }
    return 0;
}
