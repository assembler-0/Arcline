#include <stddef.h>
#include <drivers/timer.h>
#include <drivers/gic.h>
#include <kernel/irq.h>
#include <kernel/printk.h>

#define TIMER_IRQ 30

static volatile uint64_t jiffies = 0;
static uint32_t timer_freq = 0;

static inline uint64_t read_cntpct(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

static inline uint64_t read_cntfrq(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

static inline void write_cntp_tval(uint32_t val) {
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"((uint64_t)val));
}

static inline void write_cntp_ctl(uint32_t val) {
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"((uint64_t)val));
}

static void timer_irq_handler(int irq, void *dev) {
    (void)irq;
    (void)dev;
    
    jiffies++;
    
    write_cntp_tval(timer_freq / 100);
}

void timer_init(uint32_t freq_hz) {
    uint64_t cntfrq = read_cntfrq();
    timer_freq = (uint32_t)cntfrq;
    
    printk("Timer: frequency %u Hz, target %u Hz\n", timer_freq, freq_hz);
    
    irq_install_handler(TIMER_IRQ, timer_irq_handler, NULL);
    
    write_cntp_ctl(0);
    write_cntp_tval(timer_freq / freq_hz);
    write_cntp_ctl(1);
    
    irq_enable(TIMER_IRQ);
    
    printk("Timer: initialized\n");
}

uint64_t timer_get_ticks(void) {
    return jiffies;
}

void timer_udelay(uint32_t us) {
    uint64_t start = read_cntpct();
    uint64_t delta = ((uint64_t)us * timer_freq) / 1000000;
    while (read_cntpct() - start < delta);
}
