#include <drivers/gic.h>
#include <kernel/irq.h>
#include <kernel/printk.h>
#include <kernel/sched/task.h>

void handle_irq(cpu_context_t *ctx) {
    uint32_t irq = gic_ack_irq();

    if (irq < 1020) {
        irq_dispatch(ctx, irq);
    }

    gic_eoi_irq(irq);
}
