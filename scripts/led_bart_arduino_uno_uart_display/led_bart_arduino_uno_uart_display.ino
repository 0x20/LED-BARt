#include <Arduino.h>

// LED-BARt dumb display driver
// Receives 95 raw column bytes over serial, shifts them out to the LED bar.
// No font, no text parsing — just pixels in, LEDs out.

#define KLOK 2
#define DATA 3

byte pinRij[7] = {4, 5, 6, 7, 8, 9, 10};

#define PULSTIJD 1500
#define NUM_COLS 95

unsigned char kolommen[NUM_COLS];
unsigned char rxBuf[NUM_COLS];
byte rxIdx = 0;
bool frameReady = false;

void checkSerial() {
  byte limit = 16;  // don't hog CPU — keep the multiplex alive
  while (Serial.available() && limit--) {
    rxBuf[rxIdx++] = Serial.read();
    if (rxIdx >= NUM_COLS) {
      frameReady = true;  // swap happens at top of next sweep to avoid tearing
      rxIdx = 0;
      return;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(KLOK, OUTPUT);
  pinMode(DATA, OUTPUT);
  for (int i = 0; i < 7; i++)
    pinMode(pinRij[i], OUTPUT);
  memset(kolommen, 0, NUM_COLS);
  // Ignore boot garbage from ESP32 ROM bootloader
  delay(2000);
  while (Serial.available()) Serial.read();
}

void loop() {
  if (frameReady) {
    memcpy(kolommen, rxBuf, NUM_COLS);
    frameReady = false;
  }
  // Ceiling-mounted: row flip via reversed pinRij[], column flip via mirrored index below.
  for (int rij = 0; rij < 7; rij++) {
    for (int col = 0; col < NUM_COLS; col++) {
      if (bitRead(kolommen[NUM_COLS - 1 - col], rij)) {
        digitalWrite(KLOK, HIGH);
        digitalWrite(DATA, HIGH);
        digitalWrite(KLOK, LOW);
      } else {
        digitalWrite(KLOK, HIGH);
        digitalWrite(DATA, LOW);
        digitalWrite(KLOK, LOW);
      }
    }
    digitalWrite(KLOK, HIGH);
    digitalWrite(KLOK, LOW);

    digitalWrite(pinRij[rij], HIGH);
    delayMicroseconds(PULSTIJD);
    digitalWrite(pinRij[rij], LOW);

    checkSerial();
  }
}
