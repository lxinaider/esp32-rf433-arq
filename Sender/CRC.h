#ifndef CRC_H
#define CRC_H

#include <cstdint>

struct Frame;

// Calcula o CRC-8 do frame pra detectar erros de transmissão
uint8_t crc8(const Frame &frame);

#endif