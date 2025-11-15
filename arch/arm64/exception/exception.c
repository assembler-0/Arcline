#include <kernel/printk.h>
#include <stdint.h>

#define ESR_EC_SHIFT 26
#define ESR_EC_MASK 0x3F

#define EC_DATA_ABORT_SAME  0x25
#define EC_INSTR_ABORT_SAME 0x21

void handle_sync_exception(void) {
    uint64_t esr, far, elr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));
    
    uint32_t ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
    
    fprintk(STDERR_FD, "\nSync exception: EC=0x%x FAR=%p ELR=%p\n", ec, (void*)far, (void*)elr);
    
    while(1) __asm__ volatile("wfe");
}

void handle_fiq(void) {
    fprintk(STDERR_FD, "FIQ received\n");
}

void handle_serror(void) {
    fprintk(STDERR_FD, "SError received\n");
    while(1) __asm__ volatile("wfe");
}