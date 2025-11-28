# LED-BARt

**WiFi-enabled LED matrix display controller with MQTT and Home Assistant integration**

![LED-BARt](https://img.shields.io/badge/Platform-ESP32--C3-blue)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Ready-orange)
![License](https://img.shields.io/badge/License-MIT-green)

Display scrolling text, time, weather, or any data on a vintage LED bar display - controlled via WiFi, web interface, or MQTT.

## Features

✅ **Direct ESP32-C3 Control** - Single microcontroller drives LED bar directly
✅ **WiFi Configuration** - Easy web-based setup, persistent storage
✅ **Web Interface** - Control display from any device on your network
✅ **MQTT Support** - Integrate with Home Assistant, Node-RED, etc.
✅ **Raw Bitmap Mode** - Send custom graphics and animations
✅ **mDNS Support** - Access via `http://ledbart.local`
✅ **Robust Architecture** - Core display always works, extras don't break it
✅ **Home Assistant Autodiscovery** - Automatic integration

## Quick Start

### Hardware Required

- **XIAO ESP32-C3** (or any ESP32-C3 board)
- **5V LED bar display** (7-row matrix with shift registers)
- **Level shifters** (TXS0108E/TXB0109 or 74HCT245) for 3.3V→5V conversion
- **5V power supply**
- **9 GPIO connections** + power/ground

### 1. Flash Firmware

```bash
# Install PlatformIO if needed
pip install platformio

# Clone repository
git clone https://github.com/yourusername/LED-BARt
cd LED-BARt

# Flash ESP32-C3
platformio run -e xiao_esp32c3_direct --target upload

# Monitor serial output (optional)
platformio device monitor -e xiao_esp32c3_direct
```

### 2. First Setup

1. Power on - LED bar displays **"ESP32 Ready!"** for 2 seconds
2. Device creates WiFi access point:
   - **SSID**: `LED-BARt`
   - **Password**: `ledbart123`
3. Connect to WiFi and open browser to `http://192.168.4.1`
4. Click **"Configuration"** and enter your WiFi credentials
5. Save & reboot

### 3. Configure

After reboot, device connects to your WiFi network.

Access via:
- **mDNS**: `http://ledbart.local` (recommended)
- **IP Address**: Check your router or serial monitor

Configure MQTT (optional):
1. Open web interface
2. Go to **Configuration**
3. Enter MQTT broker details
4. Save & reboot

## Wiring

See **[WIRING.md](WIRING.md)** for detailed pinout and level shifter connections.

### Quick Reference

| Component | Pin | Connects To | Purpose |
|-----------|-----|-------------|---------|
| **ESP32-C3** | D2-D10 | Level shifter A1-A9 (LV) | Data signals |
| | 3.3V | Level shifter VCCB | Powers LV side |
| | GND | Common ground | Ground |
| **Level Shifter** | VCCB (LV) | ESP32 3.3V | LV power |
| | VCCA (HV) | 5V supply | HV power |
| | A1-A9 | ESP32 D2-D10 | LV data in |
| | B1-B9 | LED bar signals | HV data out |
| | GND | Common ground | Ground |
| **LED Bar** | Clock/Data/Rows | Level shifter B1-B9 | Control signals |
| | VCC | 5V supply | LED power |
| | GND | Common ground | Ground |

**Critical**: Level shifter needs BOTH 3.3V (from ESP32) AND 5V (from supply) to operate!

## Usage

### Web Interface

Control your display through any browser:

- **Main Page**: Set text, quick actions, system status
- **Configuration**: WiFi and MQTT settings
- **MQTT Help**: Full documentation and examples

Access at `http://ledbart.local` or your device's IP address.

### MQTT Control

#### Text Messages

Topic: `ledbart/text`
Payload: Plain text (max 99 characters)

```bash
# Using mosquitto_pub
mosquitto_pub -h YOUR_BROKER -t "ledbart/text" -m "Hello World!"

# Python example
import paho.mqtt.client as mqtt
client = mqtt.Client()
client.connect("YOUR_BROKER", 1883)
client.publish("ledbart/text", "Hello World!")
client.disconnect()
```

#### Raw Bitmap Graphics

Topic: `ledbart/raw`
Payload: Binary bitmap data (7 rows × up to 100 columns, bit-packed)

```python
import paho.mqtt.client as mqtt

# Create 7 rows × 100 columns bitmap
bitmap = bytearray(91)  # 7 rows × 13 bytes per row

# Set pixels (example: vertical bars)
for row in range(7):
    row_offset = row * 13
    for col in range(0, 100, 3):
        byte_idx = col // 8
        bit_idx = 7 - (col % 8)
        bitmap[row_offset + byte_idx] |= (1 << bit_idx)

client = mqtt.Client()
client.connect("YOUR_BROKER", 1883)
client.publish("ledbart/raw", bytes(bitmap))
client.disconnect()
```

### Home Assistant

LED-BARt automatically appears in Home Assistant when connected to MQTT.

**Entity**: `text.led_bart_display`

#### Example Automation

```yaml
automation:
  - alias: "Show weather on LED bar"
    trigger:
      - platform: time_pattern
        minutes: '/30'
    action:
      - service: text.set_value
        target:
          entity_id: text.led_bart_display
        data:
          value: "{{ states('sensor.temperature') }}°C {{ states('sensor.weather_condition') }}"
```

## Display Behavior

### Default Mode (No MQTT)
- Shows current **time and date**
- Format: `HH:MM DD/MM 16.5~C`
- Colon blinks every 500ms

### MQTT Text Mode
- Displays received text
- Shows for **30 seconds**
- Then returns to time display
- New messages reset timer

### MQTT Raw Bitmap Mode
- Displays custom graphics
- Shows for **30 seconds**
- Overrides text mode
- Perfect for animations

## Configuration Storage

All settings are stored in ESP32 flash memory using the Preferences API:

- WiFi SSID & Password
- MQTT broker address & credentials
- MQTT topics

Settings survive reboots and power cycles.

To factory reset, reflash firmware or clear via web interface.

## Architecture

### Core Display (Always Works)
- LED refresh runs continuously at ~100Hz
- Uses proven Arduino display code
- Non-blocking design
- **Guaranteed reliable**

### Web Server (Optional)
- Handles requests between display refreshes
- Won't interfere with LED timing
- Falls back to AP mode if WiFi fails

### MQTT (Optional)
- Only connects if configured
- Non-blocking operation
- Failures don't affect core display

Each layer is independent - if MQTT fails, web still works. If web fails, display still shows time.

## API Reference

### HTTP Endpoints

- `GET /` - Main interface
- `GET /config` - Configuration page
- `GET /mqtt-help` - MQTT documentation
- `POST /saveconfig` - Save configuration
- `GET /status` - JSON status
- `GET /status?text=Hello` - Set text via URL

### MQTT Topics

| Topic | Direction | Format | Description |
|-------|-----------|--------|-------------|
| `ledbart/text` | Subscribe | String | Display text message |
| `ledbart/raw` | Subscribe | Binary | Display raw bitmap |
| `homeassistant/text/ledbart/config` | Publish | JSON | HA autodiscovery |

## Troubleshooting

### LED bar not lighting up
- ✓ Check 5V power to LED bar
- ✓ Verify level shifter connections
- ✓ Check serial monitor for errors
- ✓ Ensure correct pin mappings

### WiFi not connecting
- ✓ Check SSID/password in configuration
- ✓ Look for "LED-BARt" AP (fallback mode)
- ✓ Check serial monitor for connection status
- ✓ Verify 2.4GHz WiFi (ESP32 doesn't support 5GHz)

### mDNS not working (ledbart.local)
- ✓ Try IP address instead
- ✓ Check router supports mDNS/Bonjour
- ✓ Some corporate networks block mDNS
- ✓ Use serial monitor to see IP address

### MQTT not connecting
- ✓ Verify broker IP/hostname
- ✓ Test broker: `mosquitto_sub -h YOUR_BROKER -v -t '#'`
- ✓ Check username/password if required
- ✓ Remember: MQTT is optional, display still works

### Display shows wrong text orientation
- ✓ Code uses proven Arduino pin mappings
- ✓ Verify row pins D4-D10 connected in order
- ✓ Check level shifter polarity

## Development

### Project Structure

```
LED-BARt/
├── src/
│   └── main_esp32c3_direct.cpp    # Main firmware
├── legacy/                         # Old implementations
│   ├── arduino_uno_project/        # Arduino-based version
│   ├── main_*.cpp                  # ESP8266/D1 Mini versions
│   └── README.md                   # Legacy documentation
├── platformio.ini                  # Build configuration
├── WIRING.md                       # Detailed wiring guide
└── README.md                       # This file
```

### Building

```bash
# Build only (no upload)
platformio run -e xiao_esp32c3_direct

# Build and upload
platformio run -e xiao_esp32c3_direct --target upload

# Clean build
platformio run -e xiao_esp32c3_direct --target clean

# Monitor serial output
platformio device monitor -e xiao_esp32c3_direct
```

### Customization

Edit `src/main_esp32c3_direct.cpp`:

```cpp
// Change AP credentials
const char* ap_ssid = "LED-BARt";
const char* ap_password = "ledbart123";

// Change mDNS hostname
const char* hostname = "ledbart";

// Change default MQTT topics
String mqtt_topic = "ledbart/text";
String mqtt_raw_topic = "ledbart/raw";

// Adjust display timeout (milliseconds)
if (hasMqttText && (millis() - lastMqttReceived > 30000)) {
```

## Legacy Implementations

Previous versions of this project used different architectures:

- **D1 Mini (ESP8266)** - Original WiFi controller
- **Arduino Uno** - LED bar driver
- **Two-chip design** - ESP8266 via serial to Arduino

See **[legacy/README.md](legacy/README.md)** for documentation on old implementations.

**Why the new approach is better:**
- ✅ Single chip instead of two
- ✅ No serial communication issues
- ✅ More features (web config, MQTT, mDNS)
- ✅ Easier to build and debug
- ✅ Smaller physical footprint

## Performance

- **Frame rate**: ~100 Hz (10ms per frame)
- **Refresh per row**: ~1500μs
- **Web requests**: Non-blocking, handled between frames
- **MQTT**: Asynchronous, no lag
- **Memory**: ~40KB RAM used, ~790KB flash

Display is always smooth and responsive.

## Credits

- **Original Author**: M. Wyns (2017)
- **Updated**: 2025
- **Display Driver**: Based on proven Arduino code
- **Web Interface**: ESP32 WebServer library
- **MQTT**: PubSubClient library
- **Time**: Time library by Paul Stoffregen

## License

MIT License - See LICENSE file for details

## Support

- **Issues**: [GitHub Issues](https://github.com/yourusername/LED-BARt/issues)
- **Documentation**: See `WIRING.md` for detailed setup
- **Legacy versions**: See `legacy/README.md`

## Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

---

**Built with ❤️ and ESP32-C3**

*For the original Arduino/ESP8266 version, see [legacy/README.md](legacy/README.md)*
