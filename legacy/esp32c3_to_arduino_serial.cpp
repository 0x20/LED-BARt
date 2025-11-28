// LED-BARt XIAO ESP32-C3 Web Server + MQTT Controller
// Web interface for debugging and control
// Sends display data to Arduino Uno via Serial
// M. Wyns 2017, Updated 2025

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <TimeLib.h>

// WiFi credentials - UPDATE THESE (or use AP mode)
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Access Point mode (fallback if WiFi fails)
const char* ap_ssid = "LED-BARt";
const char* ap_password = "ledbart123";

// MQTT broker settings - OPTIONAL
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
const char* mqtt_user = "";  // Leave empty if no auth
const char* mqtt_password = "";
const char* mqtt_topic = "ledbar/text";

// Display buffer
String displayText = "LED-BARt Ready!";
String mqttText = "";
bool hasMqttText = false;
bool textUpdated = true;
unsigned long lastTimeUpdate = 0;
unsigned long lastMqttReceived = 0;
unsigned long lastSerialSent = 0;
unsigned long lastTestMessage = 0;
bool showColon = true;
bool useAPMode = false;

// Stats for debugging
unsigned long serialMessagesSent = 0;
unsigned long mqttMessagesReceived = 0;
unsigned long webRequestsReceived = 0;
unsigned long arduinoAcksReceived = 0;
String lastArduinoAck = "No ACK yet";
unsigned long lastAckTime = 0;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);

// Function declarations
void setup_wifi();
void setup_ap();
void reconnect_mqtt();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void sendTextToArduino(String text);
void updateTimeDisplay();
void handleRoot();
void handleSetText();
void handleStatus();
void handleNotFound();

void setup() {
  // Initialize serial for communication with Arduino Uno
  // XIAO ESP32-C3 uses Serial (USB) and Serial1 (UART pins D6/D7)
  // We'll use Serial1 for Arduino communication
  Serial.begin(115200);  // USB debug

  // ESP32-C3 needs explicit pin mapping for Serial1
  // D6 = GPIO6 (TX), D7 = GPIO7 (RX)
  Serial1.begin(115200, SERIAL_8N1, 7, 6); // baud, mode, RX=7, TX=6

  delay(1000);
  Serial.println("\n\n=================================");
  Serial.println("LED-BARt Web Server Starting...");
  Serial.println("Board: XIAO ESP32-C3");
  Serial.println("=================================");

  // Try to connect to WiFi
  setup_wifi();

  // Setup Web Server
  server.on("/", handleRoot);
  server.on("/set", handleSetText);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Web server started!");
  Serial.print("Access at: http://");
  Serial.println(WiFi.localIP());

  // Setup MQTT (optional)
  if (!useAPMode && strlen(mqtt_server) > 3) {
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqtt_callback);
    Serial.println("MQTT client configured");
  }

  // Initialize time
  setTime(1496399575);

  Serial.println("\n=================================");
  Serial.println("Setup complete!");
  Serial.println("=================================\n");

  // Send initial text
  sendTextToArduino(displayText);
}

void loop() {
  // Handle web server
  server.handleClient();

  // Check for ACK messages from Arduino
  if (Serial1.available() > 0) {
    Serial.print("Serial1 RX available: ");
    Serial.println(Serial1.available());
    String response = Serial1.readStringUntil('\n');
    Serial.print("Received: '");
    Serial.print(response);
    Serial.println("'");
    if (response.startsWith("ACK:")) {
      lastArduinoAck = response.substring(4);
      arduinoAcksReceived++;
      lastAckTime = millis();
      Serial.print("Arduino ACK received: ");
      Serial.println(lastArduinoAck);
    } else {
      Serial.println("Not an ACK message");
    }
  }

  // Maintain MQTT connection (if configured)
  if (!useAPMode && strlen(mqtt_server) > 3) {
    if (!mqttClient.connected()) {
      reconnect_mqtt();
    }
    mqttClient.loop();
  }

  // If we have MQTT text, display it for 30 seconds, then go back to time display
  if (hasMqttText && (millis() - lastMqttReceived > 30000)) {
    hasMqttText = false;
    Serial.println("MQTT text timeout, switching to time display");
  }

  // Update display every 500ms
  if (millis() - lastTimeUpdate > 500) {
    lastTimeUpdate = millis();

    if (hasMqttText) {
      // Display MQTT text
      if (displayText != mqttText) {
        displayText = mqttText;
        textUpdated = true;
      }
    } else {
      // Display time/date when no MQTT text
      updateTimeDisplay();
    }
  }

  // Send updated text to Arduino if changed
  if (textUpdated) {
    sendTextToArduino(displayText);
    textUpdated = false;
  }

  // Send test message every 5 seconds for debugging
  if (millis() - lastTestMessage > 5000) {
    lastTestMessage = millis();
    sendTextToArduino("TEST " + String(millis() / 1000) + "s");
  }

  delay(10);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed! Starting Access Point...");
    setup_ap();
  }
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

