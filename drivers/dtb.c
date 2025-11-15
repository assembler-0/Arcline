#include <dtb.h>
#include <drivers/serial.h>

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
        serial_puts("DTB: Checking ");
        serial_print_hex(locations[i]);
        serial_puts(" = ");
        serial_print_hex(*ptr);
        serial_puts("\n");
        
        if (be32_to_cpu(*ptr) == 0xd00dfeed) {
            return locations[i];
        }
    }
    
    // Scan memory from kernel end
    extern char _kernel_end[];
    uint64_t start = ((uint64_t)_kernel_end + 0xfffff) & ~0xfffff; // Align to 1MB
    uint64_t end = 0x50000000; // Search up to 256MB from RAM start
    
    serial_puts("DTB: Scanning from ");
    serial_print_hex(start);
    serial_puts(" to ");
    serial_print_hex(end);
    serial_puts("\n");
    
    for (uint64_t addr = start; addr < end; addr += 0x1000) { // 4KB steps
        uint32_t *ptr = (uint32_t *)addr;
        if (be32_to_cpu(*ptr) == 0xd00dfeed) {
            return addr;
        }
    }
    
    return 0;
}

void dtb_init(void) {
    serial_puts("DTB: boot_x0 = ");
    serial_print_hex(boot_x0);
    serial_puts(", dtb_ptr = ");
    serial_print_hex(dtb_ptr);
    serial_puts("\n");
    
    // Try using boot_x0 if dtb_ptr is 0
    uint64_t dtb_addr = dtb_ptr ? dtb_ptr : boot_x0;
    
    // If still no DTB, search for it
    if (!dtb_addr) {
        serial_puts("DTB: Searching for DTB in memory...\n");
        dtb_addr = dtb_search();
    }
    
    if (!dtb_addr) {
        serial_puts("DTB: No DTB found\n");
        return;
    }
    
    // Update dtb_ptr for other functions
    dtb_ptr = dtb_addr;
    
    struct dtb_header *hdr = (struct dtb_header *)dtb_addr;
    
    // Check magic number
    if (be32_to_cpu(hdr->magic) != 0xd00dfeed) {
        serial_puts("DTB: Invalid magic number: ");
        serial_print_hex(be32_to_cpu(hdr->magic));
        serial_puts("\n");
        return;
    }
    
    serial_puts("DTB: Found valid device tree at ");
    serial_print_hex(dtb_addr);
    serial_puts("\n");
}

void dtb_dump_info(void) {
    if (!dtb_ptr) {
        serial_puts("DTB: No DTB available\n");
        return;
    }
    
    struct dtb_header *hdr = (struct dtb_header *)dtb_ptr;
    
    serial_puts("DTB Info:\n");
    serial_puts("  Magic: ");
    serial_print_hex(be32_to_cpu(hdr->magic));
    serial_puts("\n  Total size: ");
    serial_print_hex(be32_to_cpu(hdr->totalsize));
    serial_puts("\n  Version: ");
    serial_print_hex(be32_to_cpu(hdr->version));
    serial_puts("\n");
}

struct dtb_header* dtb_get() {
    return (struct dtb_header *)dtb_ptr;
}