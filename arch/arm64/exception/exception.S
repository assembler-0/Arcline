// Exception vector table for ARM64

.section ".text"

.align 11
.global exception_vector_table
exception_vector_table:
    // Current EL with SP0
    .align 7; b sync_handler_sp0
    .align 7; b irq_handler_sp0
    .align 7; b fiq_handler_sp0
    .align 7; b serror_handler_sp0

    // Current EL with SPx
    .align 7; b sync_handler_spx
    .align 7; b irq_handler_spx
    .align 7; b fiq_handler_spx
    .align 7; b serror_handler_spx

    // Lower EL using AArch64
    .align 7; b sync_handler_lower_aarch64
    .align 7; b irq_handler_lower_aarch64
    .align 7; b fiq_handler_lower_aarch64
    .align 7; b serror_handler_lower_aarch64

    // Lower EL using AArch32
    .align 7; b sync_handler_lower_aarch32
    .align 7; b irq_handler_lower_aarch32
    .align 7; b fiq_handler_lower_aarch32
    .align 7; b serror_handler_lower_aarch32

.macro save_all_but_sp
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x15, x16, [sp, #-16]!
    stp x13, x14, [sp, #-16]!
    stp x11, x12, [sp, #-16]!
    stp x9, x10, [sp, #-16]!
    stp x7, x8, [sp, #-16]!
    stp x5, x6, [sp, #-16]!
    stp x3, x4, [sp, #-16]!
    stp x1, x2, [sp, #-16]!
    stp x0, xzr, [sp, #-16]! // Store x0, and 0 for where x1 would be
.endm

.macro restore_all_but_sp
    ldp x0, xzr, [sp], #16
    ldp x1, x2, [sp], #16
    ldp x3, x4, [sp], #16
    ldp x5, x6, [sp], #16
    ldp x7, x8, [sp], #16
    ldp x9, x10, [sp], #16
    ldp x11, x12, [sp], #16
    ldp x13, x14, [sp], #16
    ldp x15, x16, [sp], #16
    ldp x17, x18, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16
.endm

irq_handler_spx:
    sub sp, sp, #272 // sizeof(cpu_context_t)
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]

    mov x9, sp
    add x9, x9, #272
    str x9, [sp, #248] // saved sp

    mrs x9, elr_el1
    str x9, [sp, #256] // pc
    mrs x9, spsr_el1
    str x9, [sp, #264] // pstate

    mov x0, sp
    bl handle_irq  // May call schedule_preempt which modifies context in-place

    // Restore from (possibly modified) context on stack
    // Load PC and pstate first
    ldr x9, [sp, #256] // Load (possibly new) PC
    msr elr_el1, x9
    ldr x9, [sp, #264] // Load (possibly new) pstate
    msr spsr_el1, x9

    // Restore all registers from stack (context may have been modified)
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]

    // Restore SP last: load new SP value, pop context frame, then switch
    ldr x9, [sp, #248]  // Load (possibly new) SP
    add sp, sp, #272    // Pop context frame first
    mov sp, x9          // Then switch to new SP

    eret

// Stubs for other handlers
sync_handler_sp0:
sync_handler_spx:
    mrs x9, esr_el1
    lsr x9, x9, #26
    cmp x9, #0x15 // SVC instruction
    b.eq svc_handler

    // Not an SVC, treat as a generic sync exception
    stp x29, x30, [sp, #-16]!
    bl handle_sync_exception
    ldp x29, x30, [sp], #16
    eret

sync_handler_lower_aarch64:
sync_handler_lower_aarch32:
    stp x29, x30, [sp, #-16]!
    bl handle_sync_exception
    ldp x29, x30, [sp], #16
    eret

svc_handler:
    sub sp, sp, #272 // sizeof(cpu_context_t)
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]

    mov x9, sp
    add x9, x9, #272
    str x9, [sp, #248] // saved sp

    mrs x9, elr_el1
    str x9, [sp, #256] // pc
    mrs x9, spsr_el1
    str x9, [sp, #264] // pstate

    mov x0, sp
    bl handle_svc  // May call sys_exit or sys_kill

    // Check if we need to reschedule (current task became zombie)
    bl should_reschedule  // Returns 1 if current task is zombie
    cmp x0, #0
    b.eq .Lsvc_no_resched

    // Reschedule needed - call schedule_preempt with context
    mov x0, sp
    bl schedule_preempt  // Will modify context in-place if switching tasks

.Lsvc_no_resched:
    // Restore from (possibly modified) context
    // Load PC and pstate first
    ldr x9, [sp, #256] // Load (possibly new) PC
    msr elr_el1, x9
    ldr x9, [sp, #264] // Load (possibly new) pstate
    msr spsr_el1, x9

    // Restore all registers from stack
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]

    // Restore SP last: load new SP value, pop context frame, then switch
    ldr x9, [sp, #248]  // Load (possibly new) SP
    add sp, sp, #272    // Pop context frame first
    mov sp, x9          // Then switch to new SP

    eret

irq_handler_sp0:
irq_handler_lower_aarch64:
irq_handler_lower_aarch32:
    b irq_handler_spx

fiq_handler_sp0:
fiq_handler_spx:
fiq_handler_lower_aarch64:
fiq_handler_lower_aarch32:
    stp x29, x30, [sp, #-16]!
    bl handle_fiq
    ldp x29, x30, [sp], #16
    eret

serror_handler_sp0:
serror_handler_spx:
serror_handler_lower_aarch64:
serror_handler_lower_aarch32:
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
