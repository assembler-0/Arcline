// Minimal VMM scaffolding (Stage 0): identity helpers and API stubs

#include <mm/vmm.h>

void vmm_init_identity(void) {
    // Placeholder for future page-table setup.
}

// Full VMM API stubs (to be implemented in Stage 1)
int vmm_init(void) {
    // TODO: Implement page table creation and MMU enable.
    return 0;
}

int vmm_map(uint64_t va, uint64_t pa, uint64_t size, uint32_t attrs) {
    (void)va; (void)pa; (void)size; (void)attrs;
    // TODO: Map range using page tables and RB-tree VMA manager.
    return -1;
}

int vmm_unmap(uint64_t va, uint64_t size) {
    (void)va; (void)size;
    // TODO: Unmap range and free page table pages if possible.
    return -1;
}

int vmm_protect(uint64_t va, uint64_t size, uint32_t attrs) {
    (void)va; (void)size; (void)attrs;
    // TODO: Update permissions for mapped range.
    return -1;
}

void vmm_dump(void) {
    // TODO: Dump VMAs and page table summary (debug only).
}

int vmm_virt_to_phys(uint64_t va, uint64_t *pa_out) {
    if (!pa_out) return -1;
    // Identity mapping assumption for now
    *pa_out = va;
    return 0;
}
