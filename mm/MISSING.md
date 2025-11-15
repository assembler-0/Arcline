# Missing MMU/VMM Features

## Critical Missing:

1. **TLB Invalidation**
   - No TLBI after map/unmap/protect
   - Stale TLB entries will cause crashes
   - Need: `tlbi vmalle1is` after changes

2. **Page Unmapping**
   - `vmm_unmap()` doesn't clear PTEs
   - Memory leak - page tables never freed
   - Need: Walk tables, clear entries, free tables

3. **Cache Maintenance**
   - No DC/IC operations after mapping
   - Instruction cache may have stale data
   - Need: `dc civac`, `ic ivau` for executable pages

4. **Permissions Enforcement**
   - Maps everything as RWX
   - No PXN/UXN bits set properly
   - Security issue

5. **Page Fault Handler**
   - No exception handler for translation faults
   - Crash on invalid access instead of handling
   - Need: ESR_EL1 parsing, fault recovery

6. **Recursive Mapping Issue**
   - Page tables themselves not mapped at higher-half
   - Will crash when TTBR0 disabled
   - Need: Map page tables in TTBR1

## Minor Missing:

1. **Large Pages (2MB/1GB)**
   - Only 4KB pages, inefficient for large regions
   - Performance issue

2. **ASID Support**
   - No address space identifiers
   - Can't switch processes without full TLB flush

3. **Memory Barriers**
   - Missing DSB after table updates
   - Potential ordering issues

4. **vmm_protect() incomplete**
   - Updates VMA but not actual PTEs
   - Permissions don't change in hardware
