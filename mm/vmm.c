// Minimal VMM scaffolding (Stage 0): identity helpers

#include <mm/vmm.h>

void vmm_init_identity(void) {
    // Placeholder for future page-table setup.
}

int vmm_virt_to_phys(uint64_t va, uint64_t *pa_out) {
    if (!pa_out) return -1;
    // Identity mapping assumption for now
    *pa_out = va;
    return 0;
}
