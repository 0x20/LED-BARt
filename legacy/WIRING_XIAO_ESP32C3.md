# LED-BARt Wiring Guide - XIAO ESP32-C3 Version

## Overview

This version uses the Seeed Studio XIAO ESP32-C3 instead of the D1 Mini. The XIAO is much smaller but equally capable for this project.

## Hardware Components

- **XIAO ESP32-C3**: WiFi/MQTT controller (3.3V logic)
- **Arduino Uno**: LED bar driver (5V logic)
- **LED Bar Display**: 5V powered LED matrix

## Pin Connections

### XIAO ESP32-C3 to Arduino Uno

| XIAO ESP32-C3 | Arduino Uno | Purpose |
|---------------|-------------|---------|
| D6 (TX)       | RX (Pin 0)  | Serial data to Arduino |
| D7 (RX)       | TX (Pin 1)  | ACK data from Arduino |
| GND           | GND         | Common ground |
| 5V            | 5V          | Power XIAO from Arduino |

**Important Notes:**
- The XIAO ESP32-C3 uses **Serial1** for UART communication (pins D6/D7)
- **Serial** (USB) is used for debugging
- D6 is TX on Serial1, D7 is RX (we only use TX → Arduino RX)
- The XIAO can be powered from the Arduino's 5V pin (it has an onboard 3.3V regulator)

### Arduino Uno to LED Bar

| Arduino Pin | LED Bar Connection | Purpose |
|-------------|-------------------|---------|
| Pin 13      | Shift Register Clock | Clock signal |
| Pin 12      | Shift Register Data  | Data signal |
| Pin 2       | Row 1 | Row driver |
| Pin 3       | Row 2 | Row driver |
| Pin 4       | Row 3 | Row driver |
| Pin 5       | Row 4 | Row driver |
| Pin 6       | Row 5 | Row driver |
| Pin 7       | Row 6 | Row driver |
| Pin 8       | Row 7 | Row driver |

**Total: 9 wires from Arduino to LED Bar**

## Power Supply

### Single Power Supply (Recommended)

1. Connect **7-12V DC** or **USB** to Arduino Uno
2. Arduino powers the XIAO via the 5V pin
3. XIAO's onboard regulator converts to 3.3V
4. LED bar gets 5V from its own power supply

### Power LED Indicators

**XIAO ESP32-C3:**
- Orange LED: Power indicator (should always be on)
- Green LED: User LED (can be controlled in code)

**Arduino Uno:**
- ON LED: Power indicator
- L LED: Pin 13 LED (blinks with clock signal)
- TX/RX LEDs: Serial communication

## Flashing Instructions

### Flash XIAO ESP32-C3

```bash
# Make sure XIAO is connected via USB-C
platformio run -e xiao_esp32c3 --target upload

# Monitor output
platformio device monitor -e xiao_esp32c3
```

**Note:** The XIAO ESP32-C3 enters bootloader mode automatically. No need to hold BOOT button for normal flashing.

### Flash Arduino Uno

```bash
# Disconnect XIAO TX wire from Arduino RX first!
platformio run -d arduino_uno_project --target upload

# Reconnect XIAO TX wire after flashing
```

## WiFi Configuration

Edit `src/main_xiao_esp32c3.cpp`:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### Access Point Mode (Default Fallback)

If WiFi connection fails, the XIAO creates its own network:

- **SSID**: `LED-BARt`
- **Password**: `ledbart123`
- **IP Address**: `192.168.4.1`

Connect to this network and access the web interface at `http://192.168.4.1`

## MQTT Configuration (Optional)

Edit `src/main_xiao_esp32c3.cpp`:

```cpp
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
const char* mqtt_topic = "ledbar/text";
```

Publish text to `ledbar/text` topic to update the display.

## Serial Communication Protocol

### XIAO to Arduino (via Serial1)

Format: `TEXT:your message here\n`

Example:
```
TEXT:Hello World!
```

### Arduino to XIAO (ACK)

Format: `ACK:received message\n`

Example:
```
ACK:Hello World!
```

## Debugging

### USB Serial Output (XIAO)

```bash
screen /dev/cu.usbmodem* 115200
# or
platformio device monitor -e xiao_esp32c3
```

### Check XIAO Detection

```bash
ls /dev/cu.usbmodem*
```

The XIAO should appear as `/dev/cu.usbmodem*` (number varies)

### Web Interface

1. Connect to WiFi network "LED-BARt" (password: ledbart123)
2. Open browser to `http://192.168.4.1`
3. View stats:
   - Serial messages sent
   - Arduino ACKs received
   - Last ACK message
   - Uptime and memory usage

## Advantages of XIAO ESP32-C3 over D1 Mini

1. **Smaller size**: 21mm x 17.5mm vs 34.2mm x 25.6mm
2. **USB-C**: More modern connector
3. **RISC-V processor**: ESP32-C3 is newer architecture
4. **Better WiFi**: More stable WiFi stack
5. **Separate UART**: Serial1 for Arduino, Serial for debug
6. **Lower power**: Better power efficiency
7. **More GPIO**: 11 GPIO pins available

## Pin Reference

### XIAO ESP32-C3 Pinout

```
       USB-C
   ┌───────────┐
D0 │           │ D10
D1 │           │ D9
D2 │           │ D8
D3 │           │ D7 (RX - Serial1)
D4 │           │ D6 (TX - Serial1) ← Connect to Arduino RX
D5 │           │ 3V3
GND│           │ GND ← Connect to Arduino GND
5V │           │ 5V  ← Connect to Arduino 5V
   └───────────┘
```

## Troubleshooting

### XIAO not detected
- Try a different USB-C cable (must support data)
- Check if drivers are installed (usually automatic on macOS)
- Try different USB port

### No ACK from Arduino
- Check TX wire connection (D6 to Arduino Pin 0)
- Verify Arduino is flashed and running
- Check serial monitor for "TEXT:" messages being sent

### WiFi not connecting
- Check SSID and password in code
- Look for "LED-BARt" AP network (fallback mode)
- Check serial monitor for WiFi status messages

### Web interface unreachable
- Make sure you're connected to "LED-BARt" WiFi
- Try `192.168.4.1` instead of hostname
- Wait 10 seconds after power-up for AP to start
