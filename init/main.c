#include <drivers/serial.h>
#include <dtb.h>
#include <kernel/printk.h>
#include <version.h>

void kmain(void) {
    serial_init();
    printk_init();

    printk("%s\n", KERNEL_NAME);
    printk("copyright (C) 2025 assembler-0\n");
    printk("build %s\n", KERNEL_BUILD_DATE);

    // Initialize and dump DTB info
    dtb_init();
    dtb_dump_info();


    // Loop forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