void reconnect_mqtt() {
  static unsigned long lastAttempt = 0;

  // Only try to reconnect every 5 seconds
  if (millis() - lastAttempt < 5000) {
    return;
  }
  lastAttempt = millis();

  if (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker...");

    String clientId = "ESP32C3-LEDBar-";
    clientId += String(random(0xffff), HEX);

    bool connected;
    if (strlen(mqtt_user) > 0) {
      connected = mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password);
    } else {
      connected = mqttClient.connect(clientId.c_str());
    }

    if (connected) {
      Serial.println("connected!");
      Serial.print("Subscribing to topic: ");
      Serial.println(mqtt_topic);
      mqttClient.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT message received on topic: ");
  Serial.println(topic);

  // Convert payload to string
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message: ");
  Serial.println(message);

  // Update MQTT text and flag
  mqttText = message;
  hasMqttText = true;
  lastMqttReceived = millis();
  displayText = message;
  textUpdated = true;
  mqttMessagesReceived++;
}

void sendTextToArduino(String text) {
  // Send text to Arduino Uno via Serial1 (UART)
  Serial1.print("TEXT:");
  Serial1.println(text);
  Serial1.flush();  // Wait for transmission to complete
  serialMessagesSent++;
  lastSerialSent = millis();

  // Debug to USB
  Serial.print("Sent to Arduino: TEXT:");
  Serial.println(text);
  Serial.print("Serial1 TX buffer: ");
  Serial.println(Serial1.availableForWrite());
}

void updateTimeDisplay() {
  // Toggle colon every 500ms for blinking effect
  showColon = !showColon;

  // Build time/date string
  char timeStr[20];
  sprintf(timeStr, "%02d%c%02d %02d/%02d  16.5~C",
          hour(),
          showColon ? ':' : ' ',
          minute(),
          day(),
          month());

  String newText = String(timeStr);

  // Only update if text changed
  if (newText != displayText) {
    displayText = newText;
    textUpdated = true;
  }
}

// Web Server Handlers

void handleRoot() {
  webRequestsReceived++;

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>LED-BARt Control</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; background: #f0f0f0; }";
  html += "h1 { color: #333; }";
  html += ".status { background: white; padding: 15px; border-radius: 5px; margin: 10px 0; }";
  html += ".control { background: white; padding: 15px; border-radius: 5px; margin: 10px 0; }";
  html += "input[type='text'] { width: 100%; padding: 10px; font-size: 16px; border: 1px solid #ddd; border-radius: 3px; box-sizing: border-box; }";
  html += "button { background: #4CAF50; color: white; padding: 12px 30px; font-size: 16px; border: none; border-radius: 3px; cursor: pointer; margin-top: 10px; }";
  html += "button:hover { background: #45a049; }";
  html += ".stat { margin: 5px 0; }";
  html += ".label { font-weight: bold; }";
  html += ".current-display { font-size: 24px; font-family: monospace; background: #000; color: #0f0; padding: 15px; border-radius: 5px; text-align: center; }";
  html += "</style>";
  html += "</head><body>";

  html += "<h1>LED-BARt Control Panel</h1>";
  html += "<p><small>XIAO ESP32-C3</small></p>";

  // Current Display
  html += "<div class='status'>";
  html += "<h2>Current Display</h2>";
  html += "<div class='current-display'>" + displayText + "</div>";
  html += "</div>";

  // Control Form
  html += "<div class='control'>";
  html += "<h2>Set Display Text</h2>";
  html += "<form action='/set' method='POST'>";
  html += "<input type='text' name='text' placeholder='Enter text to display...' maxlength='99'>";
  html += "<button type='submit'>Update Display</button>";
  html += "</form>";
  html += "</div>";

  // Status Info
  html += "<div class='status'>";
  html += "<h2>System Status</h2>";
  html += "<div class='stat'><span class='label'>WiFi Mode:</span> " + String(useAPMode ? "Access Point" : "Station") + "</div>";
  html += "<div class='stat'><span class='label'>IP Address:</span> " + (useAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "</div>";
  if (!useAPMode) {
    html += "<div class='stat'><span class='label'>WiFi Signal:</span> " + String(WiFi.RSSI()) + " dBm</div>";
  }
  html += "<div class='stat'><span class='label'>MQTT Connected:</span> " + String(mqttClient.connected() ? "Yes" : "No") + "</div>";
  html += "<div class='stat'><span class='label'>Serial Messages Sent:</span> " + String(serialMessagesSent) + "</div>";
  html += "<div class='stat'><span class='label'>Arduino ACKs Received:</span> " + String(arduinoAcksReceived) + "</div>";
  html += "<div class='stat'><span class='label'>Last Arduino ACK:</span> " + lastArduinoAck + "</div>";
  if (lastAckTime > 0) {
    html += "<div class='stat'><span class='label'>Last ACK Time:</span> " + String((millis() - lastAckTime) / 1000) + " seconds ago</div>";
  }
  html += "<div class='stat'><span class='label'>MQTT Messages Received:</span> " + String(mqttMessagesReceived) + "</div>";
  html += "<div class='stat'><span class='label'>Web Requests:</span> " + String(webRequestsReceived) + "</div>";
  html += "<div class='stat'><span class='label'>Uptime:</span> " + String(millis() / 1000) + " seconds</div>";
  html += "<div class='stat'><span class='label'>Free Heap:</span> " + String(ESP.getFreeHeap()) + " bytes</div>";
  html += "</div>";

  html += "<div class='status'>";
  html += "<h2>Quick Actions</h2>";
  html += "<button onclick=\"fetch('/set?text=Hello World!');\">Hello World!</button> ";
  html += "<button onclick=\"fetch('/set?text=Test 123');\">Test 123</button> ";
  html += "<button onclick=\"fetch('/set?text=LED-BARt!');\">LED-BARt!</button> ";
  html += "<button onclick=\"location.reload();\">Refresh</button>";
  html += "</div>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSetText() {
  webRequestsReceived++;

  String newText = "";

  if (server.hasArg("text")) {
    newText = server.arg("text");
  }

  if (newText.length() > 0) {
    displayText = newText;
    mqttText = newText;
    hasMqttText = true;
    lastMqttReceived = millis();
    textUpdated = true;

    Serial.println("Web: Display text updated to: " + newText);

    server.sendHeader("Location", "/");
    server.send(303);
  } else {
    server.send(400, "text/plain", "Missing 'text' parameter");
  }
}

void handleStatus() {
  webRequestsReceived++;

  String json = "{";
  json += "\"display_text\":\"" + displayText + "\",";
  json += "\"wifi_mode\":\"" + String(useAPMode ? "AP" : "STA") + "\",";
  json += "\"ip\":\"" + (useAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  json += "\"mqtt_connected\":" + String(mqttClient.connected() ? "true" : "false") + ",";
  json += "\"serial_sent\":" + String(serialMessagesSent) + ",";
  json += "\"mqtt_received\":" + String(mqttMessagesReceived) + ",";
  json += "\"web_requests\":" + String(webRequestsReceived) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
  json += "}";

  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}
