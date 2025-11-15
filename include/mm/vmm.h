#ifndef ARCLINE_MM_VMM_H
#define ARCLINE_MM_VMM_H

#include <stdint.h>
#include <stddef.h>

// VMM design choice: RB-tree for VMA management (kernel space for now)
// Rationale: O(log n) insert/find/remove, good fragmentation control.

// Mapping attributes (subset; extended later when MMU is enabled)
#define VMM_ATTR_R        (1u << 0)
#define VMM_ATTR_W        (1u << 1)
#define VMM_ATTR_X        (1u << 2)
#define VMM_ATTR_DEVICE   (1u << 3)  // Device-nGnRE when MMU is active
#define VMM_ATTR_NORMAL   (1u << 4)  // Normal memory (cacheable)
#define VMM_ATTR_UXN      (1u << 5)  // Unprivileged eXecute-Never (future)
#define VMM_ATTR_PXN      (1u << 6)  // Privileged eXecute-Never (future)

// Minimal VMM scaffolding (Stage 0): identity helpers

// Initialize early VMM state (identity assumptions). For now, this is a no-op
// but gives us a place to hang future page-table setup.
void vmm_init_identity(void);

// Planned full VMM API (initial implementation without enabling MMU yet)
// Returns 0 on success, negative on error.
int vmm_init(void); // create kernel page tables and VMA tree (MMU kept off for now)
int vmm_map(uint64_t va, uint64_t pa, uint64_t size, uint32_t attrs);
int vmm_unmap(uint64_t va, uint64_t size);
int vmm_protect(uint64_t va, uint64_t size, uint32_t attrs);
void vmm_dump(void); // debug helper

// Translate virtual to physical under identity-mapping assumption.
// Returns 0 on success and writes to *pa_out.
// For now, success is unconditional and pa = va.
int vmm_virt_to_phys(uint64_t va, uint64_t *pa_out);

// Translate physical to virtual under identity-mapping assumption.
static inline uint64_t vmm_phys_to_virt(uint64_t pa) { return pa; }

// Kernel higher-half virtual base configured at build time.
#ifndef VMM_KERNEL_VIRT_BASE
#define VMM_KERNEL_VIRT_BASE 0xFFFFFF8000000000ULL
#endif

// Query the configured kernel higher-half base.
uint64_t vmm_kernel_base(void);

#endif // ARCLINE_MM_VMM_H
