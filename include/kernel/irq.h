#ifndef ARCLINE_KERNEL_IRQ_H
#define ARCLINE_KERNEL_IRQ_H

#include <stdint.h>

#define MAX_IRQS 1024

typedef void (*irq_handler_t)(int irq, void *dev);

void irq_init(void);
int irq_install_handler(int irq, irq_handler_t handler, void *dev);
void irq_uninstall_handler(int irq);
void irq_enable(int irq);
void irq_disable(int irq);
void irq_dispatch(int irq);

#endif // ARCLINE_KERNEL_IRQ_H
