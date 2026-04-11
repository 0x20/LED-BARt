#include <Arduino.h>
#include <SPI.h>

// LED-BARt display driver.
// SPI 1 MHz on pin 13/11. UART 500k on pin 0. Timer1 for precise row timing.
// Triple buffer: display / ready / receive. Shows latest frame, no rate matching.
// Fixed 16ms visual frame. BAM weights [1,2,4,8] — 16 linear brightness levels.

byte pinRij[7] = {10, 9, 8, 7, 6, 5, 4};

#define NUM_BYTES   12
#define NUM_PLANES  4
#define FRAME_SIZE  (NUM_PLANES * 7 * NUM_BYTES)  // 336
#define SYNC1       0xAA
#define SYNC2       0x55
#define T_UNIT      100
#define FRAME_TIMEOUT 50
#define FRAME_PERIOD  17000  // µs fixed visual frame (~59 Hz)
#define GAP_US        100    // µs fixed gap after each row (covers worst-case checkSerial)

static const uint8_t BAM_W[] = {2, 4, 5, 8};

// Triple buffer
unsigned char displayBuf[FRAME_SIZE];
unsigned char readyBuf[FRAME_SIZE];
unsigned char rxBuf[FRAME_SIZE + 1];
volatile bool readyAvailable = false;

// Serial state
enum { WAIT_SYNC1, WAIT_SYNC2, READING } rxState = WAIT_SYNC1;
int rxIdx = 0;
unsigned long rxStart = 0;

static inline void preciseDelay(unsigned int us) {
  TCCR1B = 0; TCNT1 = 0; TCCR1A = 0; TCCR1B = 1;
  uint16_t target = us * 16;
  while (TCNT1 < target) {}
  TCCR1B = 0;
}

void checkSerial() {
  if (rxState == READING && (millis() - rxStart) > FRAME_TIMEOUT)
    rxState = WAIT_SYNC1;
  byte budget = 64;
  while (budget-- && Serial.available()) {
    uint8_t b = Serial.read();
    if (rxState == WAIT_SYNC1) {
      if (b == SYNC1) rxState = WAIT_SYNC2;
    } else if (rxState == WAIT_SYNC2) {
      if (b == SYNC2) { rxState = READING; rxIdx = 0; rxStart = millis(); }
      else if (b != SYNC1) rxState = WAIT_SYNC1;
    } else {
      rxBuf[rxIdx++] = b;
      if (rxIdx >= FRAME_SIZE + 1) {
        uint8_t crc = 0;
        for (int i = 0; i < FRAME_SIZE; i++) crc ^= rxBuf[i];
        if (crc == rxBuf[FRAME_SIZE]) {
          memcpy(readyBuf, rxBuf, FRAME_SIZE);
          readyAvailable = true;
        }
        rxState = WAIT_SYNC1;
      }
    }
  }
}

void setup() {
  pinMode(10, OUTPUT);
  for (int i = 0; i < 7; i++) pinMode(pinRij[i], OUTPUT);
  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  Serial.begin(500000);
  UCSR0B &= ~_BV(TXEN0);
  memset(displayBuf, 0, FRAME_SIZE);
  memset(readyBuf, 0, FRAME_SIZE);
  delay(500);
  while (Serial.available()) Serial.read();
}

void loop() {
  unsigned long frameStart = micros();

  for (int p = 0; p < NUM_PLANES; p++) {
    unsigned char *plane = displayBuf + p * 7 * NUM_BYTES;
    for (int r = 0; r < 7; r++) {
      unsigned char *row = plane + r * NUM_BYTES;
      for (int b = 0; b < NUM_BYTES; b++) SPI.transfer(row[b]);
      digitalWrite(pinRij[r], HIGH);
      preciseDelay(T_UNIT * BAM_W[p]);
      digitalWrite(pinRij[r], LOW);
      checkSerial();
    }
    // Swap between sub-frames for low latency (~4ms max)
    if (readyAvailable) {
      memcpy(displayBuf, readyBuf, FRAME_SIZE);
      readyAvailable = false;
    }
  }

  // Pad to fixed frame period
  while ((micros() - frameStart) < FRAME_PERIOD) {
    checkSerial();
  }
}
