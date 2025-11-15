#include <kernel/panic.h>
#include <kernel/printk.h>
#include <stdarg.h>
#include <stdint.h>

static void shutdown_system(void) {
    // Disable interrupts
    __asm__ volatile("msr daifset, #0xF" ::: "memory");
    
    // Attempt PSCI shutdown (QEMU/real hardware)
    register uint64_t x0 __asm__("x0") = 0x84000008; // PSCI_SYSTEM_OFF
    __asm__ volatile("hvc #0" :: "r"(x0) : "memory");
    
    // If PSCI fails, try QEMU semihosting exit
    register uint64_t reason __asm__("x1") = 0x18; // ADP_Stopped_ApplicationExit
    __asm__ volatile(
        "mov x0, #0x18\n"  // SYS_EXIT
        "hlt #0xF000\n"
        :: "r"(reason) : "x0", "memory"
    );
}

void __panic(const char *file, int line, const char *func, const char *fmt, ...) {
    __asm__ volatile("msr daifset, #0xF" ::: "memory");

    fprintk(STDERR_FD, "\nPanic: fatal - kernel panic - not syncing\n");
    fprintk(STDERR_FD, "Location: %s:%d\n", file, line);
    fprintk(STDERR_FD, "Function: %s\n", func);
    fprintk(STDERR_FD, "Info: ");
    
    va_list args;
    va_start(args, fmt);
    vfprintk(STDERR_FD, fmt, args);
    va_end(args);

    fprintk(STDERR_FD, "\nSystem halted.\n");
    
    shutdown_system();
    
    while(1) {
        __asm__ volatile("wfi");
    }
}
