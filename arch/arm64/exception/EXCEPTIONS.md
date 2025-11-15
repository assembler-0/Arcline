# ARM64 Exception Handling

## Current Exception Handlers

### Sync Exception (handle_sync_exception)
- Handles synchronous exceptions (page faults, undefined instructions, etc.)
- Reads ESR_EL1 (Exception Syndrome Register) for exception class
- Reads FAR_EL1 (Fault Address Register) for faulting address
- Reads ELR_EL1 (Exception Link Register) for return address

### IRQ (handle_irq)
- Handles hardware interrupts
- Full context save/restore (x0-x28, x29, x30)
- Calls GIC to acknowledge interrupt
- Dispatches to registered handler
- Sends EOI to GIC

### FIQ (handle_fiq)
- Fast interrupt handler (currently just prints message)

### SError (handle_serror)
- System error handler (asynchronous aborts)

## Adding New Exception Types

### 1. Add Handler Function in exception.c

```c
void handle_my_exception(void) {
    // Read system registers if needed
    uint64_t esr, far, elr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    
    // Handle exception
    printk("My exception: ESR=%p FAR=%p ELR=%p\n", 
           (void*)esr, (void*)far, (void*)elr);
}
```

### 2. Update exception.S

Replace existing handler stub:
```asm
my_exception_handler:
    stp x29, x30, [sp, #-16]!
    bl handle_my_exception
    ldp x29, x30, [sp], #16
    eret
```

### 3. Declare in exception.h (if needed)

```c
void handle_my_exception(void);
```

## Exception Classes (ESR_EL1.EC)

Common exception classes:
- 0x00: Unknown reason
- 0x01: Trapped WFI/WFE
- 0x07: Access to SVE/SIMD/FP
- 0x15: SVC instruction (syscalls)
- 0x20: Instruction abort (lower EL)
- 0x21: Instruction abort (same EL)
- 0x24: Data abort (lower EL)
- 0x25: Data abort (same EL)
- 0x2F: SError interrupt

## Useful System Registers

- **ESR_EL1**: Exception syndrome (type, details)
- **FAR_EL1**: Fault address (for aborts)
- **ELR_EL1**: Return address
- **SPSR_EL1**: Saved processor state
- **VBAR_EL1**: Vector base address

## Context Save/Restore

IRQ handler saves all general-purpose registers:
- x0-x28: General purpose
- x29: Frame pointer
- x30: Link register

For syscalls (future), also save:
- SP_EL0: User stack pointer
- SPSR_EL1: Processor state
- ELR_EL1: Return address

## Example: Adding Syscall Handler

```c
// In exception.c
void handle_syscall(void) {
    uint64_t syscall_num, arg0, arg1, arg2;
    
    // Syscall number in x8, args in x0-x5
    __asm__ volatile(
        "mov %0, x8\n"
        "mov %1, x0\n"
        "mov %2, x1\n"
        "mov %3, x2\n"
        : "=r"(syscall_num), "=r"(arg0), "=r"(arg1), "=r"(arg2)
    );
    
    // Dispatch to syscall table
    uint64_t ret = do_syscall(syscall_num, arg0, arg1, arg2);
    
    // Return value in x0
    __asm__ volatile("mov x0, %0" :: "r"(ret));
}
```

```asm
// In exception.S - replace sync_handler for EL0
sync_handler_el0:
    stp x29, x30, [sp, #-16]!
    stp x0, x1, [sp, #-16]!
    // ... save all registers
    
    bl handle_syscall
    
    // ... restore all registers
    ldp x0, x1, [sp], #16
    ldp x29, x30, [sp], #16
    eret
```
