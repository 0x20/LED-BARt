// LED-BARt D1 Mini MQTT Controller
// Connects to WiFi and MQTT broker, receives text messages
// Sends display data to Arduino Uno via Serial
// M. Wyns 2017, Updated 2025

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <TimeLib.h>

// WiFi credentials - UPDATE THESE
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT broker settings - UPDATE THESE
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
const char* mqtt_user = "YOUR_MQTT_USER";  // Leave empty if no auth
const char* mqtt_password = "YOUR_MQTT_PASSWORD";  // Leave empty if no auth
const char* mqtt_topic = "ledbar/text";  // Topic to subscribe to

// Display buffer
String displayText = "LED-BARt Ready!";
String mqttText = "";  // Text received from MQTT
bool hasMqttText = false;  // Flag to track if we have MQTT text
bool textUpdated = true;  // Start with true to show initial message
unsigned long lastTimeUpdate = 0;
unsigned long lastMqttReceived = 0;
bool showColon = true;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Function declarations
void setup_wifi();
void reconnect_mqtt();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void sendTextToArduino(String text);
void updateTimeDisplay();

void setup() {
  // Initialize serial for communication with Arduino Uno
  Serial.begin(115200);

  delay(1000);
  Serial.println("\n\nD1 Mini MQTT Controller Starting...");

  // Setup WiFi
  setup_wifi();

  // Setup MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqtt_callback);

  // Initialize time
  setTime(1496399575);  // Set initial time (update with NTP if needed)

  Serial.println("Setup complete!");
}

void loop() {
  // Maintain MQTT connection
  if (!mqttClient.connected()) {
    reconnect_mqtt();
  }
  mqttClient.loop();

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
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed!");
  }
}

void reconnect_mqtt() {
  // Attempt to connect to MQTT broker
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker...");

    String clientId = "ESP8266-LEDBar-";
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
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
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
}

void sendTextToArduino(String text) {
  // Send text to Arduino Uno via Serial
  Serial.print("TEXT:");
  Serial.println(text);
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
