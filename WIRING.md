# LED-BARt Direct ESP32-C3 Wiring Guide

## Overview

This configuration drives the LED bar **directly from the ESP32-C3** without an Arduino. Level shifters are required since the ESP32-C3 uses 3.3V logic and the LED bar requires 5V signals.

## Hardware Components

- **XIAO ESP32-C3**: WiFi/MQTT controller + LED bar driver (3.3V logic)
- **Level Shifters**: Convert 3.3V → 5V for LED bar control
- **LED Bar Display**: 5V powered LED matrix
- **Power Supply**: 5V for LED bar and ESP32

## Pin Connections

### ESP32-C3 to Level Shifters

| ESP32-C3 Pin | Level Shifter Connection | Purpose |
|--------------|-------------------------|---------|
| D2 (GPIO2)   | A1 (LV side)           | Shift register clock (KLOK) |
| D3 (GPIO3)   | A2 (LV side)           | Shift register data (DATA) |
| D4 (GPIO4)   | A3 (LV side)           | Row 1 |
| D5 (GPIO5)   | A4 (LV side)           | Row 2 |
| D6 (GPIO6)   | A5 (LV side)           | Row 3 |
| D7 (GPIO7)   | A6 (LV side)           | Row 4 |
| D8 (GPIO8)   | A7 (LV side)           | Row 5 |
| D9 (GPIO9)   | A8 (LV side)           | Row 6 |
| D10 (GPIO10) | A9 (LV side)           | Row 7 |
| **3.3V**     | **VCCB (LV power)**    | **Low voltage power rail** |
| **GND**      | **GND**                | **Common ground** |

### Level Shifter Power

| Power Rail | Source | Purpose |
|------------|--------|---------|
| **VCCB (LV)** | **ESP32 3.3V pin** | Powers low-voltage side of level shifter |
| **VCCA (HV)** | **5V supply** | Powers high-voltage side of level shifter |
| **GND** | **Common ground** | Shared ground between all components |

### Level Shifter to LED Bar

| Level Shifter Pin | LED Bar Connection | Purpose |
|-------------------|-------------------|---------|
| B1 (HV side)      | Shift Register Clock | Clock signal |
| B2 (HV side)      | Shift Register Data  | Data signal |
| B3 (HV side)      | Row 1 | Row driver |
| B4 (HV side)      | Row 2 | Row driver |
| B5 (HV side)      | Row 3 | Row driver |
| B6 (HV side)      | Row 4 | Row driver |
| B7 (HV side)      | Row 5 | Row driver |
| B8 (HV side)      | Row 6 | Row driver |
| B9 (HV side)      | Row 7 | Row driver |

### LED Bar Power

| LED Bar Pin | Source | Purpose |
|-------------|--------|---------|
| **VCC/5V** | **5V supply** | Powers LED matrix |
| **GND** | **Common ground** | Ground connection |

**Total: 9 data lines + power/ground**

## Complete Wiring Diagram

```
5V Supply
  ├─→ LED Bar VCC (powers LEDs)
  ├─→ Level Shifter VCCA (HV power rail)
  └─→ ESP32-C3 5V pin (optional, or use USB-C)

ESP32-C3 3.3V
  └─→ Level Shifter VCCB (LV power rail)

Common Ground (CRITICAL!)
  ├─→ ESP32-C3 GND
  ├─→ Level Shifter GND
  ├─→ LED Bar GND
  └─→ 5V Supply GND

Data Path:
ESP32 D2-D10 → Level Shifter A1-A9 (3.3V)
                     ↓ voltage shift ↓
             Level Shifter B1-B9 (5V) → LED Bar signals
```

**⚠️ CRITICAL**: All grounds must be connected together!

## Recommended Level Shifter

**TXS0108E** or **TXB0108** - 8-channel bidirectional level shifter
- You'll need **two** of these (one for 8 signals, one for the 9th)
- Or use **TXS0109E** / **TXB0109** - 9-channel version (single chip)

Alternative: **74HCT245** - Unidirectional 8-bit level shifter (cheaper, works fine for this application since we only need 3.3V→5V, not bidirectional)

## Power Supply

### Option 1: Single 5V Supply (Recommended)
1. Connect **5V DC** to LED bar power rail
2. ESP32-C3 can be powered from:
   - USB-C (for development)
   - 5V pin (requires onboard regulator, check your board)
