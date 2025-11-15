// VMM allocator with free-list and guard pages

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
} free_block_t;

static free_block_t *free_list = NULL;
static uint64_t vmalloc_next = VMALLOC_START;

static free_block_t free_pool[256];
static int free_pool_used = 0;

static free_block_t* alloc_block(void) {
    if (free_pool_used >= 256) return NULL;
    return &free_pool[free_pool_used++];
}

static uint64_t find_free_space(uint64_t size) {
    free_block_t **prev = &free_list;
    free_block_t *cur = free_list;
    
    while (cur) {
        if (cur->size >= size) {
            uint64_t va = cur->va;
            if (cur->size == size) {
                *prev = cur->next;
            } else {
                cur->va += size;
                cur->size -= size;
            }
            return va;
        }
        prev = &cur->next;
        cur = cur->next;
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
    free_list = blk;
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
