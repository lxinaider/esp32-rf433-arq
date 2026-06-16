#include "CRC.h"
#include "Frame.h"

uint8_t crc8(const Frame &frame) {
  uint8_t crc = 0x00;
  const uint8_t *bytes = frame.bytes();

  // bytes[1] até bytes[19]: type, seq, len, data[0..15]
  for (uint8_t i = 1; i < 20; i++) {
    crc ^= bytes[i];
    for (int j = 0; j < 8; j++)
      crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
  }

  return crc;
}

