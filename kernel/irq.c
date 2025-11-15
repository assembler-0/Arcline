#include <kernel/irq.h>
#include <string.h>

#include "drivers/gic.h"

typedef struct {
    irq_handler_t handler;
    void *dev;
} irq_desc_t;

static irq_desc_t irq_table[MAX_IRQS];

void irq_init(void) {
    memset(irq_table, 0, sizeof(irq_table));
}

int irq_install_handler(int irq, irq_handler_t handler, void *dev) {
    if (irq < 0 || irq >= MAX_IRQS || !handler) return -1;
    if (irq_table[irq].handler) return -2;
    
    irq_table[irq].handler = handler;
    irq_table[irq].dev = dev;
    return 0;
}

void irq_uninstall_handler(int irq) {
    if (irq < 0 || irq >= MAX_IRQS) return;
    irq_table[irq].handler = NULL;
    irq_table[irq].dev = NULL;
}

void irq_dispatch(int irq) {
    if (irq < 0 || irq >= MAX_IRQS) return;
    
    if (irq_table[irq].handler) {
        irq_table[irq].handler(irq, irq_table[irq].dev);
    }
}

void irq_enable(int irq) {
    gic_enable_irq(irq);
}

void irq_disable(int irq) {
    gic_disable_irq(irq);
}
