#include <drivers/gic.h>
#include <drivers/serial.h>
#include <drivers/timer.h>
#include <dtb.h>
#include <kernel/exception.h>
#include <kernel/irq.h>
#include <kernel/panic.h>
#include <kernel/printk.h>
#include <kernel/sched/task.h>
#include <mm/memtest.h>
#include <mm/mmu.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <version.h>

void proc(int argc, char **argv, char **envp) {
    (void)envp;
    while (1) {
        printk("Task %s: ", argv[0]);
        for (int i = 1; i < argc; i++) {
            printk("%s ", argv[i]);
        }
        printk("\n");
        for (volatile int i = 0; i < 1000000; i++)
            ; // Simple delay
    }
}

void kmain(void) {
    serial_init();
    printk_init();

    printk("%s\n", KERNEL_NAME);
    printk("%s\n", KERNEL_COPYRIGHT);
    printk("build %s\n", KERNEL_BUILD_DATE);

    // Initialize and dump DTB info
    dtb_init();
    dtb_dump_info();

    // Initialize Physical Memory Manager from DTB and run a small smoke test
    pmm_init_from_dtb();
    printk("PMM: total=%d pages, free=%d pages (size=%d KiB)\n",
           (int)pmm_total_pages(), (int)pmm_free_pages_count(),
           (int)(pmm_free_pages_count() * 4));

    // Optional: consistency check
    if (pmm_check() != 0) {
        panic("PMM consistency check failed");
    }
    printk("PMM: consistency check OK\n");

    // Initialize VMM structures (RB-tree VMAs)
    vmm_init_identity();
    if (vmm_init() != 0) {
        panic("VMM initialization failed");
    }
    printk("VMM: initialized RB-tree manager\n");

    // Initialize and enable MMU
    mmu_init();
    mmu_enable();
    mmu_switch_to_higher_half();

    // Map all available physical memory to higher-half
    uint64_t mem_size = pmm_total_pages() * 4096;
    uint64_t attrs = PTE_PAGE | PTE_SH_INNER | PTE_ATTR_IDX(MAIR_IDX_NORMAL);
    if (mmu_map_region(0, mem_size, attrs) == 0) {
        printk("MMU: mapped %d MiB physical memory to higher-half\n",
               (int)(mem_size / (1024 * 1024)));
    }

    // Run memory tests
    if (memtest_run() != 0) {
        panic("Memory tests failed");
    }

    // Initialize interrupt subsystem
    exception_init();
    irq_init();
    gic_init();
    timer_init(100);

    vmm_dump();

    task_init();

    task_args args1 = {
        .argc = 2, .argv = (char *[]){"TaskA", "Hello"}, .envp = NULL};
    task_create(proc, 0, &args1);

    task_args args2 = {
        .argc = 2, .argv = (char *[]){"TaskB", "World"}, .envp = NULL};
    task_create(proc, 0, &args2);

    task_args args3 = {
        .argc = 2, .argv = (char *[]){"TaskC", "From Gemini"}, .envp = NULL};
    task_create(proc, 0, &args3);


    printk("Created test tasks\n");

    printk("\nIRQ: enabling interrupts...\n");
    __asm__ volatile("msr daifclr, #2" ::: "memory");

    // Loop forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
