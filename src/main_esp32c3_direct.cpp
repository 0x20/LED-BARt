// LED-BARt ESP32-C3 Direct Driver
// Drives LED bar directly without Arduino
// Web interface + MQTT + WiFi config
// M. Wyns 2025

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <TimeLib.h>

// ============================================================================
// CORE LED BAR PINS (Same as Arduino - via level shifters)
// ============================================================================
#define KLOK 2  // Shift register clock
#define DATA 3  // Shift register data

// Row pins - 7 pins for the display rows
const byte pinRij[7] = {4, 5, 6, 7, 8, 9, 10};

#define pulstijd 1500  // Pulse time in microseconds

// ============================================================================
// CONFIGURATION STORAGE
// ============================================================================
Preferences preferences;

// WiFi config
String wifi_ssid = "";
String wifi_password = "";
bool useAPMode = true;

// AP fallback
const char* ap_ssid = "LED-BARt";
const char* ap_password = "ledbart123";

// MQTT config
String mqtt_server = "";
int mqtt_port = 1883;
String mqtt_user = "";
String mqtt_password = "";
String mqtt_topic = "ledbart/text";
String mqtt_raw_topic = "ledbart/raw";

// mDNS hostname
const char* hostname = "ledbart";

// ============================================================================
// DISPLAY BUFFER
// ============================================================================
unsigned char tekst[100];
int tekstLength = 19;
unsigned char rawDisplay[7][100]; // Raw bitmap buffer for MQTT raw mode
bool useRawDisplay = false;

// Stats
unsigned long lastTimeUpdate = 0;
unsigned long mqttMessagesReceived = 0;
unsigned long webRequestsReceived = 0;
bool showColon = true;

// MQTT text display
String mqttText = "";
bool hasMqttText = false;
unsigned long lastMqttReceived = 0;

