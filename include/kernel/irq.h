#ifndef ARCLINE_KERNEL_IRQ_H
#define ARCLINE_KERNEL_IRQ_H

#include <kernel/sched/task.h>
#include <stdint.h>

#define MAX_IRQS 1024

typedef void (*irq_handler_t)(cpu_context_t *ctx, int irq, void *dev);

void irq_init(void);
int irq_install_handler(int irq, irq_handler_t handler, void *dev);
void irq_uninstall_handler(int irq);
void irq_enable(int irq);
void irq_disable(int irq);
void irq_dispatch(cpu_context_t *ctx, int irq);

#endif // ARCLINE_KERNEL_IRQ_H
