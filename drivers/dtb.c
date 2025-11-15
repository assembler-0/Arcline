#include <dtb.h>
#include <kernel/printk.h>

// Convert big-endian to little-endian
static uint32_t be32_to_cpu(uint32_t val) {
    return ((val & 0xFF) << 24) | 
           ((val & 0xFF00) << 8) | 
           ((val & 0xFF0000) >> 8) | 
           ((val & 0xFF000000) >> 24);
}

// Search for DTB in memory
static uint64_t dtb_search(void) {
    // Common QEMU virt DTB locations to try
    static const uint64_t locations[5] = {
        0x48000000,  // 128MB from RAM start
        0x7ff00000,  // End of 1GB - 1MB
        0x44000000,  // 64MB from RAM start
        0x50000000,  // 256MB from RAM start
        0
    };
    
    // Try fixed locations first
    for (int i = 0; locations[i] != 0; i++) {
        uint32_t *ptr = (uint32_t *)locations[i];
        printk("DTB: Checking %p = %x\n", (void*)locations[i], *ptr);
        
        if (be32_to_cpu(*ptr) == 0xd00dfeed) {
            return locations[i];
        }
    }
    
    // Scan memory from kernel end
    extern char _kernel_end[];
    uint64_t start = ((uint64_t)_kernel_end + 0xfffff) & ~0xfffff; // Align to 1MB
    uint64_t end = 0x50000000; // Search up to 256MB from RAM start
    
    printk("DTB: Scanning from %p to %p\n", (void*)start, (void*)end);
    
    for (uint64_t addr = start; addr < end; addr += 0x1000) { // 4KB steps
        uint32_t *ptr = (uint32_t *)addr;
        if (be32_to_cpu(*ptr) == 0xd00dfeed) {
            return addr;
        }
    }
    
    return 0;
}

void dtb_init(void) {
    printk("DTB: boot_x0 = %p, dtb_ptr = %p\n", (void*)boot_x0, (void*)dtb_ptr);
    
    // Try using boot_x0 if dtb_ptr is 0
    uint64_t dtb_addr = dtb_ptr ? dtb_ptr : boot_x0;
    
    // If still no DTB, search for it
    if (!dtb_addr) {
        printk("DTB: Searching for DTB in memory...\n");
        dtb_addr = dtb_search();
    }
    
    if (!dtb_addr) {
        printk("DTB: No DTB found\n");
        return;
    }
    
    // Update dtb_ptr for other functions
    dtb_ptr = dtb_addr;
    
    struct dtb_header *hdr = (struct dtb_header *)dtb_addr;
    
    // Check magic number
    if (be32_to_cpu(hdr->magic) != 0xd00dfeed) {
        printk("DTB: Invalid magic number: %x\n", be32_to_cpu(hdr->magic));
        return;
    }
    
    printk("DTB: Found valid device tree at %p\n", (void*)dtb_addr);
}

void dtb_dump_info(void) {
    if (!dtb_ptr) {
        printk("DTB: No DTB available\n");
        return;
    }
    
    struct dtb_header *hdr = (struct dtb_header *)dtb_ptr;
    
    printk("DTB Info:\n");
    printk("  Magic: %x\n", be32_to_cpu(hdr->magic));
    printk("  Total size: %x\n", be32_to_cpu(hdr->totalsize));
    printk("  Version: %x\n", be32_to_cpu(hdr->version));
}

struct dtb_header* dtb_get() {
    return (struct dtb_header *)dtb_ptr;
}