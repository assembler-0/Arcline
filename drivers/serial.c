#include <drivers/serial.h>
#include <kernel/types.h>

// PL011 UART registers for QEMU virt machine
#define UART_BASE ((uint8_t*)0x09000000)

// See ARM PL011 Technical Reference Manual
#define UARTDR    (volatile uint32_t*)(UART_BASE + 0x000) // Data Register
#define UARTFR    (volatile uint32_t*)(UART_BASE + 0x018) // Flag Register

// Flag Register bits
#define UARTFR_TXFF (1 << 5) // Transmit FIFO full

void serial_init() {
    // For QEMU's PL011, it's usually ready to go.
    // On real hardware, we'd configure baud rate, parity, etc.
}

void serial_putc(char c) {
    // Wait for the transmit FIFO to have space.
    while (*UARTFR & UARTFR_TXFF);

    // Write the character to the data register.
    *UARTDR = c;
}

void serial_puts(const char *s) {
    for (size_t i = 0; s[i] != '\0'; i++) {
        serial_putc(s[i]);
    }
}
