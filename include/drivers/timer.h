#ifndef ARCLINE_DRIVERS_TIMER_H
#define ARCLINE_DRIVERS_TIMER_H

#include <stdint.h>

void timer_init(uint32_t freq_hz);
uint64_t timer_get_ticks(void);
void timer_udelay(uint32_t us);
uint64_t read_cntpct();
uint64_t read_cntfrq();
uint64_t get_ns();
void delay(uint32_t ms);
void delay_sec(uint32_t seconds);

#endif // ARCLINE_DRIVERS_TIMER_H
