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
    // Set up the stack pointer for the primary core.
    ldr x0, =_stack_top
    mov sp, x0

    // Clear the BSS section before calling C code.
    ldr x0, =__bss_start
    ldr x1, =__bss_end
1:  // Local label 1
    cmp x0, x1
    b.ge 2f // Branch to local label 2 forward
    str xzr, [x0], #8
    b 1b // Branch to local label 1 backward

2:  // Local label 2
    // Jump to the C entry point (kmain).
    bl kmain

// If kmain ever returns, we enter a low-power halt loop.
hang:
    wfe  // Wait for event (low-power state)
    b hang

.section ".bss"
.align 4 // 16-byte alignment as required by AArch64 PCS
stack_bottom:
    .space 16384 // Reserve 16KB for the boot stack
_stack_top:
