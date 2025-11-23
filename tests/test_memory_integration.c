#include <kernel/printk.h>
#include <kernel/sched/task.h>
#include <mm/pmm.h>
#include <mm/vmalloc.h>
#include <mm/vmm.h>
#include <string.h>

#define TEST_PATTERN_1 0xAA
#define TEST_PATTERN_2 0x55
#define TEST_PATTERN_3 0xFF
#define TEST_PATTERN_4 0x00

static int tests_passed = 0;
static int tests_failed = 0;

// Test 1: PMM Basic Allocation
static int test_pmm_basic(void) {
    printk("  [1/10] PMM basic allocation...");

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

// Test 2: PMM Write/Read Patterns
static int test_pmm_patterns(void) {
    printk("  [2/10] PMM write/read patterns...");

    void *page = pmm_alloc_page();
    if (!page) {
        printk(" FAIL (alloc)\n");
        return -1;
    }

    uint8_t *buf = (uint8_t *)page;

    // Test pattern 1
    memset(buf, TEST_PATTERN_1, 4096);
    for (int i = 0; i < 4096; i++) {
        if (buf[i] != TEST_PATTERN_1) {
            printk(" FAIL (pattern 1 at %d)\n", i);
            pmm_free_page(page);
            return -1;
        }
    }

    // Test pattern 2
    memset(buf, TEST_PATTERN_2, 4096);
    for (int i = 0; i < 4096; i++) {
        if (buf[i] != TEST_PATTERN_2) {
            printk(" FAIL (pattern 2 at %d)\n", i);
            pmm_free_page(page);
            return -1;
        }
    }

    pmm_free_page(page);
    printk(" PASS\n");
    return 0;
}

// Test 3: PMM Stress Test
static int test_pmm_stress(void) {
    printk("  [3/10] PMM stress test (128 pages)...");

#define STRESS_PAGES 128
    void *pages[STRESS_PAGES];

    // Allocate many pages
    for (int i = 0; i < STRESS_PAGES; i++) {
        pages[i] = pmm_alloc_page();
        if (!pages[i]) {
            printk(" FAIL (alloc %d)\n", i);
            for (int j = 0; j < i; j++)
                pmm_free_page(pages[j]);
            return -1;
        }
        // Write unique pattern to each page
        memset(pages[i], i & 0xFF, 4096);
    }

    // Verify all pages
    for (int i = 0; i < STRESS_PAGES; i++) {
        uint8_t *buf = (uint8_t *)pages[i];
        for (int j = 0; j < 4096; j++) {
            if (buf[j] != (uint8_t)(i & 0xFF)) {
                printk(" FAIL (verify page %d at offset %d)\n", i, j);
                for (int k = 0; k < STRESS_PAGES; k++)
                    pmm_free_page(pages[k]);
                return -1;
            }
        }
    }

    // Free all pages
    for (int i = 0; i < STRESS_PAGES; i++) {
        pmm_free_page(pages[i]);
    }

    printk(" PASS\n");
    return 0;
}

// Test 4: VMM Basic Mapping
static int test_vmm_basic(void) {
    printk("  [4/10] VMM basic mapping...");

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

// Test 5: VMM Permission Changes
static int test_vmm_protect(void) {
    printk("  [5/10] VMM permission changes...");

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

// Test 6: vmalloc Basic
static int test_vmalloc_basic(void) {
    printk("  [6/10] vmalloc basic (8KB)...");

    void *buf = vmalloc(8192);
    if (!buf) {
        printk(" FAIL (alloc)\n");
        return -1;
    }

    memset(buf, TEST_PATTERN_3, 8192);
    uint8_t *p = (uint8_t *)buf;
    for (int i = 0; i < 8192; i++) {
        if (p[i] != TEST_PATTERN_3) {
            printk(" FAIL (verify at %d)\n", i);
            vfree(buf, 8192);
            return -1;
        }
    }

    vfree(buf, 8192);
    printk(" PASS\n");
    return 0;
}

// Test 7: vmalloc Fragmentation
static int test_vmalloc_fragmentation(void) {
    printk("  [7/10] vmalloc fragmentation...");

    void *b1 = vmalloc(4096);
    void *b2 = vmalloc(8192);
    void *b3 = vmalloc(4096);

    if (!b1 || !b2 || !b3) {
        printk(" FAIL (alloc)\n");
        if (b1) vfree(b1, 4096);
        if (b2) vfree(b2, 8192);
        if (b3) vfree(b3, 4096);
        return -1;
    }

    // Free middle block
    vfree(b2, 8192);

    // Try to reallocate same size
    void *b4 = vmalloc(8192);
    if (!b4) {
        printk(" FAIL (realloc)\n");
        vfree(b1, 4096);
        vfree(b3, 4096);
        return -1;
    }

    vfree(b1, 4096);
    vfree(b3, 4096);
    vfree(b4, 8192);

    printk(" PASS\n");
    return 0;
}

// Test 8: Memory Isolation
static int test_memory_isolation(void) {
    printk("  [8/10] Memory isolation...");

    void *p1 = vmalloc(4096);
    void *p2 = vmalloc(4096);

    if (!p1 || !p2) {
        printk(" FAIL (alloc)\n");
        if (p1) vfree(p1, 4096);
        if (p2) vfree(p2, 4096);
        return -1;
    }

    memset(p1, 0xAA, 4096);
    memset(p2, 0x55, 4096);

    uint8_t *b1 = (uint8_t *)p1;
    uint8_t *b2 = (uint8_t *)p2;

    for (int i = 0; i < 4096; i++) {
        if (b1[i] != 0xAA || b2[i] != 0x55) {
            printk(" FAIL (isolation at %d)\n", i);
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

// Test 9: Large Allocation
static int test_large_allocation(void) {
    printk("  [9/10] Large allocation (64KB)...");

    void *buf = vmalloc(65536);
    if (!buf) {
        printk(" FAIL (alloc)\n");
        return -1;
    }

    // Write pattern
    uint32_t *words = (uint32_t *)buf;
    for (int i = 0; i < 65536 / 4; i++) {
        words[i] = 0xDEAD0000 | (i & 0xFFFF);
    }

    // Verify pattern
    for (int i = 0; i < 65536 / 4; i++) {
        if (words[i] != (0xDEAD0000 | (i & 0xFFFF))) {
            printk(" FAIL (verify at word %d)\n", i);
            vfree(buf, 65536);
            return -1;
        }
    }

    vfree(buf, 65536);
    printk(" PASS\n");
    return 0;
}

// Test 10: Concurrent Allocation (simulated)
static int test_concurrent_allocation(void) {
    printk("  [10/10] Concurrent allocation pattern...");

#define CONCURRENT_ALLOCS 32
    void *allocs[CONCURRENT_ALLOCS];
    size_t sizes[CONCURRENT_ALLOCS] = {
        4096, 8192, 4096, 16384, 4096, 8192, 4096, 12288,
        4096, 8192, 4096, 16384, 4096, 8192, 4096, 12288,
        4096, 8192, 4096, 16384, 4096, 8192, 4096, 12288,
        4096, 8192, 4096, 16384, 4096, 8192, 4096, 12288
    };

    // Allocate all
    for (int i = 0; i < CONCURRENT_ALLOCS; i++) {
        allocs[i] = vmalloc(sizes[i]);
        if (!allocs[i]) {
            printk(" FAIL (alloc %d)\n", i);
            for (int j = 0; j < i; j++)
                vfree(allocs[j], sizes[j]);
            return -1;
        }
        // Write unique pattern
        memset(allocs[i], i & 0xFF, sizes[i]);
    }

    // Verify all
    for (int i = 0; i < CONCURRENT_ALLOCS; i++) {
        uint8_t *buf = (uint8_t *)allocs[i];
        for (size_t j = 0; j < sizes[i]; j++) {
            if (buf[j] != (uint8_t)(i & 0xFF)) {
                printk(" FAIL (verify alloc %d at %zu)\n", i, j);
                for (int k = 0; k < CONCURRENT_ALLOCS; k++)
                    vfree(allocs[k], sizes[k]);
                return -1;
            }
        }
    }

    // Free all
    for (int i = 0; i < CONCURRENT_ALLOCS; i++) {
        vfree(allocs[i], sizes[i]);
    }

    printk(" PASS\n");
    return 0;
}

// Main test runner
int run_memory_integration_tests(void) {
    printk("\n");
    printk("========================================\n");
    printk("  MEMORY INTEGRATION TESTS\n");
    printk("========================================\n");
    printk("\n");

    size_t free_before = pmm_free_pages_count();
    tests_passed = 0;
    tests_failed = 0;

    if (test_pmm_basic() == 0) tests_passed++; else tests_failed++;
    if (test_pmm_patterns() == 0) tests_passed++; else tests_failed++;
    if (test_pmm_stress() == 0) tests_passed++; else tests_failed++;
    if (test_vmm_basic() == 0) tests_passed++; else tests_failed++;
    if (test_vmm_protect() == 0) tests_passed++; else tests_failed++;
    if (test_vmalloc_basic() == 0) tests_passed++; else tests_failed++;
    if (test_vmalloc_fragmentation() == 0) tests_passed++; else tests_failed++;
    if (test_memory_isolation() == 0) tests_passed++; else tests_failed++;
    if (test_large_allocation() == 0) tests_passed++; else tests_failed++;
    if (test_concurrent_allocation() == 0) tests_passed++; else tests_failed++;

    size_t free_after = pmm_free_pages_count();
    int leaked = (int)(free_before - free_after);

    printk("\n");
    printk("========================================\n");
    printk("  RESULTS: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    printk("========================================\n");
    printk("Memory: %d pages before, %d pages after", (int)free_before, (int)free_after);

    if (leaked > 0) {
        printk(" (LEAKED: %d pages)\n", leaked);
    } else if (leaked < 0) {
        printk(" (GAINED: %d pages - unexpected!)\n", -leaked);
    } else {
        printk(" (no leaks)\n");
    }

    vmalloc_stats();

    printk("\n");

    return tests_failed == 0 ? 0 : -1;
}
