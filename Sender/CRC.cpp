#include "CRC.h"
#include "Frame.h"

uint8_t crc8(const Frame &frame) {
  uint8_t crc = 0x00;
  const uint8_t *bytes = frame.bytes();

  // Passa por type, seq, len e data (bytes 1-19) e fcs
  for (uint8_t i = 1; i < 20; i++) {
    crc ^= bytes[i];
    for (int j = 0; j < 8; j++)
      // Algoritmo CRC com padrão 0x87
      crc = (crc & 0x80) ? (crc << 1) ^ 0x87 : (crc << 1);
  }

  return crc;
}