// ============================================================================
// FONT DATA (5x7 font from Arduino)
// ============================================================================
static unsigned char Font5x7[] = {
  0x00, 0x00, 0x00, 0x00, 0x00,// (space) 32
  0x00, 0x00, 0x5F, 0x00, 0x00,// ! 33
  0x00, 0x07, 0x00, 0x07, 0x00,// "
  0x14, 0x7F, 0x14, 0x7F, 0x14,// #
  0x24, 0x2A, 0x7F, 0x2A, 0x12,// $
  0x23, 0x13, 0x08, 0x64, 0x62,// %
  0x36, 0x49, 0x55, 0x22, 0x50,// &
  0x00, 0x05, 0x03, 0x00, 0x00,// '
  0x00, 0x1C, 0x22, 0x41, 0x00,// (
  0x00, 0x41, 0x22, 0x1C, 0x00,// )
  0x08, 0x2A, 0x1C, 0x2A, 0x08,// *
  0x08, 0x08, 0x3E, 0x08, 0x08,// +
  0x00, 0x50, 0x30, 0x00, 0x00,// ,
  0x08, 0x08, 0x08, 0x08, 0x08,// -
  0x00, 0x60, 0x60, 0x00, 0x00,// .
  0x20, 0x10, 0x08, 0x04, 0x02,// /
  0x3E, 0x51, 0x49, 0x45, 0x3E,// 0
  0x00, 0x42, 0x7F, 0x40, 0x00,// 1
  0x42, 0x61, 0x51, 0x49, 0x46,// 2
  0x21, 0x41, 0x45, 0x4B, 0x31,// 3
  0x18, 0x14, 0x12, 0x7F, 0x10,// 4
  0x27, 0x45, 0x45, 0x45, 0x39,// 5
  0x3C, 0x4A, 0x49, 0x49, 0x30,// 6
  0x01, 0x71, 0x09, 0x05, 0x03,// 7
  0x36, 0x49, 0x49, 0x49, 0x36,// 8
  0x06, 0x49, 0x49, 0x29, 0x1E,// 9
  0x00, 0x36, 0x36, 0x00, 0x00,// :
  0x00, 0x56, 0x36, 0x00, 0x00,// ;
  0x00, 0x08, 0x14, 0x22, 0x41,// <
  0x14, 0x14, 0x14, 0x14, 0x14,// =
  0x41, 0x22, 0x14, 0x08, 0x00,// >
  0x02, 0x01, 0x51, 0x09, 0x06,// ?
  0x32, 0x49, 0x79, 0x41, 0x3E,// @
  0x7E, 0x11, 0x11, 0x11, 0x7E,// A
  0x7F, 0x49, 0x49, 0x49, 0x36,// B
  0x3E, 0x41, 0x41, 0x41, 0x22,// C
  0x7F, 0x41, 0x41, 0x22, 0x1C,// D
  0x7F, 0x49, 0x49, 0x49, 0x41,// E
  0x7F, 0x09, 0x09, 0x01, 0x01,// F
  0x3E, 0x41, 0x41, 0x51, 0x32,// G
  0x7F, 0x08, 0x08, 0x08, 0x7F,// H
  0x00, 0x41, 0x7F, 0x41, 0x00,// I
  0x20, 0x40, 0x41, 0x3F, 0x01,// J
  0x7F, 0x08, 0x14, 0x22, 0x41,// K
  0x7F, 0x40, 0x40, 0x40, 0x40,// L
  0x7F, 0x02, 0x04, 0x02, 0x7F,// M
  0x7F, 0x04, 0x08, 0x10, 0x7F,// N
  0x3E, 0x41, 0x41, 0x41, 0x3E,// O
  0x7F, 0x09, 0x09, 0x09, 0x06,// P
  0x3E, 0x41, 0x51, 0x21, 0x5E,// Q
  0x7F, 0x09, 0x19, 0x29, 0x46,// R
  0x46, 0x49, 0x49, 0x49, 0x31,// S
  0x01, 0x01, 0x7F, 0x01, 0x01,// T
  0x3F, 0x40, 0x40, 0x40, 0x3F,// U
  0x1F, 0x20, 0x40, 0x20, 0x1F,// V
  0x7F, 0x20, 0x18, 0x20, 0x7F,// W
  0x63, 0x14, 0x08, 0x14, 0x63,// X
  0x03, 0x04, 0x78, 0x04, 0x03,// Y
  0x61, 0x51, 0x49, 0x45, 0x43,// Z
  0x00, 0x00, 0x7F, 0x41, 0x41,// [
  0x02, 0x04, 0x08, 0x10, 0x20,// "\"
  0x41, 0x41, 0x7F, 0x00, 0x00,// ]
  0x04, 0x02, 0x01, 0x02, 0x04,// ^
  0x40, 0x40, 0x40, 0x40, 0x40,// _
  0x00, 0x01, 0x02, 0x04, 0x00,// `
  0x20, 0x54, 0x54, 0x54, 0x78,// a
  0x7F, 0x48, 0x44, 0x44, 0x38,// b
  0x38, 0x44, 0x44, 0x44, 0x20,// c
  0x38, 0x44, 0x44, 0x48, 0x7F,// d
  0x38, 0x54, 0x54, 0x54, 0x18,// e
  0x08, 0x7E, 0x09, 0x01, 0x02,// f
  0x08, 0x14, 0x54, 0x54, 0x3C,// g
  0x7F, 0x08, 0x04, 0x04, 0x78,// h
  0x00, 0x44, 0x7D, 0x40, 0x00,// i
  0x20, 0x40, 0x44, 0x3D, 0x00,// j
  0x00, 0x7F, 0x10, 0x28, 0x44,// k
  0x00, 0x41, 0x7F, 0x40, 0x00,// l
  0x7C, 0x04, 0x18, 0x04, 0x78,// m
  0x7C, 0x08, 0x04, 0x04, 0x78,// n
  0x38, 0x44, 0x44, 0x44, 0x38,// o
  0x7C, 0x14, 0x14, 0x14, 0x08,// p
  0x08, 0x14, 0x14, 0x18, 0x7C,// q
  0x7C, 0x08, 0x04, 0x04, 0x08,// r
  0x48, 0x54, 0x54, 0x54, 0x20,// s
  0x04, 0x3F, 0x44, 0x40, 0x20,// t
  0x3C, 0x40, 0x40, 0x20, 0x7C,// u
  0x1C, 0x20, 0x40, 0x20, 0x1C,// v
  0x3C, 0x40, 0x30, 0x40, 0x3C,// w
  0x44, 0x28, 0x10, 0x28, 0x44,// x
  0x0C, 0x50, 0x50, 0x50, 0x3C,// y
  0x44, 0x64, 0x54, 0x4C, 0x44,// z
  0x00, 0x08, 0x36, 0x41, 0x00,// {
  0x00, 0x00, 0x7F, 0x00, 0x00,// |
  0x00, 0x41, 0x36, 0x08, 0x00,// }
  0x00, 0x07, 0x05, 0x07, 0x00 // °  126
};

