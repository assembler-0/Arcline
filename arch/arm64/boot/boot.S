/*
 * arch/arm64/boot/boot.S
 *
 * Copyright (C) 2025 Arcline
 *
 * This is the first code that runs on startup. It is responsible for
 * setting up a temporary stack, clearing the BSS section, and jumping
 * to the C entry point (kmain).
 */

.section ".text.boot"

.global _start

_start:
    // Save DTB pointer (passed in x0 by bootloader/QEMU)
    ldr x1, =dtb_ptr
    str x0, [x1]

    // Also save original x0 for debugging
    ldr x1, =boot_x0
    str x0, [x1]

dtb_saved:

    // Check if we're the primary CPU (MPIDR_EL1 & 0xFF should be 0)
    mrs x0, mpidr_el1
    and x0, x0, #0xFF
    cbnz x0, hang // If not CPU 0, hang

    // Set up the stack pointer (must be 16-byte aligned for aarch64)
    ldr x0, =_stack_top
    and x0, x0, #~15 // Align to 16 bytes
    mov sp, x0

    // Clear the BSS section before calling C code
    ldr x0, =__bss_start
    ldr x1, =__bss_end

bss_clear_loop:
    cmp x0, x1
    b.ge bss_clear_done
    str xzr, [x0], #8
    b bss_clear_loop

bss_clear_done:
    // Ensure all memory operations complete
    dsb sy
    isb

    // Jump to the C entry point (kmain)
    bl kmain

// If kmain ever returns, we enter a low-power halt loop
hang:
    wfe // Wait for event (low-power state)
    b hang

// Put stack in .data section so it's available immediately
.section ".data"
.align 16 // Ensure 16-byte alignment for aarch64
.global stack_bottom
.global _stack_top
stack_bottom:
    .space 16384 // Reserve 16KB for the boot stack
_stack_top:

// Global variable to store DTB pointer
.global dtb_ptr
dtb_ptr:
    .quad 0

// Debug variable to store original x0
.global boot_x0
boot_x0:
    .quad 0