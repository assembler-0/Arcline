#include <drivers/serial.h>
#include <dtb.h>
#include <kernel/printk.h>
#include <version.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

void kmain(void) {
    serial_init();
    printk_init();

    printk("%s\n", KERNEL_NAME);
    printk("%s\n",KERNEL_COPYRIGHT);
    printk("build %s\n", KERNEL_BUILD_DATE);

    // Initialize and dump DTB info
    dtb_init();
    dtb_dump_info();

    // Initialize Physical Memory Manager from DTB and run a small smoke test
    pmm_init_from_dtb();
    printk("PMM: total=%d pages, free=%d pages (size=%d KiB)\n",
           (int)pmm_total_pages(), (int)pmm_free_pages_count(), (int)(pmm_free_pages_count() * 4));

    // Optional: consistency check
    if (pmm_check() == 0) {
        printk("PMM: consistency check OK\n");
    } else {
        printk("PMM: consistency check FAILED\n");
        return;
    }

    // Initialize VMM structures (RB-tree VMAs). MMU remains off for now.
    vmm_init_identity();
    if (vmm_init() == 0) {
        printk("VMM: initialized RB-tree manager\n");
    } else {
        printk("VMM: init failed\n");
        return;
    }

    // Dump VMAs (should be empty at this stage)
    vmm_dump();

    // Loop forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
