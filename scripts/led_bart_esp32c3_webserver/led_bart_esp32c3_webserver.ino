#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HardwareSerial.h>
#include <WebSocketsServer.h>
#include "effects.h"
#include "config.h"

// UART0 on GPIO20(RX)/GPIO21(TX) — dedicated to Arduino
HardwareSerial UnoSerial(0);

WebServer server(80);
WebSocketsServer ws(81);

#define LOG_MAX 4096
String logBuffer;

// ============================================================
// Utility
// ============================================================

void appendLog(const String &msg) {
  logBuffer += msg + "\n";
  if (logBuffer.length() > LOG_MAX)
    logBuffer = logBuffer.substring(logBuffer.length() - LOG_MAX);
}

// Render text to 95 column bytes using font, send to Arduino
void sendText(const String &text) {
  uint8_t cols[NUM_COLS];
  memset(cols, 0, NUM_COLS);
  String padded = text;
  while ((int)padded.length() < TEKST_LEN) padded += ' ';
  textToColumns(padded.c_str(), TEKST_LEN, cols, NUM_COLS);
  UnoSerial.write(cols, NUM_COLS);
}

// Send 95 raw column bytes directly to Arduino
void sendPixels(const uint8_t *cols) {
  UnoSerial.write(cols, NUM_COLS);
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
  UnoSerial.begin(115200, SERIAL_8N1, 20, 21);
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
  server.handleClient();
  ws.loop();

  if (activeEffect.length() > 0) {
    unsigned long now = millis();
    if (now - lastFrame >= (unsigned long)effectSpeed) {
      lastFrame = now;
      tickEffect();
      sendPixels(frameBuf);
    }
  }
}
