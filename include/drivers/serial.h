#ifndef _DRIVERS_SERIAL_H
#define _DRIVERS_SERIAL_H
#include <stdint.h>

void serial_init();
void serial_putc(char c);
void serial_puts(const char *s);
void serial_print_hex(uint64_t val);

#endif // _DRIVERS_SERIAL_H
