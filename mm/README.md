Virtual Memory Manager (VMM)

Overview
The VMM uses a Red-Black Tree (RB-tree) to manage Virtual Memory Areas (VMAs). Each VMA describes a contiguous virtual address interval [va, va+size) mapped to a contiguous physical interval [pa, pa+size) with attributes (R/W/X and memory type).

Current status
- RB-tree VMA manager implemented (insert/find/remove, overlap checks).
- vmm_map/unmap/protect manipulate VMAs with strict page alignment (4 KiB).
- vmm_virt_to_phys translates via VMA coverage; otherwise falls back to identity.
- Page tables/MMU programming is staged for a later step; for now mappings are tracked logically.

Mapping plan (from → to)
All addresses and sizes below are examples for QEMU virt with the current kernel. Exact values are printed at boot when needed.

- Kernel text and read-only data
  - From: [_kernel_start(text), _etext/rodata_end)
  - To:   Same physical addresses (identity initially)
  - Attr: VMM_ATTR_R | VMM_ATTR_X | VMM_ATTR_NORMAL (execute allowed, read-only)

- Kernel data and BSS
  - From: [data_start, __bss_end)
  - To:   Identity
  - Attr: VMM_ATTR_R | VMM_ATTR_W | VMM_ATTR_NORMAL | VMM_ATTR_PXN (no execute)

- Boot/kernel stacks
  - From: [stack_bottom, _stack_top)
  - To:   Identity
  - Attr: VMM_ATTR_R | VMM_ATTR_W | VMM_ATTR_NORMAL | VMM_ATTR_PXN (no execute)

- Device Tree Blob (DTB)
  - From: [dtb_ptr, dtb_ptr + totalsize)
  - To:   Identity
  - Attr: VMM_ATTR_R | VMM_ATTR_NORMAL | VMM_ATTR_PXN (no execute)

- UART MMIO (chosen stdout-path)
  - From: [uart_base & ~0xFFF, +4 KiB)
  - To:   Same physical page
  - Attr: VMM_ATTR_R | VMM_ATTR_W | VMM_ATTR_DEVICE | VMM_ATTR_PXN (device-nGnRE)

- Linear RAM window (early identity slice)
  - From: [RAM base from DTB, minimal window large enough for kernel and early allocs)
  - To:   Identity
  - Attr: VMM_ATTR_R | VMM_ATTR_W | VMM_ATTR_NORMAL | VMM_ATTR_PXN (no execute)

Notes and future direction
- In the next step we will materialize the above plan into real AArch64 page tables (4 KiB granule), program MAIR/TCR/TTBR/SCTLR, and enable the MMU.
- Where alignment allows, we’ll use 2 MiB and 1 GiB block mappings for performance.
- Eventually we will move the kernel to a higher-half virtual base while keeping a low identity window for early boot and device access.
- Reserved-memory regions from the DTB will be mapped (or intentionally left unmapped) with Device attributes as appropriate.

API summary
- vmm_init(): initialize the VMA tree (no MMU changes yet).
- vmm_map(va, pa, size, attrs): add a page-aligned mapping to the RB-tree; fails on overlap.
- vmm_unmap(va, size): remove an exact-match VMA (split/merge to arrive later).
- vmm_protect(va, size, attrs): change attributes for an exact-match VMA.
- vmm_virt_to_phys(va, &pa): translate using the VMA tree; falls back to identity if unmapped.
- vmm_dump(): print VMAs in order for debugging.
