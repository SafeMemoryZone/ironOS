#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

// Read a byte from the specified port
uint8_t port_byte_in(uint16_t port);

// Write a byte to the specified port
void port_byte_out(uint16_t port, uint8_t data);

// Read a word from the specified port
uint16_t port_word_in(uint16_t port);

// Write a word to the specified port
void port_word_out(uint16_t port, uint16_t data);

// Small delay to prevent race conditions on old hardware
void io_wait(void);

#endif
