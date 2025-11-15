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

Higher-half kernel on AArch64: Do we need to map “a lot” like x86?

- On x86 higher-half kernels often mirror large parts of physical memory into the upper half and also keep a lower-half identity window for early/compat code. This can involve many page tables if done at 4 KiB granularity.
- On AArch64, a common design is to use two translation base registers:
  - TTBR0_EL1 for the lower VA space (userspace or a small identity window)
  - TTBR1_EL1 for the upper VA space (kernel higher-half)
- Because TTBR1 covers the upper half of the VA space, the kernel can live entirely there, while keeping a minimal identity slice via TTBR0 for early boot and device pokes. You do not need to map “a lot” if you choose a compact kernel layout:
  - Map only kernel text/rodata, data/bss, stacks, DTB, and required MMIO in TTBR1.
  - Keep a small identity window (e.g., the first 1–2 GiB or even just the RAM region covering the kernel and early allocs) via TTBR0 until fully booted.
- Large block mappings help: AArch64 supports block entries at 1 GiB and 2 MiB when aligned, so you can drastically reduce page table size compared to a 4 KiB page-only scheme.

Configured higher-half base

- Build-time constant KERNEL_VIRT_BASE (default 0xFFFFFF8000000000 for 48-bit VA) is exposed to C as VMM_KERNEL_VIRT_BASE.
- The helper vmm_kernel_base() returns this value. We only log it for now; enabling MMU and actually relocating the kernel there will come in the next step.

Planned enablement steps (AArch64)

1. Create kernel page tables rooted for TTBR1_EL1 at VMM_KERNEL_VIRT_BASE.
   - Map kernel image sections with correct attributes (Normal, RO+X for text; RW+PXN for data).
   - Map DTB and required device MMIO (UART) with Device-nGnRE.
2. Create a minimal TTBR0_EL1 table providing a small identity window over RAM/MMIO as needed.
3. Program MAIR_EL1, TCR_EL1 (split between TTBR0/1), TTBR0/1, SCTLR_EL1; then enable the MMU.
4. Switch stack pointers and exception vectors to higher-half addresses; retain identity window temporarily.
5. Optionally drop or shrink the identity window once stable.

API summary
- vmm_init(): initialize the VMA tree (no MMU changes yet).
- vmm_map(va, pa, size, attrs): add a page-aligned mapping to the RB-tree; fails on overlap.
- vmm_unmap(va, size): remove an exact-match VMA (split/merge to arrive later).
- vmm_protect(va, size, attrs): change attributes for an exact-match VMA.
- vmm_virt_to_phys(va, &pa): translate using the VMA tree; falls back to identity if unmapped.
- vmm_dump(): print VMAs in order for debugging.
