#include <WiFi.h>
#include <WebServer.h>

#define WIFI_SSID "0x20"
#define WIFI_PASS "unicorns"

WebServer server(80);

void handleText() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "no body");
    return;
  }
  String body = server.arg("plain");
  Serial.println("Received: " + body);
  server.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());
  
  server.on("/text", HTTP_POST, handleText);
  server.begin();
}

void loop() {
  server.handleClient();
}
