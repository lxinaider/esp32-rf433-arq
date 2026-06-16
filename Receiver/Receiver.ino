#include <LiquidCrystal_I2C.h>
#include "Frame.h"
#include "CRC.h"
#include "4b6b.h"
#include "ARQ.h"

#define RX_PIN 16
#define TX_PIN 17
#define TX_VCC_PIN 33

#define LCD_COLS 16
#define LCD_ROWS 2
#define LCD_ADDR 0x27

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

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

void sendACK(uint8_t seq) {
  delay(RX_SETTLE_MS);
  txPowerOn();

  Frame ack(Type::ACK, seq, 0);

  for (uint8_t i = 0; i < PREAMBLE_BYTES_ACK; i++) {
    uint8_t hi, lo;
    enc4b6b(PREAMBLE_BYTE, hi, lo);
    Serial2.write(hi);
    Serial2.write(lo);
  }

  for (const uint8_t b : ack) {
    uint8_t hi, lo;
    enc4b6b(b, hi, lo);
    Serial2.write(hi);
    Serial2.write(lo);
  }

  Serial2.flush();
  txPowerOff();

  Serial.printf("[RX] ACK enviado seq=%d\n", seq);
}

RecvResult receiveFrame(Frame &frame) {
  if (!waitForFlag()) {
    return RecvResult::TIMEOUT;
  }

  uint8_t type_raw, seq, len;
  if (!read4b6b(type_raw) || !read4b6b(seq) || !read4b6b(len)) {
    Serial.println("[RX] Timeout lendo header, descartando");
    return RecvResult::DISCARD;
  }

  if (type_raw > Type::END) {
    Serial.printf("[RX] Tipo inválido: %d, descartando\n", type_raw);
    return RecvResult::DISCARD;
  }
  if (len > sizeof(Frame::data)) {
    Serial.printf("[RX] len inválido: %d, descartando\n", len);
    return RecvResult::DISCARD;
  }

  frame = Frame((Type)type_raw, seq, len);

  for (uint8_t i = 0; i < sizeof(Frame::data); i++) {
    if (!read4b6b(frame.data[i])) {
      Serial.println("[RX] Timeout lendo payload, descartando");
      return RecvResult::DISCARD;
    }
  }

  uint8_t fcs;
  if (!read4b6b(fcs)) {
    Serial.println("[RX] Timeout lendo FCS, descartando");
    return RecvResult::DISCARD;
  }

  frame.fcs = fcs;
  if (!frame.validate()) {
    Serial.printf("[RX] CRC inválido (seq=%d), descartando\n", seq);
    return RecvResult::DISCARD;
  }

  return RecvResult::OK;
}

void displayFrame(const Frame &frame) {
  lcd.clear();
  lcd.setCursor(0, 0);

  if (frame.len > 0 && frame.data[0] == 0xFE) {
    lcd.createChar(0, (uint8_t *)frame.data + 1);
    lcd.setCursor(0, 0);
    lcd.write((uint8_t)0);
    Serial.println("[RX] Imagem exibida no LCD");
  } else {
    for (uint8_t i = 0; i < frame.len && i < LCD_COLS; i++) {
      lcd.print((char)frame.data[i]);
      Serial.print((char)frame.data[i]);
    }
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(1200, SERIAL_8N1, RX_PIN, TX_PIN);

  pinMode(TX_VCC_PIN, OUTPUT);
  txPowerOff();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Aguardando...");

  Serial.println("[RX] Receptor iniciado");
}

uint8_t expected = 0;

void loop() {
  Frame frame;
  RecvResult result = receiveFrame(frame);

  if (result == RecvResult::TIMEOUT) {
    delay(10);
    return;
  }

  if (result == RecvResult::DISCARD) {
    return;
  }

  Serial.printf("[RX] Frame recebido tipo=%d seq=%d len=%d (esperado seq=%d)\n",
                frame.type, frame.seq, frame.len, expected);

  if (frame.type == Type::END) {
    Serial.println("[RX] END recebido, sessão encerrada");
    lcd.clear();
    lcd.print("FIM");
    expected = 0;
    return;
  }

  if (frame.type != Type::DATA) {
    Serial.printf("[RX] Tipo inesperado %d ignorando\n", frame.type);
    return;
  }

  if (frame.seq == expected) {
    displayFrame(frame);
    sendACK(frame.seq);
    expected ^= 1;
  } else {
    Serial.printf("[RX] Duplicata seq=%d re-enviando ACK\n", frame.seq);
    sendACK(frame.seq);
  }
}
