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
        fprintk(STDERR_FD,"PMM: consistency check OK\n");
    } else {
        printk("PMM: consistency check FAILED\n");
    }

    // Initialize minimal VMM identity layer and demonstrate vaddr->paddr
    vmm_init_identity();
    {
        uint64_t va = (uint64_t)&kmain;
        uint64_t pa = 0;
        if (vmm_virt_to_phys(va, &pa) == 0) {
            printk("VMM: v2p %p -> %p (identity)\n", (void*)va, (void*)pa);
        }
    }

    // Loop forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
