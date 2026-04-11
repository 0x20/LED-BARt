#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HardwareSerial.h>
#include <WebSocketsServer.h>
#include "effects.h"
#include "config.h"

// UART1 on GPIO20(RX)/GPIO21(TX) — dedicated to Arduino.
HardwareSerial UnoSerial(0);

WebServer server(80);
WebSocketsServer ws(81);

#define LOG_MAX 4096
String logBuffer;

// Wire protocol: [0xAA][0x55][data...] where byte 0 bit 7 distinguishes
// frames (bit7=0, 336 data + 1 CRC) from commands (bit7=1, cmd-specific payload).
#define WIRE_SYNC1  0xAA
#define WIRE_SYNC2  0x55
#define WIRE_PLANES 4
#define WIRE_BYTES_PER_ROW 12
#define WIRE_FRAME_BYTES (WIRE_PLANES * 7 * WIRE_BYTES_PER_ROW)

// Flicker-free BAM LUT: user level 0..15 → plane mask.
// BAM weights [2,4,5,8]. Every non-off level uses 2+ planes so pixels
// get 2+ pulses per frame (125Hz+) — no visible flicker.
//
// Levels 1-2 use temporal dithering (randomly off or sum 6 per frame)
// to fill the gap between off and the dimmest multi-plane level.
// Levels 3-15 map directly to multi-plane patterns.
static const uint8_t USER_TO_BAM[16] = {
  0x0,  // 0: off
  0xFF, // 1: DITHER marker (handled in code)
  0xFF, // 2: DITHER marker
  0x3,  // 3: sum 6   (2+4)     dimmest solid
  0x5,  // 4: sum 7   (2+5)
  0x6,  // 5: sum 9   (4+5)
  0x9,  // 6: sum 10  (2+8)
  0x7,  // 7: sum 11  (2+4+5)
  0xA,  // 8: sum 12  (4+8)
  0xC,  // 9: sum 13  (5+8)
  0xB,  // 10: sum 14 (2+4+8)
  0xD,  // 11: sum 15 (2+5+8)
  0xE,  // 12: sum 17 (4+5+8)
  0xF,  // 13: sum 19 (all)
  0xF,  // 14: sum 19
  0xF,  // 15: sum 19
};

// Resolve a user level to a BAM plane mask, with temporal dithering
// for levels 1-2 (the gap between off and dimmest multi-plane).
static inline uint8_t resolveBAM(uint8_t level) {
  uint8_t bam = USER_TO_BAM[level & 0x0F];
  if (bam != 0xFF) return bam;
  // Dither: level 1 = ~33% chance of sum 6, level 2 = ~67%
  // This gives perceived brightness between off and sum 6.
  return (random(3) < (level & 0x0F)) ? 0x3 : 0x0;
}

// Pack pixel into BAM planes. Col c → SPI bit position (c+1), skipping
// the padding bit at position 0 (byte 0 MSB, falls off 95-cell chain).
static inline void packPixel(uint8_t packed[WIRE_PLANES][7][WIRE_BYTES_PER_ROW],
                             int col, int row, uint8_t bam) {
  int bp = col;
  uint8_t mask = 1 << (7 - (bp & 7));
  int bi = bp >> 3;
  if (bam & 1) packed[0][row][bi] |= mask;
  if (bam & 2) packed[1][row][bi] |= mask;
  if (bam & 4) packed[2][row][bi] |= mask;
  if (bam & 8) packed[3][row][bi] |= mask;
}

void appendLog(const String &msg) {
  logBuffer += msg + "\n";
  if (logBuffer.length() > LOG_MAX)
    logBuffer = logBuffer.substring(logBuffer.length() - LOG_MAX);
}
static void wireSend(const uint8_t packed[WIRE_PLANES][7][WIRE_BYTES_PER_ROW]) {
  const uint8_t *p = (const uint8_t *)packed;
  uint8_t crc = 0;
  for (int i = 0; i < WIRE_FRAME_BYTES; i++) crc ^= p[i];

  UnoSerial.write(WIRE_SYNC1);
  UnoSerial.write(WIRE_SYNC2);
  UnoSerial.write(p, WIRE_FRAME_BYTES);
  UnoSerial.write(crc);
}

// Send 95 raw 1-bit column bytes (existing effects format) to the UNO.
// Lit pixels are sent at full brightness (BAM pattern 0b1111 = sum 15).
void sendPixels(const uint8_t *cols) {
  static uint8_t packed[WIRE_PLANES][7][WIRE_BYTES_PER_ROW];
  memset(packed, 0, sizeof(packed));

  for (int col = 0; col < NUM_COLS; col++) {
    uint8_t colByte = cols[col];
    if (colByte == 0) continue;
    for (int r = 0; r < 7; r++) {
      if (colByte & (1 << r)) packPixel(packed, col, r, 0b1111);
    }
  }

  wireSend(packed);
}