// ============================================================================
// NETWORKING
// ============================================================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
void loadConfig();
void saveConfig();
void setup_wifi();
void setup_ap();
void reconnect_mqtt();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void displayText();
void updateTimeDisplay();
void handleRoot();
void handleConfig();
void handleSaveConfig();
void handleStatus();
void handleMQTTHelp();
void handleNotFound();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=================================");
  Serial.println("LED-BARt Direct ESP32-C3 Driver");
  Serial.println("Board: XIAO ESP32-C3");
  Serial.println("=================================");

  // Initialize LED bar pins
  pinMode(KLOK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  for (int i = 0; i <= 6; i++) {
    pinMode(pinRij[i], OUTPUT);
    digitalWrite(pinRij[i], LOW);
  }

  // Load configuration from flash
  loadConfig();

  // Show startup message
  strcpy((char*)tekst, "ESP32 Ready!");
  tekstLength = 12;

  // Display startup message for 2 seconds
  unsigned long startTime = millis();
  while (millis() - startTime < 2000) {
    displayText();
  }

  // Setup WiFi
  setup_wifi();

  // Setup mDNS
  if (!useAPMode) {
    if (MDNS.begin(hostname)) {
      Serial.print("mDNS started: http://");
      Serial.print(hostname);
      Serial.println(".local");
      MDNS.addService("http", "tcp", 80);
    }
  }

  // Setup Web Server
  server.on("/", handleRoot);
  server.on("/config", handleConfig);
  server.on("/saveconfig", HTTP_POST, handleSaveConfig);
  server.on("/status", handleStatus);
  server.on("/mqtt-help", handleMQTTHelp);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Web server started!");
  Serial.print("Access at: http://");
  if (useAPMode) {
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.print(WiFi.localIP());
    Serial.print(" or http://");
    Serial.print(hostname);
    Serial.println(".local");
  }

  // Setup MQTT (optional)
  if (!useAPMode && mqtt_server.length() > 3) {
    mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
    mqttClient.setCallback(mqtt_callback);
    Serial.println("MQTT client configured");
  }

  // Initialize time
  setTime(1496399575);

  Serial.println("\n=================================");
  Serial.println("Setup complete!");
  Serial.println("=================================\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // Handle web server
  server.handleClient();

  // Maintain MQTT connection
  if (!useAPMode && mqtt_server.length() > 3) {
    if (!mqttClient.connected()) {
      reconnect_mqtt();
    }
    mqttClient.loop();
  }

  // If we have MQTT text, display it for 30 seconds, then go back to time
  if (hasMqttText && (millis() - lastMqttReceived > 30000)) {
    hasMqttText = false;
    useRawDisplay = false;
    Serial.println("MQTT text timeout, switching to time display");
  }

  // Update display every 500ms
  if (millis() - lastTimeUpdate > 500) {
    lastTimeUpdate = millis();
    showColon = !showColon;

    if (!useRawDisplay && !hasMqttText) {
      // Display time/date when no MQTT text
      updateTimeDisplay();
    } else if (hasMqttText && !useRawDisplay) {
      // Display MQTT text
      tekstLength = min(mqttText.length(), 99);
      for (int i = 0; i < tekstLength; i++) {
        tekst[i] = mqttText.charAt(i);
      }
      tekst[tekstLength] = '\0';
    }
  }

  // Always display (either raw bitmap or text)
  displayText();

  delay(1);
}

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================
void loadConfig() {
  preferences.begin("ledbart", false);

  wifi_ssid = preferences.getString("wifi_ssid", "");
  wifi_password = preferences.getString("wifi_pass", "");
  mqtt_server = preferences.getString("mqtt_srv", "");
  mqtt_port = preferences.getInt("mqtt_port", 1883);
  mqtt_user = preferences.getString("mqtt_user", "");
  mqtt_password = preferences.getString("mqtt_pass", "");
  mqtt_topic = preferences.getString("mqtt_topic", "ledbart/text");
  mqtt_raw_topic = preferences.getString("mqtt_raw", "ledbart/raw");

  preferences.end();

  Serial.println("Configuration loaded:");
  Serial.print("  WiFi SSID: ");
  Serial.println(wifi_ssid.length() > 0 ? wifi_ssid : "(not set)");
  Serial.print("  MQTT Server: ");
  Serial.println(mqtt_server.length() > 0 ? mqtt_server : "(not set)");
}

void saveConfig() {
  preferences.begin("ledbart", false);

  preferences.putString("wifi_ssid", wifi_ssid);
  preferences.putString("wifi_pass", wifi_password);
  preferences.putString("mqtt_srv", mqtt_server);
  preferences.putInt("mqtt_port", mqtt_port);
  preferences.putString("mqtt_user", mqtt_user);
  preferences.putString("mqtt_pass", mqtt_password);
  preferences.putString("mqtt_topic", mqtt_topic);
  preferences.putString("mqtt_raw", mqtt_raw_topic);

  preferences.end();

  Serial.println("Configuration saved!");
}

// ============================================================================
// WIFI MANAGEMENT
// ============================================================================
void setup_wifi() {
  delay(10);

  // Try configured WiFi if available
  if (wifi_ssid.length() > 0) {
    Serial.println();
    Serial.print("Connecting to WiFi: ");
    Serial.println(wifi_ssid);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      useAPMode = false;
      Serial.println("");
      Serial.println("WiFi connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Signal strength: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      return;
    }
  }

  // Fall back to AP mode
  Serial.println("");
  Serial.println("Starting Access Point mode...");
  setup_ap();
}

void setup_ap() {
  useAPMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

  Serial.println("Access Point started!");
  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.print("Password: ");
  Serial.println(ap_password);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
}

// ============================================================================
// MQTT
// ============================================================================
void reconnect_mqtt() {
  static unsigned long lastAttempt = 0;

  if (millis() - lastAttempt < 5000) {
    return;
  }
  lastAttempt = millis();

  if (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker...");

    String clientId = "ESP32C3-LEDBar-";
    clientId += String(random(0xffff), HEX);

    bool connected;
    if (mqtt_user.length() > 0) {
      connected = mqttClient.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_password.c_str());
    } else {
      connected = mqttClient.connect(clientId.c_str());
    }

    if (connected) {
      Serial.println("connected!");
      Serial.print("Subscribing to topics: ");
      Serial.print(mqtt_topic);
      Serial.print(", ");
      Serial.println(mqtt_raw_topic);

      mqttClient.subscribe(mqtt_topic.c_str());
      mqttClient.subscribe(mqtt_raw_topic.c_str());

      // Publish Home Assistant autodiscovery
      String ha_config_topic = "homeassistant/text/ledbart/config";
      String ha_config = "{\"name\":\"LED-BARt Display\",\"unique_id\":\"ledbart_display\",\"command_topic\":\"" + mqtt_topic + "\",\"max\":99}";
      mqttClient.publish(ha_config_topic.c_str(), ha_config.c_str(), true);
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT message received on topic: ");
  Serial.println(topic);

  String topicStr = String(topic);

  // Handle raw bitmap data
  if (topicStr == mqtt_raw_topic) {
    Serial.println("Raw bitmap data received");

    // Format: 7 rows * up to 100 columns, packed as bits
    // Each row can be up to 100 bits = 13 bytes (rounded up)
    // Total max: 7 * 13 = 91 bytes

    if (length >= 7) { // At minimum need 7 bytes (1 per row)
      useRawDisplay = true;
      hasMqttText = true;
      lastMqttReceived = millis();

      // Decode packed bitmap
      int byteIdx = 0;
      for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 100; col++) {
          int bitPos = col % 8;
          if (bitPos == 0 && col > 0) byteIdx++;

          if (byteIdx < length) {
            rawDisplay[row][col] = (payload[byteIdx] >> (7 - bitPos)) & 0x01;
          } else {
            rawDisplay[row][col] = 0;
          }
        }
        // Move to next row's bytes (13 bytes per row max)
        byteIdx = (row + 1) * 13;
      }

      mqttMessagesReceived++;
      return;
    }
  }

  // Handle text data
  if (topicStr == mqtt_topic) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
      message += (char)payload[i];
    }

    Serial.print("Text message: ");
    Serial.println(message);

    mqttText = message;
    hasMqttText = true;
    useRawDisplay = false;
    lastMqttReceived = millis();
    mqttMessagesReceived++;
  }
}

