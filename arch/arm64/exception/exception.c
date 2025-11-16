#include <kernel/panic.h>
#include <kernel/sched/task.h>
#include <kernel/syscall.h>
#include <kernel/types.h>

#define ESR_EC_SHIFT 26
#define ESR_EC_MASK 0x3F

#define EC_DATA_ABORT_SAME 0x25
#define EC_INSTR_ABORT_SAME 0x21

void handle_svc(cpu_context_t *ctx) {
    uint64_t syscall_num = ctx->x8;
    uint64_t ret = do_syscall(syscall_num, ctx->x0, ctx->x1, ctx->x2, ctx->x3,
                                ctx->x4, ctx->x5);
    ctx->x0 = ret;
}

void handle_sync_exception(void) {
    uint64_t esr, far, elr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    __asm__ volatile("mrs %0, elr_el1" : "=r"(elr));

    uint32_t ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

    panic("Sync exception: EC=0x%x FAR=%p ELR=%p", ec, (void *)far,
          (void *)elr);
}

void handle_fiq(void) { panic("Unexpected FIQ"); }

void handle_serror(void) { panic("SError exception"); }