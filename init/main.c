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

    void* a = pmm_alloc_page();
    void* b = pmm_alloc_pages(2);
    printk("PMM: alloc a=%p, b=%p\n", a, b);
    printk("PMM: free now=%d pages\n", (int)pmm_free_pages_count());
    if (a) pmm_free_page(a);
    if (b) pmm_free_pages(b, 2);
    printk("PMM: after free=%d pages\n", (int)pmm_free_pages_count());


    // Loop forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
