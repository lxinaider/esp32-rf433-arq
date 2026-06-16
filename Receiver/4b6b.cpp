#include "4b6b.h"

uint8_t dec4b6b(uint8_t symbol) {
  for (uint8_t i = 0; i < 16; i++)
    if (encode_table[i] == symbol)
      return i;

  return 0xFF; // símbolo inválido
}

void enc4b6b(uint8_t byte, uint8_t &hi, uint8_t &lo) {
  hi = encode_table[byte >> 4];
  lo = encode_table[byte & 0x0F];
}