// ============================================================================
// LED BAR DISPLAY (Direct from Arduino logic - TESTED AND WORKING)
// ============================================================================
void displayText() {
  if (useRawDisplay) {
    // Display raw bitmap data
    for (int rij = 0; rij <= 6; rij++) {
      for (int col = 0; col < 100; col++) {
        if (rawDisplay[rij][col]) {
          digitalWrite(KLOK, HIGH);
          digitalWrite(DATA, HIGH);
          digitalWrite(KLOK, LOW);
          digitalWrite(DATA, HIGH);
        } else {
          digitalWrite(KLOK, HIGH);
          digitalWrite(DATA, LOW);
          digitalWrite(KLOK, LOW);
          digitalWrite(DATA, LOW);
        }
      }

      digitalWrite(KLOK, HIGH);
      digitalWrite(KLOK, LOW);

      // Reversed row pins (same as working Arduino code)
      digitalWrite(pinRij[6-rij], HIGH);
      delayMicroseconds(pulstijd);
      digitalWrite(pinRij[6-rij], LOW);
    }
  } else {
    // Display text using font (same as working Arduino code)
    for (int rij = 0; rij <= 6; rij++) {
      for (int kar = 0; kar < tekstLength; kar++) {
        for (int kolom = 0; kolom <= 4; kolom++) {
          if (bitRead((Font5x7[(tekst[kar]-32)*5+kolom]), rij)) {
            digitalWrite(KLOK, HIGH);
            digitalWrite(DATA, HIGH);
            digitalWrite(KLOK, LOW);
            digitalWrite(DATA, HIGH);
          } else {
            digitalWrite(KLOK, HIGH);
            digitalWrite(DATA, LOW);
            digitalWrite(KLOK, LOW);
            digitalWrite(DATA, LOW);
          }
        }
      }

      digitalWrite(KLOK, HIGH);
      digitalWrite(KLOK, LOW);

      // Reversed row pins (same as working Arduino code)
      digitalWrite(pinRij[6-rij], HIGH);
      delayMicroseconds(pulstijd);
      digitalWrite(pinRij[6-rij], LOW);
    }
  }
}

