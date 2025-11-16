#include <kernel/panic.h>
#include <kernel/printk.h>
#include <mm/pmm.h>
#include <mm/vmalloc.h>
#include <mm/vmm.h>
#include <string.h>

#define TEST_PATTERN_1 0xAA
#define TEST_PATTERN_2 0x55
#define TEST_PATTERN_3 0xFF
#define TEST_PATTERN_4 0x00

static int test_pmm_basic(void) {
    printk("  [1/8] PMM basic allocation...");

    void *p1 = pmm_alloc_page();
    void *p2 = pmm_alloc_page();
    void *p3 = pmm_alloc_page();

    if (!p1 || !p2 || !p3 || p1 == p2 || p2 == p3 || p1 == p3) {
        printk(" FAIL\n");
        return -1;
    }

    pmm_free_page(p1);
    pmm_free_page(p2);
    pmm_free_page(p3);

    printk(" PASS\n");
    return 0;
}

static int test_pmm_patterns(void) {
    printk("  [2/8] PMM write/read patterns...");

    void *page = pmm_alloc_page();
    if (!page) {
        printk(" FAIL (alloc)\n");
        return -1;
    }

    uint8_t *buf = (uint8_t *)page;

    memset(buf, TEST_PATTERN_1, 4096);
    for (int i = 0; i < 4096; i++) {
        if (buf[i] != TEST_PATTERN_1) {
            printk(" FAIL (pattern 1)\n");
            pmm_free_page(page);
            return -1;
        }
    }

    memset(buf, TEST_PATTERN_2, 4096);
    for (int i = 0; i < 4096; i++) {
        if (buf[i] != TEST_PATTERN_2) {
            printk(" FAIL (pattern 2)\n");
            pmm_free_page(page);
            return -1;
        }
    }

    pmm_free_page(page);
    printk(" PASS\n");
    return 0;
}

static int test_pmm_stress(void) {
    printk("  [3/8] PMM stress test...");

#define STRESS_PAGES 64
    void *pages[STRESS_PAGES];

    for (int i = 0; i < STRESS_PAGES; i++) {
        pages[i] = pmm_alloc_page();
        if (!pages[i]) {
            printk(" FAIL (alloc %d)\n", i);
            for (int j = 0; j < i; j++)
                pmm_free_page(pages[j]);
            return -1;
        }
        memset(pages[i], i & 0xFF, 4096);
    }

    for (int i = 0; i < STRESS_PAGES; i++) {
        uint8_t *buf = (uint8_t *)pages[i];
        for (int j = 0; j < 4096; j++) {
            if (buf[j] != (uint8_t)(i & 0xFF)) {
                printk(" FAIL (verify %d)\n", i);
                for (int k = 0; k < STRESS_PAGES; k++)
                    pmm_free_page(pages[k]);
                return -1;
            }
        }
    }

    for (int i = 0; i < STRESS_PAGES; i++) {
        pmm_free_page(pages[i]);
    }

    printk(" PASS\n");
    return 0;
}

static int test_vmm_basic(void) {
    printk("  [4/8] VMM basic mapping...");

    void *page = pmm_alloc_page();
    if (!page) {
        printk(" FAIL (alloc)\n");
        return -1;
    }

    uint64_t va = vmm_kernel_base() + 0x50000000ULL;
    int ret = vmm_map(va, (uint64_t)page, 4096,
                      VMM_ATTR_R | VMM_ATTR_W | VMM_ATTR_NORMAL);
    if (ret != 0) {
        printk(" FAIL (map)\n");
        pmm_free_page(page);
        return -1;
    }

    volatile uint32_t *ptr = (volatile uint32_t *)va;
    *ptr = 0xDEADBEEF;
    if (*ptr != 0xDEADBEEF) {
        printk(" FAIL (verify)\n");
        vmm_unmap(va, 4096);
        pmm_free_page(page);
        return -1;
    }

    vmm_unmap(va, 4096);
    pmm_free_page(page);

    printk(" PASS\n");
    return 0;
}

