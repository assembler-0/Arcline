#include <drivers/serial.h>
#include <dtb.h>
#include <kernel/printk.h>
#include <version.h>
#include <mm/pmm.h>

void kmain(void) {
    serial_init();
    printk_init();

    printk("%s\n", KERNEL_NAME);
    printk("copyright (C) 2025 assembler-0\n");
    printk("build %s\n", KERNEL_BUILD_DATE);

    // Initialize and dump DTB info
    dtb_init();
    dtb_dump_info();

    // Initialize Physical Memory Manager from DTB and run a small smoke test
    pmm_init_from_dtb();
    printk("PMM: total=%d pages, free=%d pages (size=%d KiB)\n",
           (int)pmm_total_pages(), (int)pmm_free_pages_count(), (int)(pmm_free_pages_count() * 4));


    // Loop forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