void updateTimeDisplay() {
  char timeStr[20];
  sprintf(timeStr, "%02d%c%02d %02d/%02d  16.5~C",
          hour(),
          showColon ? ':' : ' ',
          minute(),
          day(),
          month());

  tekstLength = strlen(timeStr);
  strcpy((char*)tekst, timeStr);
}

// ============================================================================
// WEB SERVER HANDLERS
// ============================================================================
void handleRoot() {
  webRequestsReceived++;

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>LED-BARt Control</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; background: #f0f0f0; }";
  html += "h1 { color: #333; }";
  html += ".card { background: white; padding: 15px; border-radius: 5px; margin: 10px 0; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
  html += "input[type='text'], input[type='password'], input[type='number'] { width: 100%; padding: 10px; margin: 5px 0; border: 1px solid #ddd; border-radius: 3px; box-sizing: border-box; }";
  html += "button, .btn { background: #4CAF50; color: white; padding: 12px 30px; font-size: 16px; border: none; border-radius: 3px; cursor: pointer; margin: 5px; text-decoration: none; display: inline-block; }";
  html += "button:hover, .btn:hover { background: #45a049; }";
  html += ".btn-info { background: #2196F3; }";
  html += ".btn-info:hover { background: #0b7dda; }";
  html += ".current-display { font-size: 24px; font-family: monospace; background: #000; color: #0f0; padding: 15px; border-radius: 5px; text-align: center; }";
  html += "label { font-weight: bold; display: block; margin-top: 10px; }";
  html += ".help-icon { cursor: help; color: #2196F3; margin-left: 5px; }";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>LED-BARt Control Panel</h1>";
  html += "<p><small>ESP32-C3 Direct Driver</small></p>";

  // Current Display
  html += "<div class='card'>";
  html += "<h2>Current Display</h2>";
  html += "<div class='current-display'>" + String((char*)tekst) + "</div>";
  html += "</div>";

  // Control Form
  html += "<div class='card'>";
  html += "<h2>Set Display Text</h2>";
  html += "<form action='/status' method='POST'>";
  html += "<input type='text' name='text' placeholder='Enter text to display...' maxlength='99'>";
  html += "<button type='submit'>Update Display</button>";
  html += "</form>";
  html += "</div>";

  // Quick Actions
  html += "<div class='card'>";
  html += "<h2>Quick Actions</h2>";
  html += "<button onclick=\"fetch('/status?text=Hello World!');\">Hello World!</button> ";
  html += "<button onclick=\"fetch('/status?text=LED-BARt!');\">LED-BARt!</button> ";
  html += "<a href='/config' class='btn btn-info'>Configuration</a> ";
  html += "<a href='/mqtt-help' class='btn btn-info'>MQTT Help</a>";
  html += "</div>";

  // Status Info
  html += "<div class='card'>";
  html += "<h2>System Status</h2>";
  html += "<div><strong>WiFi Mode:</strong> " + String(useAPMode ? "Access Point" : "Station") + "</div>";
  html += "<div><strong>IP Address:</strong> " + (useAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "</div>";
  if (!useAPMode) {
    html += "<div><strong>Hostname:</strong> " + String(hostname) + ".local</div>";
    html += "<div><strong>WiFi Signal:</strong> " + String(WiFi.RSSI()) + " dBm</div>";
  }
  html += "<div><strong>MQTT Connected:</strong> " + String(mqttClient.connected() ? "Yes" : "No") + "</div>";
  html += "<div><strong>MQTT Messages:</strong> " + String(mqttMessagesReceived) + "</div>";
  html += "<div><strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</div>";
  html += "<div><strong>Free Heap:</strong> " + String(ESP.getFreeHeap()) + " bytes</div>";
  html += "</div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleConfig() {
  webRequestsReceived++;

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>LED-BARt Configuration</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; background: #f0f0f0; }";
  html += "h1 { color: #333; }";
  html += ".card { background: white; padding: 15px; border-radius: 5px; margin: 10px 0; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
  html += "input[type='text'], input[type='password'], input[type='number'] { width: 100%; padding: 10px; margin: 5px 0; border: 1px solid #ddd; border-radius: 3px; box-sizing: border-box; }";
  html += "button { background: #4CAF50; color: white; padding: 12px 30px; font-size: 16px; border: none; border-radius: 3px; cursor: pointer; margin: 5px; width: 100%; }";
  html += "button:hover { background: #45a049; }";
  html += "label { font-weight: bold; display: block; margin-top: 10px; }";
  html += ".info { color: #666; font-size: 14px; margin-top: 5px; }";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>Configuration</h1>";
  html += "<form action='/saveconfig' method='POST'>";

  // WiFi Configuration
  html += "<div class='card'>";
  html += "<h2>WiFi Settings</h2>";
  html += "<p class='info'>Configure WiFi network. Device will reboot after saving.</p>";
  html += "<label>SSID:</label>";
  html += "<input type='text' name='wifi_ssid' value='" + wifi_ssid + "' placeholder='Your WiFi Network Name'>";
  html += "<label>Password:</label>";
  html += "<input type='password' name='wifi_pass' value='" + wifi_password + "' placeholder='WiFi Password'>";
  html += "</div>";

  // MQTT Configuration
  html += "<div class='card'>";
  html += "<h2>MQTT Settings</h2>";
  html += "<p class='info'>Optional. Leave blank to disable MQTT.</p>";
  html += "<label>Server IP/Hostname:</label>";
  html += "<input type='text' name='mqtt_srv' value='" + mqtt_server + "' placeholder='192.168.1.100 or mqtt.local'>";
  html += "<label>Port:</label>";
  html += "<input type='number' name='mqtt_port' value='" + String(mqtt_port) + "' placeholder='1883'>";
  html += "<label>Username (optional):</label>";
  html += "<input type='text' name='mqtt_user' value='" + mqtt_user + "' placeholder='MQTT Username'>";
  html += "<label>Password (optional):</label>";
  html += "<input type='password' name='mqtt_pass' value='" + mqtt_password + "' placeholder='MQTT Password'>";
  html += "<label>Text Topic:</label>";
  html += "<input type='text' name='mqtt_topic' value='" + mqtt_topic + "' placeholder='ledbart/text'>";
  html += "<label>Raw Bitmap Topic:</label>";
  html += "<input type='text' name='mqtt_raw' value='" + mqtt_raw_topic + "' placeholder='ledbart/raw'>";
  html += "</div>";

  html += "<button type='submit'>Save & Reboot</button>";
  html += "</form>";
  html += "<br><a href='/'>← Back to Main</a>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSaveConfig() {
  webRequestsReceived++;

  if (server.hasArg("wifi_ssid")) wifi_ssid = server.arg("wifi_ssid");
  if (server.hasArg("wifi_pass")) wifi_password = server.arg("wifi_pass");
  if (server.hasArg("mqtt_srv")) mqtt_server = server.arg("mqtt_srv");
  if (server.hasArg("mqtt_port")) mqtt_port = server.arg("mqtt_port").toInt();
  if (server.hasArg("mqtt_user")) mqtt_user = server.arg("mqtt_user");
  if (server.hasArg("mqtt_pass")) mqtt_password = server.arg("mqtt_pass");
  if (server.hasArg("mqtt_topic")) mqtt_topic = server.arg("mqtt_topic");
  if (server.hasArg("mqtt_raw")) mqtt_raw_topic = server.arg("mqtt_raw");

  saveConfig();

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv='refresh' content='10;url=/'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configuration Saved</title>";
  html += "<style>body { font-family: Arial; text-align: center; padding: 50px; }</style>";
  html += "</head><body>";
  html += "<h1>Configuration Saved!</h1>";
  html += "<p>Device will reboot in 3 seconds...</p>";
  html += "<p>Reconnect to the new WiFi network if changed.</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);

  delay(3000);
  ESP.restart();
}

void handleStatus() {
  webRequestsReceived++;

  // Handle text update via GET or POST
  if (server.hasArg("text")) {
    String newText = server.arg("text");
    if (newText.length() > 0) {
      mqttText = newText;
      hasMqttText = true;
      useRawDisplay = false;
      lastMqttReceived = millis();

      tekstLength = min(newText.length(), 99);
      for (int i = 0; i < tekstLength; i++) {
        tekst[i] = newText.charAt(i);
      }
      tekst[tekstLength] = '\0';
    }
  }

  // Return JSON status
  String json = "{";
  json += "\"display_text\":\"" + String((char*)tekst) + "\",";
  json += "\"wifi_mode\":\"" + String(useAPMode ? "AP" : "STA") + "\",";
  json += "\"ip\":\"" + (useAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  json += "\"hostname\":\"" + String(hostname) + ".local\",";
  json += "\"mqtt_connected\":" + String(mqttClient.connected() ? "true" : "false") + ",";
  json += "\"mqtt_messages\":" + String(mqttMessagesReceived) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
  json += "}";

  server.send(200, "application/json", json);
}

void handleMQTTHelp() {
  webRequestsReceived++;

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>MQTT Documentation - LED-BARt</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; max-width: 900px; margin: 50px auto; padding: 20px; background: #f0f0f0; line-height: 1.6; }";
  html += "h1, h2, h3 { color: #333; }";
  html += ".card { background: white; padding: 20px; border-radius: 5px; margin: 15px 0; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
  html += "code { background: #f4f4f4; padding: 2px 6px; border-radius: 3px; font-family: 'Courier New', monospace; }";
  html += "pre { background: #2d2d2d; color: #f8f8f8; padding: 15px; border-radius: 5px; overflow-x: auto; }";
  html += ".topic { color: #e83e8c; }";
  html += ".example { background: #e7f3ff; padding: 10px; border-left: 4px solid #2196F3; margin: 10px 0; }";
  html += "table { width: 100%; border-collapse: collapse; margin: 10px 0; }";
  html += "th, td { padding: 10px; border: 1px solid #ddd; text-align: left; }";
  html += "th { background: #4CAF50; color: white; }";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>LED-BARt MQTT Documentation</h1>";

  // Overview
  html += "<div class='card'>";
  html += "<h2>Overview</h2>";
  html += "<p>LED-BARt supports MQTT for remote control and integration with home automation systems like Home Assistant.</p>";
  html += "<p><strong>Current Configuration:</strong></p>";
  html += "<ul>";
  html += "<li><strong>Server:</strong> " + (mqtt_server.length() > 0 ? mqtt_server : "<em>Not configured</em>") + ":" + String(mqtt_port) + "</li>";
  html += "<li><strong>Text Topic:</strong> <code class='topic'>" + mqtt_topic + "</code></li>";
  html += "<li><strong>Raw Bitmap Topic:</strong> <code class='topic'>" + mqtt_raw_topic + "</code></li>";
  html += "</ul>";
  html += "</div>";

  // Text Messages
  html += "<div class='card'>";
  html += "<h2>1. Text Messages</h2>";
  html += "<p>Send plain text to display on the LED bar.</p>";
  html += "<h3>Topic</h3>";
  html += "<code class='topic'>" + mqtt_topic + "</code>";
  html += "<h3>Payload</h3>";
  html += "<p>Plain text string (max 99 characters)</p>";
  html += "<h3>Examples</h3>";
  html += "<div class='example'>";
  html += "<strong>Mosquitto CLI:</strong>";
  html += "<pre>mosquitto_pub -h YOUR_MQTT_BROKER -t \"" + mqtt_topic + "\" -m \"Hello World!\"</pre>";
  html += "</div>";
  html += "<div class='example'>";
  html += "<strong>Python (paho-mqtt):</strong>";
  html += "<pre>import paho.mqtt.client as mqtt\n";
  html += "client = mqtt.Client()\n";
  html += "client.connect(\"YOUR_MQTT_BROKER\", 1883)\n";
  html += "client.publish(\"" + mqtt_topic + "\", \"Hello World!\")\n";
  html += "client.disconnect()</pre>";
  html += "</div>";
  html += "<div class='example'>";
  html += "<strong>Node-RED:</strong>";
  html += "<pre>[\n";
  html += "  {\"id\":\"mqtt-out\",\"type\":\"mqtt out\",\"topic\":\"" + mqtt_topic + "\",\"qos\":\"0\",\"retain\":\"false\",\"broker\":\"YOUR_BROKER\"}\n";
  html += "]</pre>";
  html += "</div>";
  html += "</div>";

  // Raw Bitmap
  html += "<div class='card'>";
  html += "<h2>2. Raw Bitmap Data</h2>";
  html += "<p>Send raw pixel data for direct LED control. Useful for animations and graphics.</p>";
  html += "<h3>Topic</h3>";
  html += "<code class='topic'>" + mqtt_raw_topic + "</code>";
  html += "<h3>Payload Format</h3>";
  html += "<p>Binary data: 7 rows × up to 100 columns, packed as bits.</p>";
  html += "<table>";
  html += "<tr><th>Parameter</th><th>Value</th></tr>";
  html += "<tr><td>Rows</td><td>7 (fixed)</td></tr>";
  html += "<tr><td>Columns</td><td>Up to 100</td></tr>";
  html += "<tr><td>Bytes per row</td><td>13 (100 bits ÷ 8, rounded up)</td></tr>";
  html += "<tr><td>Total bytes</td><td>91 max (7 rows × 13 bytes)</td></tr>";
  html += "<tr><td>Bit order</td><td>MSB first (leftmost column = bit 7)</td></tr>";
  html += "</table>";
  html += "<h3>Example (Python)</h3>";
  html += "<div class='example'>";
  html += "<pre>import paho.mqtt.client as mqtt\n\n";
  html += "# Create a simple pattern: vertical bars\n";
  html += "bitmap = bytearray(91)  # 7 rows * 13 bytes\n\n";
  html += "for row in range(7):\n";
  html += "    row_offset = row * 13\n";
  html += "    # Set every 3rd column ON (creates vertical bars)\n";
  html += "    for col in range(0, 100, 3):\n";
  html += "        byte_idx = col // 8\n";
  html += "        bit_idx = 7 - (col % 8)\n";
  html += "        bitmap[row_offset + byte_idx] |= (1 << bit_idx)\n\n";
  html += "client = mqtt.Client()\n";
  html += "client.connect(\"YOUR_MQTT_BROKER\", 1883)\n";
  html += "client.publish(\"" + mqtt_raw_topic + "\", bytes(bitmap))\n";
  html += "client.disconnect()</pre>";
  html += "</div>";
  html += "</div>";

  // Home Assistant
  html += "<div class='card'>";
  html += "<h2>3. Home Assistant Integration</h2>";
  html += "<p>LED-BARt supports Home Assistant MQTT autodiscovery.</p>";
  html += "<h3>Automatic Discovery</h3>";
  html += "<p>When LED-BARt connects to MQTT, it automatically publishes discovery messages. The device should appear in Home Assistant as <code>text.led_bart_display</code>.</p>";
  html += "<h3>Manual Configuration</h3>";
  html += "<p>Add to your <code>configuration.yaml</code>:</p>";
  html += "<div class='example'>";
  html += "<pre>text:\n";
  html += "  - platform: mqtt\n";
  html += "    name: \"LED-BARt Display\"\n";
  html += "    command_topic: \"" + mqtt_topic + "\"\n";
  html += "    max: 99\n";
  html += "    mode: text</pre>";
  html += "</div>";
  html += "<h3>Automation Example</h3>";
  html += "<div class='example'>";
  html += "<pre>automation:\n";
  html += "  - alias: \"Show weather on LED bar\"\n";
  html += "    trigger:\n";
  html += "      - platform: time_pattern\n";
  html += "        minutes: '/30'\n";
  html += "    action:\n";
  html += "      - service: text.set_value\n";
  html += "        target:\n";
  html += "          entity_id: text.led_bart_display\n";
  html += "        data:\n";
  html += "          value: \"{{ states('sensor.temperature') }}~C {{ states('sensor.weather_condition') }}\"</pre>";
  html += "</div>";
  html += "</div>";

  // Behavior
  html += "<div class='card'>";
  html += "<h2>Display Behavior</h2>";
  html += "<ul>";
  html += "<li>MQTT messages are displayed for <strong>30 seconds</strong></li>";
  html += "<li>After timeout, display returns to <strong>time/date</strong></li>";
  html += "<li>New MQTT messages reset the 30-second timer</li>";
  html += "<li>Raw bitmap mode overrides text mode</li>";
  html += "</ul>";
  html += "</div>";

  html += "<br><a href='/'>← Back to Main</a>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}
