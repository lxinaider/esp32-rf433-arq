#ifndef CRC_H
#define CRC_H

#include <cstdint>

struct Frame;

uint8_t crc8(const Frame &frame);

#endif

