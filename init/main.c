#include <drivers/serial.h>
#include <dtb.h>

void kmain(void) {
    serial_init();
    serial_puts("Arcline(R) microkernel\n");
    serial_puts("copyright (C) 2025 assembler-0\n");
    
    // Initialize and dump DTB info
    dtb_init();
    dtb_dump_info();

    // Loop forever
    while (1) {
        __asm__ volatile("wfe");
    }
}