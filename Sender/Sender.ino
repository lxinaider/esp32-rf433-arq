#include "Frame.h"
#include "CRC.h"
#include "4b6b.h"
#include "ARQ.h"

#define RX_PIN 16
#define TX_PIN 17
#define TX_VCC_PIN 33

const char *messages[] = {
  "Billie Jean is",
  "not my lover",
  "She's just",
  "a girl",
  "Who claims that",
  "I am the one",
  "But the kid",
  "is not my son"
};

const uint8_t custom_char[] = {
  0xFE, 0x00, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00
};

#define MSG_COUNT (sizeof(messages) / sizeof(messages[0]))  // = 8, só strings
#define TOTAL_FRAMES (MSG_COUNT + 1)                        // = 9, inclui custom_char

inline void txPowerOn() {
  digitalWrite(TX_VCC_PIN, HIGH);
  delay(100);
}

inline void txPowerOff() {
  digitalWrite(TX_VCC_PIN, LOW);
  delay(RX_SETTLE_MS);
}

bool readRawByte(uint8_t &out, uint32_t timeoutMs = TIMEOUT_MS) {
  uint32_t t0 = millis();
  while (!Serial2.available()) {
    if (millis() - t0 > timeoutMs) return false;
  }
  out = Serial2.read();
  return true;
}

bool read4b6b(uint8_t &out, uint32_t timeoutMs = TIMEOUT_MS) {
  uint8_t hi, lo;
  if (!readRawByte(hi, timeoutMs) || !readRawByte(lo, timeoutMs))
    return false;

  uint8_t a = dec4b6b(hi);
  uint8_t b = dec4b6b(lo);
  if (a == 0xFF || b == 0xFF)
    return false;

  out = (a << 4) | b;
  return true;
}

bool waitForFlag(uint32_t timeoutMs = TIMEOUT_MS) {
  uint32_t t0 = millis();
  uint8_t hi;

  if (!readRawByte(hi, timeoutMs)) return false;

  while (millis() - t0 < timeoutMs) {
    uint8_t lo;
    if (!readRawByte(lo, timeoutMs)) return false;

    uint8_t dhi = dec4b6b(hi);
    uint8_t dlo = dec4b6b(lo);

    if (dhi != 0xFF && dlo != 0xFF) {
      if (((dhi << 4) | dlo) == FLAG_BYTE) return true;
    }

    hi = lo;
  }

  return false;
}

void sendFrame(const Frame &frame) {
  txPowerOn();

  for (uint8_t i = 0; i < PREAMBLE_BYTES_TX; i++) {
    uint8_t hi, lo;
    enc4b6b(PREAMBLE_BYTE, hi, lo);
    Serial2.write(hi);
    Serial2.write(lo);
  }

  for (const uint8_t b : frame) {
    uint8_t hi, lo;
    enc4b6b(b, hi, lo);
    Serial2.write(hi);
    Serial2.write(lo);
  }

  Serial2.flush();
  txPowerOff();
}

uint8_t receiveACK() {
  if (!waitForFlag()) {
    Serial.println("[TX] Timeout aguardando flag do ACK");
    return 0xFF;
  }

  uint8_t type_raw, seq, len;
  if (!read4b6b(type_raw) || !read4b6b(seq) || !read4b6b(len)) {
    Serial.println("[TX] Timeout lendo header do ACK");
    return 0xFF;
  }

  if (type_raw != Type::ACK) {
    Serial.printf("[TX] Tipo inesperado: %d (esperava ACK)\n", type_raw);
    return 0xFF;
  }

  if (len != 0) {
    Serial.println("[TX] ACK com len != 0 — descartando");
    return 0xFF;
  }

  // Lê data[16] + fcs para poder validar o CRC completo
  Frame ack(Type::ACK, seq, 0);
  for (uint8_t i = 0; i < sizeof(Frame::data); i++) {
    if (!read4b6b(ack.data[i])) {
      Serial.println("[TX] Timeout lendo payload do ACK");
      return 0xFF;
    }
  }

  uint8_t fcs;
  if (!read4b6b(fcs)) {
    Serial.println("[TX] Timeout lendo FCS do ACK");
    return 0xFF;
  }

  ack.fcs = fcs;
  if (!ack.validate()) {
    Serial.printf("[TX] FCS inválido no ACK (seq=%d)\n", seq);
    return 0xFF;
  }

  return seq;
}

void sendWithARQ(const Frame &frame) {
  sendFrame(frame);
  Serial.printf("[TX] Enviado seq=%d  tipo=%d  len=%d\n", frame.seq, frame.type, frame.len);

  uint32_t t0 = millis();
  while (millis() - t0 < TIMEOUT_MS) {
    if (!Serial2.available()) continue;

    uint8_t ackSeq = receiveACK();

    if (ackSeq == 0xFF) {
      Serial.println("[TX] ACK inválido — aguardando novo ACK");
      continue;
    }

    if (ackSeq == frame.seq) {
      Serial.printf("[TX] ACK confirmado seq=%d\n", ackSeq);
      return;
    }

    Serial.printf("[TX] ACK duplicado seq=%d (esperava %d) — ignorando\n",
                  ackSeq, frame.seq);
  }
}

Frame makeDataFrame(uint8_t msgIdx, uint8_t seq) {
  if (msgIdx < MSG_COUNT) {
    const char *msg = messages[msgIdx];
    return Frame(Type::DATA, seq,
                 (uint8_t)strlen(msg),
                 (const uint8_t *)msg);
  }
  // Último frame: caractere customizado para o LCD
  return Frame(Type::DATA, seq,
               (uint8_t)sizeof(custom_char),
               custom_char);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(1200, SERIAL_8N1, RX_PIN, TX_PIN);

  pinMode(TX_VCC_PIN, OUTPUT);
  txPowerOff();

  for (uint8_t i = 0; i < MSG_COUNT; i++) {
    if (strlen(messages[i]) > sizeof(Frame::data)) {
      Serial.printf("[TX] ERRO: mensagem[%d] muito longa (%d > %d)\n",
                    i, strlen(messages[i]), sizeof(Frame::data));
      while (true) delay(1000);
    }
  }

  Serial.println("[TX] Transmissor pronto");
}

void loop() {
  uint8_t seq = 0;

  Serial.println("[TX] Iniciando transmissão...");

  for (uint8_t i = 0; i < TOTAL_FRAMES; i++) {
    Frame frame = makeDataFrame(i, seq);

    sendWithARQ(frame);

    seq ^= 1;
  }

  Serial.println("[TX] Enviando END...");
  Frame end_frame(Type::END, 0, 0);
  sendFrame(end_frame);
  delay(200);

  Serial.println("[TX] Transmissão concluída. Aguardando 10s para reiniciar...");
  delay(10000);
}
