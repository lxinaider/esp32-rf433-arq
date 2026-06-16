#ifndef FRAME_H
#define FRAME_H

// Marca o início de todo frame
#define FLAG_BYTE 0x7E

#include <cstdint>
#include <cstring>

#include "4b6b.h"
#include "CRC.h"

enum Type : uint8_t {
  DATA = 0, // frame com dados
  ACK = 1, // confirmação de recebimento
  END = 2, // sinaliza fim da transmissão
};

// Layout do frame: [flag | type | seq | len | data[16] | fcs] = 21 bytes no total
struct Frame {
  uint8_t flag; // sempre 0x7E
  Type type; // DATA, ACK ou END
  uint8_t seq; // número de sequência, alterna entre 0 e 1
  uint8_t len; // quantos bytes do data[] são válidos
  uint8_t data[16]; // payload
  uint8_t fcs; // CRC-8 calculado sobre type+seq+len+data

  // Constrói um frame e já calcula o CRC
  Frame(Type type, uint8_t seq, uint8_t len, const uint8_t *payload = nullptr)
      : flag(FLAG_BYTE), type(type), seq(seq), len(len), fcs(0) {
    memset(this->data, 0, sizeof(this->data));
    if (payload != nullptr && len > 0)
      memcpy(this->data, payload, len);
    this->fcs = crc8(*this);
  }

  // Construtor pra frames sem dados
  Frame(Type type) : flag(FLAG_BYTE), type(type), seq(0), len(0), fcs(0) {
    memset(this->data, 0, sizeof(this->data));
  }

  // Frame vazio
  Frame() : flag(FLAG_BYTE), type(Type::DATA), seq(0), len(0), fcs(0) {
    memset(this->data, 0, sizeof(this->data));
  }

  // Acessa frame como array de bytes.
  uint8_t *bytes() { return reinterpret_cast<uint8_t *>(this); }
  const uint8_t *bytes() const { return reinterpret_cast<const uint8_t *>(this); }

  // Calcula o FCS e verifica se o resto é 0
  bool validate() const { return crc8(*this) == 0; }

  // Permite iterar o frame com for-range: `for (uint8_t b : frame)`
  const uint8_t *begin() const { return bytes(); }
  const uint8_t *end() const {
    return bytes() + sizeof(flag) + sizeof(type) + sizeof(seq) + sizeof(len) +
           sizeof(data) + sizeof(fcs);
  }
};

#endif