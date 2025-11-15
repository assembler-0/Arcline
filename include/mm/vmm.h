#ifndef ARCLINE_MM_VMM_H
#define ARCLINE_MM_VMM_H

#include <stdint.h>
#include <stddef.h>

// Minimal VMM scaffolding (Stage 0): identity helpers

// Initialize early VMM state (identity assumptions). For now, this is a no-op
// but gives us a place to hang future page-table setup.
void vmm_init_identity(void);

// Translate virtual to physical under identity-mapping assumption.
// Returns 0 on success and writes to *pa_out.
// For now, success is unconditional and pa = va.
int vmm_virt_to_phys(uint64_t va, uint64_t *pa_out);

// Translate physical to virtual under identity-mapping assumption.
static inline uint64_t vmm_phys_to_virt(uint64_t pa) { return pa; }

#endif // ARCLINE_MM_VMM_H
