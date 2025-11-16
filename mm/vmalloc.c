// VMM allocator with coalescing free-list and guard pages

#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/mmu.h>
#include <kernel/printk.h>

#define VMALLOC_START 0xFFFFFF8080000000ULL
#define VMALLOC_END   0xFFFFFF80C0000000ULL
#define GUARD_SIZE 4096ULL

typedef struct free_block {
    uint64_t va;
    uint64_t size;
    struct free_block *next;
    struct free_block *prev;
} free_block_t;

static free_block_t *free_list = NULL;
static uint64_t vmalloc_next = VMALLOC_START;

static free_block_t free_pool[512];
static free_block_t *free_pool_list = NULL;

static void init_pool(void) {
    for (int i = 0; i < 512; i++) {
        free_pool[i].next = free_pool_list;
        free_pool_list = &free_pool[i];
    }
}

static free_block_t* alloc_block(void) {
    if (!free_pool_list) {
        static int initialized = 0;
        if (!initialized) {
            init_pool();
            initialized = 1;
        }
        if (!free_pool_list) return NULL;
    }
    free_block_t *blk = free_pool_list;
    free_pool_list = blk->next;
    blk->next = blk->prev = NULL;
    return blk;
}

static void free_block(free_block_t *blk) {
    blk->next = free_pool_list;
    free_pool_list = blk;
}

static void coalesce_free_list(void) {
    if (!free_list) return;
    
    int changed = 1;
    while (changed) {
        changed = 0;
        free_block_t *cur = free_list;
        while (cur) {
            free_block_t *next = cur->next;
            while (next) {
                if (cur->va + cur->size == next->va) {
                    cur->size += next->size;
                    if (cur->prev) cur->prev->next = cur->next;
                    else free_list = cur->next;
                    if (cur->next) cur->next->prev = cur->prev;
                    
                    if (next->prev) next->prev->next = next->next;
                    else free_list = next->next;
                    if (next->next) next->next->prev = next->prev;
                    
                    free_block_t *tmp = next->next;
                    free_block(next);
                    next = tmp;
                    changed = 1;
                } else if (next->va + next->size == cur->va) {
                    next->size += cur->size;
                    if (next->prev) next->prev->next = next->next;
                    else free_list = next->next;
                    if (next->next) next->next->prev = next->prev;
                    
                    if (cur->prev) cur->prev->next = cur->next;
                    else free_list = cur->next;
                    if (cur->next) cur->next->prev = cur->prev;
                    
                    free_block_t *tmp = next->next;
                    free_block(cur);
                    cur = next;
                    next = tmp;
                    changed = 1;
                } else {
                    next = next->next;
                }
            }
            cur = cur->next;
        }
    }
}

static uint64_t find_free_space(uint64_t size) {
    free_block_t **prev = &free_list;
    free_block_t *cur = free_list;
    free_block_t *best = NULL;
    free_block_t **best_prev = NULL;
    
    while (cur) {
        if (cur->size >= size && (!best || cur->size < best->size)) {
            best = cur;
            best_prev = prev;
        }
        prev = &cur->next;
        cur = cur->next;
    }
    
    if (best) {
        uint64_t va = best->va;
        if (best->size == size) {
            *best_prev = best->next;
            if (best->next) best->next->prev = best->prev;
            free_block(best);
        } else {
            best->va += size;
            best->size -= size;
        }
        return va;
    }
    
    if (vmalloc_next + size > VMALLOC_END) return 0;
    uint64_t va = vmalloc_next;
    vmalloc_next += size;
    return va;
}

static void add_free_space(uint64_t va, uint64_t size) {
    free_block_t *blk = alloc_block();
    if (!blk) return;
    
    blk->va = va;
    blk->size = size;
    blk->next = free_list;
    blk->prev = NULL;
    if (free_list) free_list->prev = blk;
    free_list = blk;
    
    coalesce_free_list();
}

void* vmalloc(uint64_t size) {
    if (size == 0) return NULL;
    
    uint64_t pages = (size + 4095) / 4096;
    uint64_t data_size = pages * 4096;
    uint64_t total_size = data_size + 2 * GUARD_SIZE;
    
    uint64_t base_va = find_free_space(total_size);
    if (!base_va) return NULL;
    
    void *guard1 = pmm_alloc_page();
    if (!guard1) {
        add_free_space(base_va, total_size);
        return NULL;
    }
    vmm_map(base_va, (uint64_t)guard1, GUARD_SIZE, VMM_ATTR_PXN);
    
    uint64_t data_va = base_va + GUARD_SIZE;
    for (uint64_t i = 0; i < pages; i++) {
        void *page = pmm_alloc_page();
        if (!page) {
            for (uint64_t j = 0; j < i; j++) {
                uint64_t pa;
                vmm_virt_to_phys(data_va + j * 4096, &pa);
                vmm_unmap(data_va + j * 4096, 4096);
                pmm_free_page((void*)pa);
            }
            vmm_unmap(base_va, GUARD_SIZE);
            pmm_free_page(guard1);
            add_free_space(base_va, total_size);
            return NULL;
        }
        vmm_map(data_va + i * 4096, (uint64_t)page, 4096,
                VMM_ATTR_R | VMM_ATTR_W | VMM_ATTR_NORMAL | VMM_ATTR_PXN);
    }
    
    void *guard2 = pmm_alloc_page();
    if (!guard2) {
        for (uint64_t i = 0; i < pages; i++) {
            uint64_t pa;
            vmm_virt_to_phys(data_va + i * 4096, &pa);
            vmm_unmap(data_va + i * 4096, 4096);
            pmm_free_page((void*)pa);
        }
        vmm_unmap(base_va, GUARD_SIZE);
        pmm_free_page(guard1);
        add_free_space(base_va, total_size);
        return NULL;
    }
    vmm_map(data_va + data_size, (uint64_t)guard2, GUARD_SIZE, VMM_ATTR_PXN);
    
    return (void*)data_va;
}

void vfree(void *ptr, uint64_t size) {
    if (!ptr || size == 0) return;
    
    uint64_t pages = (size + 4095) / 4096;
    uint64_t data_size = pages * 4096;
    uint64_t data_va = (uint64_t)ptr;
    uint64_t base_va = data_va - GUARD_SIZE;
    uint64_t total_size = data_size + 2 * GUARD_SIZE;
    
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t pa;
        if (vmm_virt_to_phys(data_va + i * 4096, &pa) == 0) {
            vmm_unmap(data_va + i * 4096, 4096);
            pmm_free_page((void*)pa);
        }
    }
    
    uint64_t pa;
    if (vmm_virt_to_phys(base_va, &pa) == 0) {
        vmm_unmap(base_va, GUARD_SIZE);
        pmm_free_page((void*)pa);
    }
    if (vmm_virt_to_phys(data_va + data_size, &pa) == 0) {
        vmm_unmap(data_va + data_size, GUARD_SIZE);
        pmm_free_page((void*)pa);
    }
    
    add_free_space(base_va, total_size);
}

void vmalloc_stats(void) {
    uint64_t total_free = 0;
    int block_count = 0;
    
    free_block_t *cur = free_list;
    while (cur) {
        total_free += cur->size;
        block_count++;
        cur = cur->next;
    }
    
    uint64_t used = vmalloc_next - VMALLOC_START;
    uint64_t total = VMALLOC_END - VMALLOC_START;
    
    printk("vmalloc: used=%llu KB, free=%llu KB, blocks=%d\n",
           used / 1024, total_free / 1024, block_count);
}
