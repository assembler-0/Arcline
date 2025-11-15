#include <drivers/serial.h>
#include <dtb.h>
#include <version.h>

void kmain(void) {
    serial_init();
    serial_puts(KERNEL_NAME);
    serial_puts("\ncopyright (C) 2025 assembler-0");
    serial_puts("\nbuild ");
    serial_puts(KERNEL_BUILD_DATE);
    serial_puts("\n");

    // Initialize and dump DTB info
    dtb_init();
    dtb_dump_info();

    // Loop forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
