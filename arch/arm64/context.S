// ARM64 context switching

.global switch_to
switch_to:
    // x0 = prev context, x1 = next context

    // Save callee-saved registers (x19-x30, sp, pc)
    stp x19, x20, [x0, #152]
    stp x21, x22, [x0, #168]
    stp x23, x24, [x0, #184]
    stp x25, x26, [x0, #200]
    stp x27, x28, [x0, #216]
    stp x29, x30, [x0, #232]
    mov x9, sp
    str x9, [x0, #248]
    adr x9, 1f
    str x9, [x0, #256]

    // Restore callee-saved registers
    ldp x19, x20, [x1, #152]
    ldp x21, x22, [x1, #168]
    ldp x23, x24, [x1, #184]
    ldp x25, x26, [x1, #200]
    ldp x27, x28, [x1, #216]
    ldp x29, x30, [x1, #232]
    ldr x9, [x1, #248]
    mov sp, x9
    ldr x9, [x1, #256]
    br x9
1:
    ret