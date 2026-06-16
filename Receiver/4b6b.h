#ifndef _4B6B_H
#define _4B6B_H

#include <cstdint>

static const uint8_t encode_table[] = {0x0D, 0x0E, 0x13, 0x15, 0x16, 0x19,
                                       0x1A, 0x1C, 0x23, 0x25, 0x26, 0x29,
                                       0x2A, 0x2C, 0x32, 0x34};

uint8_t dec4b6b(uint8_t symbol);
void enc4b6b(uint8_t byte, uint8_t &hi, uint8_t &lo);

#endif
