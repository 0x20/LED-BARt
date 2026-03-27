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

void handleText() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "no body");
    return;
  }
  String body = server.arg("plain");
  Serial.println("Received: " + body);
  UnoSerial.println(body);
  server.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  UnoSerial.begin(9600, SERIAL_8N1, -1, -1);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: http://" MDNS_HOSTNAME ".local");
  } else {
    Serial.println("mDNS failed");
  }

  server.on("/text", HTTP_POST, handleText);
  server.begin();
}

void loop() {
  server.handleClient();
}
