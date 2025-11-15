#include <drivers/serial.h>
#include <dtb.h>
#include <kernel/printk.h>
#include <version.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/mmu.h>

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

    // Initialize VMM structures (RB-tree VMAs)
    vmm_init_identity();
    if (vmm_init() == 0) {
        printk("VMM: initialized RB-tree manager\n");
    } else {
        printk("VMM: init failed\n");
        return;
    }

    // Initialize and enable MMU
    mmu_init();
    mmu_enable();
    mmu_switch_to_higher_half();

    // Map all available physical memory to higher-half
    uint64_t mem_size = pmm_total_pages() * 4096;
    uint64_t attrs = PTE_PAGE | PTE_SH_INNER | PTE_ATTR_IDX(MAIR_IDX_NORMAL);
    if (mmu_map_region(0, mem_size, attrs) == 0) {
        printk("MMU: mapped %d MiB physical memory to higher-half\n", (int)(mem_size / (1024*1024)));
    }

    printk("MMU: TTBR0=%p TTBR1=%p\n", (void*)mmu_get_ttbr0(), (void*)mmu_get_ttbr1());
    
    // Test higher-half mapping
    void *test_page = pmm_alloc_page();
    if (test_page) {
        uint64_t test_va = vmm_kernel_base() + 0x10000000ULL;
        if (vmm_map(test_va, (uint64_t)test_page, 4096, VMM_ATTR_R | VMM_ATTR_W | VMM_ATTR_NORMAL) == 0) {
            printk("VMM: test mapping VA=%p -> PA=%p\n", (void*)test_va, test_page);
        }
    }
    
    vmm_dump();

    // Loop forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