static int test_vmm_protect(void) {
    printk("  [5/8] VMM permission changes...");

    void *page = pmm_alloc_page();
    if (!page) {
        printk(" FAIL (alloc)\n");
        return -1;
    }

    uint64_t va = vmm_kernel_base() + 0x51000000ULL;
    vmm_map(va, (uint64_t)page, 4096,
            VMM_ATTR_R | VMM_ATTR_W | VMM_ATTR_NORMAL);

    volatile uint32_t *ptr = (volatile uint32_t *)va;
    *ptr = 0x12345678;

    vmm_protect(va, 4096, VMM_ATTR_R | VMM_ATTR_NORMAL | VMM_ATTR_PXN);

    if (*ptr != 0x12345678) {
        printk(" FAIL (read after protect)\n");
        vmm_unmap(va, 4096);
        pmm_free_page(page);
        return -1;
    }

    vmm_unmap(va, 4096);
    pmm_free_page(page);

    printk(" PASS\n");
    return 0;
}

static int test_vmalloc_basic(void) {
    printk("  [6/8] vmalloc basic...");

    void *buf = vmalloc(8192);
    if (!buf) {
        printk(" FAIL (alloc)\n");
        return -1;
    }

    memset(buf, TEST_PATTERN_3, 8192);
    uint8_t *p = (uint8_t *)buf;
    for (int i = 0; i < 8192; i++) {
        if (p[i] != TEST_PATTERN_3) {
            printk(" FAIL (verify)\n");
            vfree(buf, 8192);
            return -1;
        }
    }

    vfree(buf, 8192);
    printk(" PASS\n");
    return 0;
}

static int test_vmalloc_fragmentation(void) {
    printk("  [7/8] vmalloc fragmentation...");

    void *b1 = vmalloc(4096);
    void *b2 = vmalloc(8192);
    void *b3 = vmalloc(4096);

    if (!b1 || !b2 || !b3) {
        printk(" FAIL (alloc)\n");
        if (b1)
            vfree(b1, 4096);
        if (b2)
            vfree(b2, 8192);
        if (b3)
            vfree(b3, 4096);
        return -1;
    }

    vfree(b2, 8192);

    void *b4 = vmalloc(8192);
    if (!b4) {
        printk(" FAIL (realloc)\n");
        vfree(b1, 4096);
        vfree(b3, 4096);
        return -1;
    }

    if (b4 != b2) {
        printk(" WARN (no reuse)\n");
    }

    vfree(b1, 4096);
    vfree(b3, 4096);
    vfree(b4, 8192);

    return 0;
}

static int test_memory_isolation(void) {
    printk("  [8/8] Memory isolation...");

    void *p1 = vmalloc(4096);
    void *p2 = vmalloc(4096);

    if (!p1 || !p2) {
        printk(" FAIL (alloc)\n");
        if (p1)
            vfree(p1, 4096);
        if (p2)
            vfree(p2, 4096);
        return -1;
    }

    memset(p1, 0xAA, 4096);
    memset(p2, 0x55, 4096);

    uint8_t *b1 = (uint8_t *)p1;
    uint8_t *b2 = (uint8_t *)p2;

    for (int i = 0; i < 4096; i++) {
        if (b1[i] != 0xAA || b2[i] != 0x55) {
            printk(" FAIL (isolation)\n");
            vfree(p1, 4096);
            vfree(p2, 4096);
            return -1;
        }
    }

    vfree(p1, 4096);
    vfree(p2, 4096);

    printk(" PASS\n");
    return 0;
}

int memtest_run(void) {

    size_t free_before = pmm_free_pages_count();

    int failed = 0;

    if (test_pmm_basic() < 0)
        failed++;
    if (test_pmm_patterns() < 0)
        failed++;
    if (test_pmm_stress() < 0)
        failed++;
    if (test_vmm_basic() < 0)
        failed++;
    if (test_vmm_protect() < 0)
        failed++;
    if (test_vmalloc_basic() < 0)
        failed++;
    if (test_vmalloc_fragmentation() < 0)
        failed++;
    if (test_memory_isolation() < 0)
        failed++;

    size_t free_after = pmm_free_pages_count();
    int leaked = (int)(free_before - free_after);

    printk("\nResults: %d/8 tests passed\n", 8 - failed);
    printk("Memory: %d pages before, %d pages after", (int)free_before,
           (int)free_after);

    if (leaked > 0) {
        printk(" (diff: %d pages)\n", leaked);
    } else {
        printk(" (no leaks)\n");
    }

    vmalloc_stats();

    return failed == 0 ? 0 : -1;
}
