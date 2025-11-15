#ifndef ARCLINE_DRIVERS_GIC_H
#define ARCLINE_DRIVERS_GIC_H

#include <stdint.h>

void gic_init(void);
void gic_enable_irq(int irq);
void gic_disable_irq(int irq);
uint32_t gic_ack_irq(void);
void gic_eoi_irq(uint32_t irq);

#endif // ARCLINE_DRIVERS_GIC_H
