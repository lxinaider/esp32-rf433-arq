#include <LiquidCrystal_I2C.h>
#include "Frame.h"
#include "CRC.h"
#include "4b6b.h"
#include "ARQ.h"

#define RX_PIN 16
#define TX_PIN 17
#define TX_VCC_PIN 33 // pino que liga/desliga a alimentação do módulo TX

#define LCD_COLS 16
#define LCD_ROWS 2
#define LCD_ADDR 0x27

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Liga o transmissor e aguarda 100ms pra ele estabilizar antes de enviar
inline void txPowerOn() {
  digitalWrite(TX_VCC_PIN, HIGH);
  delay(100);
}

// Desliga o transmissor e aguarda o receptor estabilizar antes de ouvir
inline void txPowerOff() {
  digitalWrite(TX_VCC_PIN, LOW);
  delay(RX_SETTLE_MS);
}

// Espera um byte chegar na Serial2 dentro do timeout
// Retorna false se expirar
bool readRawByte(uint8_t &out, uint32_t timeoutMs = TIMEOUT_MS) {
  uint32_t t0 = millis();
  while (!Serial2.available()) {
    if (millis() - t0 > timeoutMs) return false;
  }
  out = Serial2.read();
  return true;
}

// Lê dois símbolos 6b e decodifica de volta pra um byte
// Descarta se inválido
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

// Varre o stream byte a byte até encontrar o FLAG_BYTE
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

    // Avança um símbolo por vez pra não perder o flag.
    hi = lo;
  }

  return false;
}

// Envia um ACK pro transmissor confirmando que o frame chegou
void sendACK(uint8_t seq) {
  delay(RX_SETTLE_MS);
  txPowerOn();

  Frame ack(Type::ACK, seq, 0);

  // Preâmbulo pra dar tempo do TX estabilizar antes dos dados do ACK
  for (uint8_t i = 0; i < PREAMBLE_BYTES_ACK; i++) {
    uint8_t hi, lo;
    enc4b6b(PREAMBLE_BYTE, hi, lo);
    Serial2.write(hi);
    Serial2.write(lo);
  }

  // Envia o frame ACK codificado em 4b6b byte a byte
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

// Tenta receber um frame completo: espera flag, lê header, payload e valida CRC
RecvResult receiveFrame(Frame &frame) {
  if (!waitForFlag()) {
    return RecvResult::TIMEOUT;
  }

  uint8_t type_raw, seq, len;
  if (!read4b6b(type_raw) || !read4b6b(seq) || !read4b6b(len)) {
    Serial.println("[RX] Timeout lendo header, descartando");
    return RecvResult::DISCARD;
  }

  // Rejeita tipo ou tamanho fora do esperado antes de ler o resto
  if (type_raw > Type::END) {
    Serial.printf("[RX] Tipo inválido: %d, descartando\n", type_raw);
    return RecvResult::DISCARD;
  }
  if (len > sizeof(Frame::data)) {
    Serial.printf("[RX] len inválido: %d, descartando\n", len);
    return RecvResult::DISCARD;
  }

  frame = Frame((Type)type_raw, seq, len);

  // Lê sempre os 16 bytes do campo data, mesmo que len seja menor
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

// Exibe o conteúdo do frame no LCD
// Se o primeiro byte for 0xFE, é um char customizado
void displayFrame(const Frame &frame) {
  lcd.clear();
  lcd.setCursor(0, 0);

  if (frame.len > 0 && frame.data[0] == 0xFE) {
    // 0xFE é um marcador interno
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
  Serial2.begin(1200, SERIAL_8N1, RX_PIN, TX_PIN); // 1200 baud

  pinMode(TX_VCC_PIN, OUTPUT);
  txPowerOff(); // começa com TX desligado

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Aguardando...");

  Serial.println("[RX] Receptor iniciado");
}

// seq esperado
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
    // Frame novo e na ordem
    displayFrame(frame);
    sendACK(frame.seq);
    expected ^= 1;
  } else {
    // Reenvia o ACK sem exibir de novo
    Serial.printf("[RX] seq=%d re-enviando ACK\n", frame.seq);
    sendACK(frame.seq);
  }
}
