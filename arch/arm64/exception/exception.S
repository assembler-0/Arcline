// Exception vector table for ARM64

.section ".text"

.align 11
.global exception_vector_table
exception_vector_table:
    // Current EL with SP0
    .align 7
    b sync_handler
    .align 7
    b irq_handler
    .align 7
    b fiq_handler
    .align 7
    b serror_handler

    // Current EL with SPx
    .align 7
    b sync_handler
    .align 7
    b irq_handler
    .align 7
    b fiq_handler
    .align 7
    b serror_handler

    // Lower EL using AArch64
    .align 7
    b sync_handler
    .align 7
    b irq_handler
    .align 7
    b fiq_handler
    .align 7
    b serror_handler

    // Lower EL using AArch32
    .align 7
    b sync_handler
    .align 7
    b irq_handler
    .align 7
    b fiq_handler
    .align 7
    b serror_handler

sync_handler:
    stp x29, x30, [sp, #-16]!
    bl handle_sync_exception
    ldp x29, x30, [sp], #16
    eret

irq_handler:
    stp x29, x30, [sp, #-16]!
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x8, x9, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    stp x18, x19, [sp, #-16]!
    stp x20, x21, [sp, #-16]!
    stp x22, x23, [sp, #-16]!
    stp x24, x25, [sp, #-16]!
    stp x26, x27, [sp, #-16]!
    stp x28, xzr, [sp, #-16]!

    bl handle_irq

    ldp x28, xzr, [sp], #16
    ldp x26, x27, [sp], #16
    ldp x24, x25, [sp], #16
    ldp x22, x23, [sp], #16
    ldp x20, x21, [sp], #16
    ldp x18, x19, [sp], #16
    ldp x16, x17, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8, x9, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16
    ldp x29, x30, [sp], #16
    eret

fiq_handler:
    stp x29, x30, [sp, #-16]!
    bl handle_fiq
    ldp x29, x30, [sp], #16
    eret

serror_handler:
    stp x29, x30, [sp, #-16]!
    bl handle_serror
    ldp x29, x30, [sp], #16
    eret

.global exception_init
exception_init:
    adr x0, exception_vector_table
    msr vbar_el1, x0
    isb
    ret