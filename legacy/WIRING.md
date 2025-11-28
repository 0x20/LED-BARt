# LED-BARt Wiring Guide

## Overview
This setup uses two microcontrollers:
- **D1 Mini (ESP8266)**: Runs WiFi/MQTT logic, communicates via Serial
- **Arduino Uno**: Controls the 5V LED bar display (receives data from D1 Mini)

## D1 Mini to Arduino Uno Connections

| D1 Mini Pin | Arduino Uno Pin | Function |
|-------------|-----------------|----------|
| TX (GPIO1) | RX (Pin 0) | Serial data from D1 to Uno |
| GND | GND | Common ground |
| 5V | 5V | Power D1 Mini from Arduino |

**Note**: The D1 Mini can be powered directly from the Arduino Uno's 5V pin. The D1 Mini has an onboard voltage regulator that converts 5V to 3.3V for the ESP8266 chip.

## Arduino Uno to LED Bar Connections

### Shift Register Control
| Arduino Pin | Function | LED Bar Connection |
|-------------|----------|-------------------|
| Pin 13 | KLOK (Clock) | Shift register clock |
| Pin 12 | DATA (Data) | Shift register data input |

### Row Control (7 rows)
| Arduino Pin | LED Bar Row |
|-------------|-------------|
| Pin 2 | Row 1 |
| Pin 3 | Row 2 |
| Pin 4 | Row 3 |
| Pin 5 | Row 4 |
| Pin 6 | Row 5 |
| Pin 7 | Row 6 |
| Pin 8 | Row 7 |

## Power Supply Options

### Option 1: Single Power Supply (Recommended)
Power the Arduino Uno with a 5V power supply (USB or 7-12V DC barrel jack), and the D1 Mini gets power from the Arduino's 5V pin.

**Connections:**
- Arduino Uno 5V pin → D1 Mini 5V pin
- Arduino Uno GND → D1 Mini GND
- Arduino Uno powered via USB or DC barrel jack (7-12V recommended)

**Power Requirements:**
- D1 Mini draws ~80mA during WiFi transmission
- Arduino Uno can supply up to 500mA from its 5V pin (when powered via USB) or more via barrel jack
- Total current draw: Arduino + D1 Mini + LED Bar

### Option 2: Separate Power Supplies
If the LED bar draws significant current, use separate power supplies:
- Arduino Uno: 5V USB or 7-12V DC barrel jack
- D1 Mini: Separate 5V USB power supply
- **Must connect grounds together!**

## Serial Communication Voltage Levels

**Safe Configuration:**
- D1 Mini TX (3.3V) → Arduino Uno RX: **SAFE** - 3.3V is recognized as HIGH by Arduino
- Arduino Uno TX (5V) → D1 Mini RX: **Not used in this project** (one-way communication only)

The current design only sends data from D1 Mini to Arduino, so no level shifter is needed!

## Building and Flashing

### For D1 Mini with MQTT:
```bash
# Configure WiFi and MQTT settings in src/main_mqtt.cpp first!
platformio run -e d1_mini_mqtt --target upload
```

### For Arduino Uno:
```bash
platformio run -e uno --target upload
```

### For original D1 Mini standalone (no Arduino):
```bash
platformio run -e d1_mini --target upload
```

## MQTT Configuration

Edit `src/main_mqtt.cpp` and update these settings:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const char* mqtt_topic = "ledbar/text";
```

## MQTT Usage

Send text to display:
```bash
mosquitto_pub -h YOUR_MQTT_BROKER -t ledbar/text -m "Hello World!"
```

The D1 Mini will receive the message and send it to the Arduino Uno for display.

## Serial Protocol

The D1 Mini sends text to Arduino in this format:
```
TEXT:your message here\n
```

## Monitoring

Monitor D1 Mini output:
```bash
platformio device monitor -e d1_mini_mqtt
```

Monitor Arduino Uno output:
```bash
platformio device monitor -e uno
```

## Complete Wiring Diagram

```
┌─────────────────────┐
│   D1 Mini (ESP8266) │
│                     │
│  TX ────────────────┼──→ RX (Pin 0)
│  GND ───────────────┼──→ GND         ┌──────────────────┐
│  5V ────────────────┼──→ 5V          │   Arduino Uno    │
└─────────────────────┘                │                  │
                                       │ Pin 13 ──────────┼──→ Clock (Shift Register)
                                       │ Pin 12 ──────────┼──→ Data (Shift Register)
                                       │                  │
    ┌──────────────────────────────────┤ Pin 2  ──────────┼──→ Row 1
    │                                  │ Pin 3  ──────────┼──→ Row 2
    │                                  │ Pin 4  ──────────┼──→ Row 3
    │                                  │ Pin 5  ──────────┼──→ Row 4
    │  Power Supply (7-12V DC)         │ Pin 6  ──────────┼──→ Row 5
    │  or USB 5V                       │ Pin 7  ──────────┼──→ Row 6
    └──→ DC Jack / USB                 │ Pin 8  ──────────┼──→ Row 7
                                       └──────────────────┘
                                                │
                                                ↓
                                       ┌──────────────────┐
                                       │  LED Bar Display │
                                       │    (7 rows)      │
                                       └──────────────────┘
```

### Summary of Connections

**Power:**
- 1 power supply to Arduino Uno (USB or 7-12V DC)
- D1 Mini powered from Arduino 5V pin

**Signal (3 wires between D1 Mini and Arduino):**
1. D1 Mini TX → Arduino RX (Pin 0)
2. D1 Mini GND → Arduino GND
3. D1 Mini 5V → Arduino 5V

**LED Bar (9 wires from Arduino to LED bar):**
1. Arduino Pin 13 → Shift Register Clock
2. Arduino Pin 12 → Shift Register Data
3. Arduino Pins 2-8 → LED Bar Rows 1-7 (7 wires)
