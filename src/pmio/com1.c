#include <pmio/ports.h>
#include <stdint.h>

#define COM1 0x3f8

int init_com1() {
    port_byte_out(COM1 + 1, 0x00);    // Disable all interrupts
    port_byte_out(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    port_byte_out(COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    port_byte_out(COM1 + 1, 0x00);    //                  (hi byte)
    port_byte_out(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    port_byte_out(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    port_byte_out(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    port_byte_out(COM1 + 4, 0x1E);    // Set in loopback mode, test the serial chip
    port_byte_out(COM1 + 0, 0xAE);    // Test serial chip (send byte 0xAE)

    // Check if serial is faulty (i.e: not same byte as sent)
    if (port_byte_in(COM1 + 0) != 0xAE) {
        return 1;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    port_byte_out(COM1 + 4, 0x0F);
    return 0;
}

// If the hardware is ready to accept another byte
static int is_transmit_empty() {
    return port_byte_in(COM1 + 5) & 0x20;
}

// Write single character
void write_com1(char a) {
    // Wait until hardware is ready
    while (is_transmit_empty() == 0);

    port_byte_out(COM1, a);
}
