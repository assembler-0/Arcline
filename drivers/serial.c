#include <drivers/serial.h>
#include <kernel/types.h>

// PL011 UART registers for QEMU virt machine
#define UART_BASE 0x09000000UL

// PL011 UART register offsets
#define UARTDR    (volatile uint32_t*)(UART_BASE + 0x000) // Data Register
#define UARTFR    (volatile uint32_t*)(UART_BASE + 0x018) // Flag Register
#define UARTLCR_H (volatile uint32_t*)(UART_BASE + 0x02C) // Line Control Register
#define UARTCR    (volatile uint32_t*)(UART_BASE + 0x030) // Control Register

// Flag Register bits
#define UARTFR_TXFF (1 << 5) // Transmit FIFO full
#define UARTFR_BUSY (1 << 3) // UART busy

// Control Register bits
#define UARTCR_UARTEN (1 << 0) // UART enable
#define UARTCR_TXE    (1 << 8) // Transmit enable
#define UARTCR_RXE    (1 << 9) // Receive enable

// Line Control Register bits
#define UARTLCR_H_WLEN_8 (3 << 5) // 8-bit word length

// Memory barrier for ARM64
static inline void mb(void) {
    __asm__ volatile("dsb sy" ::: "memory");
}

void serial_init() {
    // Disable UART
    *UARTCR = 0;
    mb();
    
    // Set 8-bit word length, no parity, 1 stop bit
    *UARTLCR_H = UARTLCR_H_WLEN_8;
    mb();
    
    // Enable UART, transmit and receive
    *UARTCR = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;
    mb();
}

void serial_putc(char c) {
    // Wait for the transmit FIFO to have space
    while (*UARTFR & UARTFR_TXFF) {
        mb();
    }

    // Write the character to the data register
    *UARTDR = (uint32_t)c;
    mb();
    
    // Convert \n to \r\n for proper line endings
    if (c == '\n') {
        serial_putc('\r');
    }
}

void serial_puts(const char *s) {
    if (!s) return;
    
    while (*s) {
        serial_putc(*s++);
    }
}
