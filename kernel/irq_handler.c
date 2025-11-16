#include <drivers/gic.h>
#include <kernel/irq.h>

void handle_irq(void) {
    uint32_t irq = gic_ack_irq();

    if (irq < 1020) {
        irq_dispatch(irq);
    }

    gic_eoi_irq(irq);
}
