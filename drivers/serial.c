#include <drivers/serial.h>
#include <dtb.h>
#include <kernel/types.h>

// Default PL011 UART base for QEMU virt machine (fallback)
#define DEFAULT_UART_BASE 0x09000000ULL

// Runtime UART base (may be overridden by DTB)
static uint64_t uart_base = DEFAULT_UART_BASE;

// PL011 UART register offsets
#define UARTDR() ((volatile uint32_t *)(uart_base + 0x000)) // Data Register
#define UARTFR() ((volatile uint32_t *)(uart_base + 0x018)) // Flag Register
#define UARTLCR_H()                                                            \
    ((volatile uint32_t *)(uart_base + 0x02C)) // Line Control Register
#define UARTCR() ((volatile uint32_t *)(uart_base + 0x030)) // Control Register

// Flag Register bits
#define UARTFR_TXFF (1 << 5) // Transmit FIFO full
#define UARTFR_BUSY (1 << 3) // UART busy

// Control Register bits
#define UARTCR_UARTEN (1 << 0) // UART enable
#define UARTCR_TXE (1 << 8)    // Transmit enable
#define UARTCR_RXE (1 << 9)    // Receive enable

// Line Control Register bits
#define UARTLCR_H_WLEN_8 (3 << 5) // 8-bit word length

// Memory barrier for ARM64
static inline void mb(void) { __asm__ volatile("dsb sy" ::: "memory"); }

void serial_init() {
    // Try to get UART base from DTB (chosen/stdout-path)
    uint64_t dtb_uart_base = 0;
    if (dtb_get_stdout_uart_base(&dtb_uart_base) == 0 && dtb_uart_base != 0) {
        uart_base = dtb_uart_base;
    }

    // Disable UART
    *UARTCR() = 0;
    mb();

    // Set 8-bit word length, no parity, 1 stop bit
    *UARTLCR_H() = UARTLCR_H_WLEN_8;
    mb();

    // Enable UART, transmit and receive
    *UARTCR() = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;
    mb();
}

// A very small spin timeout to avoid hard-lock if UART isn't ready
#define UART_SPIN_MAX 1000000

static inline void uart_wait_tx_space(void) {
    unsigned int spins = 0;
    while (*UARTFR() & UARTFR_TXFF) {
        if (++spins >= UART_SPIN_MAX) {
            // Give up waiting; avoid hanging the kernel
            break;
        }
        mb();
    }
}

void serial_putc(char c) {
    // Convert \n to \r\n for proper line endings
    if (c == '\n') {
        // Send \r first
        uart_wait_tx_space();
        *UARTDR() = (uint32_t)'\r';
        mb();
    }

    // Wait for the transmit FIFO to have space
    uart_wait_tx_space();

    // Write the character to the data register
    *UARTDR() = (uint32_t)c;
    mb();
}

void serial_puts(const char *s) {
    if (!s)
        return;

    while (*s) {
        serial_putc(*s++);
    }
}

void serial_print_hex(uint64_t val) {
    char buf[20];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';

    if (val == 0) {
        serial_puts("0x0");
        return;
    }

    while (val > 0) {
        int digit = val & 0xF;
        *--p = (digit < 10) ? '0' + digit : 'a' + digit - 10;
        val >>= 4;
    }

    serial_puts("0x");
    serial_puts(p);
}