3. Level shifters get 5V on HV side, 3.3V from ESP32 on LV side

### Option 2: Separate Supplies
1. **5V supply** → LED bar + level shifter HV side
2. **3.3V supply** (or USB) → ESP32-C3 + level shifter LV side
3. **Important**: Connect grounds together

## Flashing Instructions

```bash
# Flash the ESP32-C3 with direct driver firmware
platformio run -e xiao_esp32c3_direct --target upload

# Monitor serial output
platformio device monitor -e xiao_esp32c3_direct
```

## Initial Setup

1. **First Boot**: ESP32 creates WiFi access point
   - **SSID**: `LED-BARt`
   - **Password**: `ledbart123`

2. **Connect** to the WiFi network

3. **Open browser** to `http://192.168.4.1`

4. **Configure WiFi**:
   - Click "Configuration"
   - Enter your WiFi SSID and password
   - Click "Save & Reboot"

5. **Device reboots** and connects to your WiFi

6. **Access via mDNS**: `http://ledbart.local` (or use IP from router)

## WiFi Configuration

The ESP32-C3 stores WiFi credentials in flash memory (Preferences API).

### Configuration Storage:
- **WiFi SSID**: Stored in `preferences["wifi_ssid"]`
- **WiFi Password**: Stored in `preferences["wifi_pass"]`
- **MQTT Server**: Stored in `preferences["mqtt_srv"]`
- **MQTT Port**: Stored in `preferences["mqtt_port"]`
- **MQTT Topics**: Stored in `preferences["mqtt_topic"]` and `preferences["mqtt_raw"]`

### Factory Reset:
To reset to AP mode, you can either:
1. Use the web interface to clear WiFi settings
2. Or reflash the firmware (this will erase preferences)

## MQTT Configuration

### Basic Setup:
1. Open web interface (`http://ledbart.local`)
2. Click "Configuration"
3. Enter MQTT broker details:
   - **Server**: IP or hostname (e.g., `192.168.1.100` or `homeassistant.local`)
   - **Port**: Usually `1883`
   - **Username/Password**: If required by your broker
   - **Topics**: Default are `ledbart/text` and `ledbart/raw`

### Topics:

#### Text Display: `ledbart/text`
Send plain text messages (max 99 characters)
```bash
mosquitto_pub -h YOUR_BROKER -t "ledbart/text" -m "Hello World!"
```

#### Raw Bitmap: `ledbart/raw`
Send raw pixel data (7 rows × up to 100 columns)
- Format: Binary data, packed bits
- Size: Up to 91 bytes (7 rows × 13 bytes per row)
- Bit order: MSB first

## Home Assistant Integration

LED-BARt automatically publishes MQTT autodiscovery messages for Home Assistant.

### Automatic Discovery:
Once connected to MQTT, the device appears as `text.led_bart_display`

### Manual Configuration:
Add to `configuration.yaml`:
```yaml
text:
  - platform: mqtt
    name: "LED-BARt Display"
    command_topic: "ledbart/text"
    max: 99
```

### Example Automation:
```yaml
automation:
  - alias: "Show time on LED bar"
    trigger:
      - platform: time_pattern
        minutes: '/1'
    action:
      - service: text.set_value
        target:
          entity_id: text.led_bart_display
        data:
          value: "{{ now().strftime('%H:%M') }}"
```

For more examples, visit `/mqtt-help` in the web interface.

## Display Behavior

### Core Functionality (Always Works):
- ✅ Powers on → Shows "ESP32 Ready!" for 2 seconds
- ✅ Then displays time/date with blinking colon
- ✅ Updates every 500ms
- ✅ Display orientation matches Arduino version (tested & working)

### Web Interface (Optional):
- Set custom text via web form
- Quick action buttons
- Real-time status display
- Configuration management

### MQTT (Optional):
- Text messages displayed for 30 seconds
- Then returns to time display
- New messages reset the timer
- Raw bitmap mode for animations/graphics

## Debugging

### Serial Monitor (USB):
```bash
platformio device monitor -e xiao_esp32c3_direct
```

You'll see:
- WiFi connection status
- IP address
- MQTT connection status
- Incoming messages

### Web Interface:
Access `http://ledbart.local` or `http://192.168.4.1` (AP mode) to see:
- Current display text
- WiFi signal strength
- MQTT connection status
- Uptime and memory usage
- Configuration options

