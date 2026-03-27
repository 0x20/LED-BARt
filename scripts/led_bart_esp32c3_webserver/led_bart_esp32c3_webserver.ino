#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HardwareSerial.h>

#define WIFI_SSID "0x20"
#define WIFI_PASS "unicorns"
#define MDNS_HOSTNAME "ledbart"

// UART0 default pins: D6=TX, D7=RX
HardwareSerial UnoSerial(0);

WebServer server(80);

// Log buffer served at GET /log
#define LOG_MAX 4096
String logBuffer;

// Outputs to Serial, LED bar, and log buffer
void out(const String &msg) {
  Serial.println(msg);
  UnoSerial.println(msg);
  logBuffer += msg + "\n";
  if (logBuffer.length() > LOG_MAX)
    logBuffer = logBuffer.substring(logBuffer.length() - LOG_MAX);
}

// Outputs to Serial and log buffer only (no LED bar)
void log(const String &msg) {
  Serial.println(msg);
  logBuffer += msg + "\n";
  if (logBuffer.length() > LOG_MAX)
    logBuffer = logBuffer.substring(logBuffer.length() - LOG_MAX);
}

void handleText() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "no body");
    return;
  }
  String body = server.arg("plain");
  log("Received: " + body);
  out(body);
  server.send(200, "text/plain", "ok");
}

void handleLog() {
  server.send(200, "text/plain", logBuffer);
}

void setup() {
  Serial.begin(115200);
  UnoSerial.begin(9600, SERIAL_8N1, -1, -1);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  log("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  log("IP: " + WiFi.localIP().toString());

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    log("mDNS: http://" MDNS_HOSTNAME ".local");
  } else {
    log("mDNS failed");
  }

  server.on("/text", HTTP_POST, handleText);
  server.on("/log", HTTP_GET, handleLog);
  server.begin();
}

void loop() {
  server.handleClient();
}