// Send a row-major level array (7 rows × 95 cols, values 0..15) through
// the gamma LUT. Use this from gradient-aware effects when they exist.
void sendLevels(const uint8_t levels[7][NUM_COLS]) {
  static uint8_t packed[WIRE_PLANES][7][WIRE_BYTES_PER_ROW];
  memset(packed, 0, sizeof(packed));

  for (int r = 0; r < 7; r++) {
    for (int col = 0; col < NUM_COLS; col++) {
      uint8_t bam = resolveBAM(levels[r][col]);
      if (bam) packPixel(packed, col, r, bam);
    }
  }

  wireSend(packed);
}

// Render text to 95 column bytes using font, send to Arduino
void sendText(const String &text) {
  uint8_t cols[NUM_COLS];
  memset(cols, 0, NUM_COLS);
  String padded = text;
  while ((int)padded.length() < TEKST_LEN) padded += ' ';
  textToColumns(padded.c_str(), TEKST_LEN, cols, NUM_COLS);
  sendPixels(cols);
}

uint8_t hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0;
}

void decodeHexPixels(const char *hex, size_t len) {
  if (len < NUM_COLS * 2) return;
  uint8_t cols[NUM_COLS];
  for (int i = 0; i < NUM_COLS; i++)
    cols[i] = (hexVal(hex[i * 2]) << 4) | hexVal(hex[i * 2 + 1]);
  sendPixels(cols);
}

// ============================================================
// HTTP handlers
// ============================================================

void wsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_BIN && length >= NUM_COLS) {
    stopEffect();
    sendPixels(payload);
  }
}

void cors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
}

void handleOptions() {
  cors();
  server.send(204);
}

void handleText() {
  cors();
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "no body");
    return;
  }
  stopEffect();
  String body = server.arg("plain");
  appendLog("Text: " + body);
  sendText(body);
  server.send(200, "text/plain", "ok");
}

void handlePixels() {
  cors();
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "no body");
    return;
  }
  String body = server.arg("plain");
  if (body.length() < NUM_COLS * 2) {
    server.send(400, "text/plain", "need 190 hex chars");
    return;
  }
  stopEffect();
  decodeHexPixels(body.c_str(), body.length());
  server.send(200, "text/plain", "ok");
}

void handleLog() {
  cors();
  server.send(200, "text/plain", logBuffer);
}

// POST /effect — start an effect
void handleStartEffect() {
  cors();
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }
  String body = server.arg("plain");

  // Simple JSON parsing (no library needed for flat objects)
  String effect = "";
  String text = "";
  int speed = 100;

  int idx = body.indexOf("\"effect\"");
  if (idx >= 0) {
    int colon = body.indexOf(':', idx);
    int q1 = body.indexOf('"', colon + 1);
    int q2 = body.indexOf('"', q1 + 1);
    if (q1 >= 0 && q2 > q1) effect = body.substring(q1 + 1, q2);
  }

  idx = body.indexOf("\"text\"");
  if (idx >= 0) {
    int colon = body.indexOf(':', idx);
    int q1 = body.indexOf('"', colon + 1);
    int q2 = body.indexOf('"', q1 + 1);
    if (q1 >= 0 && q2 > q1) text = body.substring(q1 + 1, q2);
  }

  idx = body.indexOf("\"speed\"");
  if (idx >= 0) {
    int colon = body.indexOf(':', idx);
    if (colon >= 0) {
      String numStr = "";
      for (int i = colon + 1; i < (int)body.length(); i++) {
        char c = body[i];
        if (c >= '0' && c <= '9') numStr += c;
        else if (numStr.length() > 0) break;
      }
      if (numStr.length() > 0) speed = numStr.toInt();
    }
  }

  if (effect.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"missing effect\"}");
    return;
  }
  if (!isValidEffect(effect)) {
    server.send(400, "application/json", "{\"error\":\"unknown effect\"}");
    return;
  }

  activeEffect = effect;
  effectText = text.length() > 0 ? text : "LED-BARt";
  effectSpeed = speed > 0 ? speed : 100;

  appendLog("Effect: " + activeEffect + " text=" + effectText + " speed=" + String(effectSpeed));

  bool oneShot = initEffect();
  if (oneShot) sendPixels(frameBuf);

  server.send(200, "application/json",
    "{\"effect\":\"" + activeEffect + "\",\"text\":\"" + effectText + "\",\"speed\":" + String(effectSpeed) + "}");
}

