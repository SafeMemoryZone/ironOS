#include <pmio/ports.h>
#include <stdint.h>

// Read a byte from the specified port
uint8_t port_byte_in(uint16_t port) {
    uint8_t result;
    __asm__("inb %1, %0" : "=a" (result) : "d" (port));
    return result;
}

// Write a byte to the specified port
void port_byte_out(uint16_t port, uint8_t data) {
    __asm__("outb %0, %1" : : "a" (data), "d" (port));
}

// Read a word
uint16_t port_word_in(uint16_t port) {
    uint16_t result;
    __asm__("inw %1, %0" : "=a" (result) : "d" (port));
    return result;
}

// Write a word
void port_word_out(uint16_t port, uint16_t data) {
    __asm__("outw %0, %1" : : "a" (data), "d" (port));
}

void io_wait(void) {
    port_byte_out(0x80, 0);
}
