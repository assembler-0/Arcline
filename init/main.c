#include <drivers/serial.h>

void kmain(void) {
    serial_init();
    serial_puts("SERIAL: Initialized\n");

    // Loop forever
    while (1) {
        serial_puts("c");
        __asm__ volatile("wfe");
    }
}