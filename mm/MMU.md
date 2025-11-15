# ARM64 MMU Implementation

## Overview
This implements a 4-level page table structure for ARM64 with 48-bit virtual addressing.

## Page Table Hierarchy
- **L0 (PGD)**: Bits [47:39] - 512 entries
- **L1 (PUD)**: Bits [38:30] - 512 entries  
- **L2 (PMD)**: Bits [29:21] - 512 entries
- **L3 (PTE)**: Bits [20:12] - 512 entries
- **Offset**: Bits [11:0] - 4KB page

## Memory Attributes (MAIR_EL1)
- **Index 0**: Device memory (nGnRnE)
- **Index 1**: Normal non-cacheable
- **Index 2**: Normal write-back cacheable

## Key Functions

### mmu_init()
- Allocates kernel page table (PGD)
- Identity maps kernel code/data
- Identity maps first 1GB for devices

### mmu_enable()
- Configures MAIR_EL1, TCR_EL1, TTBR1_EL1
- Enables MMU via SCTLR_EL1.M bit
- Uses TTBR1 for kernel (upper half VA space)

### mmu_map_page()
- Maps single 4KB page
- Allocates intermediate tables as needed
- Sets attributes (cacheable, shareable, access flags)

## Integration with VMM
- VMM calls mmu_map_page() when TTBR1 is active
- Translates VMM attributes to PTE flags
- Supports read-only, device, and normal memory types

## Usage
```c
mmu_init();      // Setup page tables
mmu_enable();    // Turn on MMU
vmm_map(...);    // Now programs actual page tables
```
