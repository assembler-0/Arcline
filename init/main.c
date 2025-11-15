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

    // Test different print functions
    printk("Testing printk: %d %x %p %s\n", 42, 0xdeadbeef, (void*)0x1234, "hello");
    fprintk(STDERR_FD, "Error message to stderr\n");

    // Loop forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
