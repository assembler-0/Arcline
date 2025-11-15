#include <kernel/types.h>
#include <drivers/serial.h>

// The C entry point for the kernel. 
void kmain(void) {
    serial_init();
    serial_puts("Hello from Arcline Kernel!\n");

    // Loop forever.
    while (1) {
        // On a real system, we'd enter a low-power state.
        // The 'wfe' in boot.S handles this if we return,
        // but a kernel's main function should not return.
    }
}