// DELETE /effect — stop current effect
void handleStopEffect() {
  cors();
  stopEffect();
  memset(frameBuf, 0, NUM_COLS);
  sendPixels(frameBuf);
  appendLog("Effect stopped");
  server.send(200, "application/json", "{\"effect\":\"\"}");
}

// GET /effect — return current effect status
void handleGetEffect() {
  cors();
  server.send(200, "application/json",
    "{\"effect\":\"" + activeEffect + "\",\"text\":\"" + effectText + "\",\"speed\":" + String(effectSpeed) + "}");
}

void handleHelp() {
  cors();
  server.send(200, "text/plain",
    "LED-BARt API\n"
    "=============\n"
    "Display: 95x7 pixels (19 chars x 5px wide, 7px tall)\n"
    "\n"
    "HTTP Endpoints:\n"
    "\n"
    "  POST /text\n"
    "    Send text to display (max 19 chars, rendered with 5x7 font)\n"
    "    curl -X POST http://ledbart.local/text -d 'HELLO WORLD'\n"
    "\n"
    "  POST /pixels\n"
    "    Send raw pixel data as 190 hex chars (95 columns)\n"
    "\n"
    "  POST /effect\n"
    "    Start an effect. JSON body: {\"effect\":\"scroll\",\"text\":\"hello\",\"speed\":100}\n"
    "    Effects: scroll, blink, wave, rain, sparkle, gol, inverted, pulse\n"
    "    curl -X POST http://ledbart.local/effect -H 'Content-Type: application/json' -d '{\"effect\":\"rain\",\"speed\":100}'\n"
    "\n"
    "  DELETE /effect\n"
    "    Stop current effect, clear display\n"
    "    curl -X DELETE http://ledbart.local/effect\n"
    "\n"
    "  GET /effect\n"
    "    Get current effect status as JSON\n"
    "\n"
    "  GET /log\n"
    "    View recent activity log\n"
    "\n"
    "  GET /help\n"
    "    This page\n"
    "\n"
    "WebSocket (port 81):\n"
    "  Binary frames: 95 raw bytes (column data)\n"
    "\n"
    "Column format:\n"
    "  Each byte = 1 column, bits 0-6 = rows (bit 0 = top, bit 6 = bottom)\n"
    "  0x7F = all rows lit, 0x00 = all rows off\n"
  );
}

// ============================================================
// Setup & Loop
// ============================================================

void setup() {
  UnoSerial.begin(500000, SERIAL_8N1, 20, 21);
  delay(500);
  sendText(" Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setSleep(false);
  appendLog("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  appendLog("IP: " + WiFi.localIP().toString());

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    appendLog("mDNS: http://" MDNS_HOSTNAME ".local");
  } else {
    appendLog("mDNS failed");
  }

  server.on("/text", HTTP_POST, handleText);
  server.on("/text", HTTP_OPTIONS, handleOptions);
  server.on("/pixels", HTTP_POST, handlePixels);
  server.on("/pixels", HTTP_OPTIONS, handleOptions);
  server.on("/effect", HTTP_POST, handleStartEffect);
  server.on("/effect", HTTP_DELETE, handleStopEffect);
  server.on("/effect", HTTP_GET, handleGetEffect);
  server.on("/effect", HTTP_OPTIONS, handleOptions);
  server.on("/log", HTTP_GET, handleLog);
  server.on("/log", HTTP_OPTIONS, handleOptions);
  server.on("/help", HTTP_GET, handleHelp);
  server.on("/api", HTTP_GET, handleHelp);
  server.begin();

  ws.begin();
  ws.onEvent(wsEvent);

  activeEffect = "scroll";
  effectText = "Welcome to 0x20!";
  effectSpeed = 80;
  initEffect();

}

void loop() {
  // Effect first — runs before server can block
  if (activeEffect.length() > 0) {
    unsigned long now = millis();
    if (now - lastFrame >= (unsigned long)effectSpeed) {
      lastFrame += effectSpeed;
      if (now - lastFrame > (unsigned long)effectSpeed) lastFrame = now;
      tickEffect();
      if (effectUsesLevels)
        sendLevels(frameLevels);
      else
        sendPixels(frameBuf);
    }
  }
  // Throttle server handling so network housekeeping can't stall effects.
  // HTTP responses delayed up to 200ms — fine for a display controller.
  static unsigned long lastServer = 0;
  if (millis() - lastServer > 200) {
    lastServer = millis();
    server.handleClient();
    ws.loop();
  }
}
