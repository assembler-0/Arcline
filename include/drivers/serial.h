#ifndef _DRIVERS_SERIAL_H
#define _DRIVERS_SERIAL_H

void serial_init();
void serial_putc(char c);
void serial_puts(const char *s);

#endif // _DRIVERS_SERIAL_H