### LED Indicators:
- **Orange LED (power)**: Should always be on
- **Green LED (builtin)**: Available for status indication

### Common Issues:

**LED bar not displaying:**
- Check level shifter connections
- Verify 5V power to LED bar
- Check serial monitor for errors

**WiFi not connecting:**
- Check SSID/password in configuration
- Look for "LED-BARt" AP (fallback mode)
- Check serial monitor for connection attempts

**MQTT not working:**
- Verify broker IP/hostname is correct
- Check broker is running: `mosquitto_sub -h YOUR_BROKER -v -t '#'`
- Check serial monitor for MQTT connection status
- MQTT is optional - core display will still work

**Web interface unreachable:**
- In AP mode: Connect to "LED-BARt" WiFi first
- In STA mode: Check router for IP or use `ledbart.local`
- Try IP address instead of mDNS if `.local` doesn't work

## Pin Reference

### XIAO ESP32-C3 Pinout (for LED bar)

```
        USB-C
   ┌───────────┐
D0 │           │ D10 → Row 7 (via level shifter)
D1 │           │ D9  → Row 6 (via level shifter)
D2 │ KLOK      │ D8  → Row 5 (via level shifter)
D3 │ DATA      │ D7  → Row 4 (via level shifter)
D4 │ Row 1     │ D6  → Row 3 (via level shifter)
D5 │ Row 2     │ 3V3 → Level shifter VCCB (LV power)
GND│           │ GND → Common ground
5V │           │ 5V  → Level shifter VCCA (HV power) + LED Bar VCC
   └───────────┘
```

**Power connections (right side, bottom):**
- **3V3**: Powers level shifter low-voltage side (VCCB)
- **GND**: Common ground for everything
- **5V**: Powers level shifter high-voltage side (VCCA) AND LED bar VCC

All data pins go through level shifters before connecting to LED bar.

## Level Shifter Wiring Example (TXS0108E)

```
TXS0108E Chip 1:
  LV (3.3V) ← ESP32 3.3V
  GND ← Common ground
  HV (5V) ← 5V supply
  A1 ← ESP32 D2 (KLOK)
  A2 ← ESP32 D3 (DATA)
  A3 ← ESP32 D4 (Row 1)
  A4 ← ESP32 D5 (Row 2)
  A5 ← ESP32 D6 (Row 3)
  A6 ← ESP32 D7 (Row 4)
  A7 ← ESP32 D8 (Row 5)
  A8 ← ESP32 D9 (Row 6)
  B1 → LED Bar Clock
  B2 → LED Bar Data
  B3 → LED Bar Row 1
  B4 → LED Bar Row 2
  B5 → LED Bar Row 3
  B6 → LED Bar Row 4
  B7 → LED Bar Row 5
  B8 → LED Bar Row 6

TXS0108E Chip 2 (or use single TXS0109E):
  LV (3.3V) ← ESP32 3.3V
  GND ← Common ground
  HV (5V) ← 5V supply
  A1 ← ESP32 D10 (Row 7)
  B1 → LED Bar Row 7
```

## Advantages Over Arduino Version

1. **Single microcontroller**: No serial communication issues
2. **WiFi built-in**: Web interface + MQTT
3. **Configuration storage**: WiFi/MQTT settings saved in flash
4. **mDNS support**: Access via `ledbart.local`
5. **Home Assistant integration**: Autodiscovery support
6. **More features**: Raw bitmap mode for graphics
7. **Smaller footprint**: One chip instead of two
8. **Easier debugging**: Web interface shows all status

## Code Architecture

### Core Display (Highest Priority):
- LED refresh runs continuously in main loop
- Display code taken directly from working Arduino version
- Same font, same timing, same row reversal logic
- **Guaranteed to work** - uses proven code

### Web Server (Secondary):
- Non-blocking
- Handles requests between display refreshes
- Won't interfere with LED timing

### MQTT (Optional):
- Only connects if configured
- Non-blocking
- Failures don't affect core display

### Configuration (Persistent):
- Uses ESP32 Preferences library
- Survives reboots
- Factory resetable via web interface

## Performance Notes

- **Display refresh**: ~1500μs per row × 7 rows = ~10ms per frame
- **Frame rate**: ~100 Hz
- **Web server**: Handles requests between frames
- **MQTT**: Publishes/subscribes without blocking
- **No lag**: Display is always smooth and responsive
