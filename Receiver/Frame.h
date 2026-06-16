#ifndef FRAME_H
#define FRAME_H

#include <cstdint>
#include <cstring>

#include "4b6b.h"
#include "CRC.h"

enum Type : uint8_t {
  DATA = 0,
  ACK = 1,
  END = 2,
};

struct Frame {
  uint8_t flag;
  Type type;
  uint8_t seq;
  uint8_t len;
  uint8_t data[16];
  uint8_t fcs;

  Frame(uint8_t flag, Type type, uint8_t seq, uint8_t len,
        const uint8_t *payload = nullptr)
      : flag(flag), type(type), seq(seq), len(len), fcs(0) {
    memset(this->data, 0, sizeof(this->data));
    if (payload != nullptr && len > 0)
      memcpy(this->data, payload, len);
    this->fcs = crc8(*this);
  }

  Frame(Type type) : flag(0x7E), type(type), seq(0), len(0), fcs(0) {
    memset(this->data, 0, sizeof(this->data));
  }

  uint8_t *bytes() { return reinterpret_cast<uint8_t *>(this); }
  const uint8_t *bytes() const {
    return reinterpret_cast<const uint8_t *>(this);
  }

  bool validate() const { return crc8(*this) == fcs; }

  const uint8_t *begin() const { return bytes(); }
  const uint8_t *end() const {
    return bytes() + sizeof(flag) + sizeof(type) + sizeof(seq) + sizeof(len) +
           sizeof(data) + sizeof(fcs);
  }
};

#endif
