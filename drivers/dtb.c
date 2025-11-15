#include <dtb.h>
#include <drivers/serial.h>

// Convert big-endian to little-endian
static uint32_t be32_to_cpu(uint32_t val) {
    return ((val & 0xFF) << 24) | 
           ((val & 0xFF00) << 8) | 
           ((val & 0xFF0000) >> 8) | 
           ((val & 0xFF000000) >> 24);
}

void dtb_init(void) {
    serial_puts("DTB: boot_x0 = ");
    serial_print_hex(boot_x0);
    serial_puts(", dtb_ptr = ");
    serial_print_hex(dtb_ptr);
    serial_puts("\n");
    
    // Try using boot_x0 if dtb_ptr is 0
    uint64_t dtb_addr = dtb_ptr ? dtb_ptr : boot_x0;
    
    if (!dtb_addr) {
        serial_puts("DTB: No DTB pointer found\n");
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