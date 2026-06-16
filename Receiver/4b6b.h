#ifndef _4B6B_H
#define _4B6B_H
 
#include <cstdint>

// Mapeia 4 bits pra um símbolo de 6 bits pro módulo RF não perder o sinal durante a transmissão.
static const uint8_t encode_table[] = {
  0x0D, 0x0E, 0x13, 0x15, 0x16, 0x19,
  0x1A, 0x1C, 0x23, 0x25, 0x26, 0x29,
  0x2A, 0x2C, 0x32, 0x34
};

// Decodifica um símbolo 6b de volta pro nibble original
uint8_t dec4b6b(uint8_t symbol);

// Codifica um byte em dois símbolos 6b: hi = primeiros 4 bits, lo = últimos 4 bits
void enc4b6b(uint8_t byte, uint8_t &hi, uint8_t &lo);
 
#endif
 