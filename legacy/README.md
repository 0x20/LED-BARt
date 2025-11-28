# LED-BARt Legacy Implementations

This directory contains previous versions of the LED-BARt project that used different hardware architectures.

## ⚠️ Notice

These implementations are **deprecated** and kept for historical reference only.

**For current projects, use the [main ESP32-C3 direct driver](../README.md)** which is:
- Simpler (one chip instead of two)
- More reliable (no serial communication issues)
- More featured (web config, MQTT, mDNS)
- Easier to build and debug

## Legacy Architectures

### 1. D1 Mini + Arduino Uno (Two-Chip Design)

**Files:**
- `main_xiao_esp32c3.cpp` - ESP32-C3 version that talks to Arduino
- `arduino_uno_project/` - Arduino Uno LED driver
- `WIRING_XIAO_ESP32C3.md` - Wiring guide for two-chip setup

**How it worked:**
1. ESP32-C3 (or D1 Mini) handles WiFi/MQTT
2. Sends text via Serial (115200 baud) to Arduino Uno
3. Arduino Uno drives the 5V LED bar hardware

**Problems:**
- Serial communication unreliable (boot messages, timing issues)
- Two microcontrollers = more complexity
- Had to disconnect TX/RX to flash Arduino
- More power consumption
- Larger physical footprint

### 2. D1 Mini Standalone (ESP8266)

**Files:**
- `main.cpp` - Original D1 Mini standalone version
- `main_mqtt.cpp` - D1 Mini with MQTT support
- `main_webserver.cpp` - D1 Mini with web interface
- `WIRING.md` - Original D1 Mini wiring

**How it worked:**
- D1 Mini (ESP8266) directly drove LED bar
- 3.3V logic output
- Basic WiFi/MQTT support

**Problems:**
- 3.3V outputs may not reliably drive 5V LED logic
- ESP8266 less powerful than ESP32-C3
- No web configuration interface
- Settings hardcoded in source

### 3. Old Scripts

**Files:**
- `scripts/led_bart_arduino_uno/` - Original Arduino Uno standalone
- `scripts/led_bart_esp8266/` - Old ESP8266 scripts
- `scripts/led_bart_arduino_uno_receiver/` - Arduino receiver sketch

These were development/testing versions before the PlatformIO structure was created.

## When to Use Legacy Versions

**Short answer: Don't.**

**Long answer:** Only if you:
- Already built one of these and it's working
- Have specific hardware constraints
- Are maintaining an existing installation
- Want to understand the project's evolution

For new projects, **always use the current ESP32-C3 direct driver**.

## Migration Guide

### From Two-Chip to Direct Driver

**What you need:**
- Level shifters (TXS0108E × 2, or TXB0109 × 1, or 74HCT245)
- Same ESP32-C3 you already have
- Can retire the Arduino Uno

**Steps:**
1. Build level shifter circuit (see [../WIRING.md](../WIRING.md))
2. Connect ESP32-C3 GPIO (D2-D10) → level shifters → LED bar
3. Flash new firmware: `platformio run -e xiao_esp32c3_direct --target upload`
4. Configure WiFi via web interface
5. Done!

**What you gain:**
- ✅ Web-based WiFi configuration (no more hardcoded SSIDs)
- ✅ Web-based MQTT configuration
- ✅ mDNS support (`http://ledbart.local`)
- ✅ Home Assistant autodiscovery
- ✅ Raw bitmap mode for graphics
- ✅ Persistent configuration storage
- ✅ Better debugging (web interface shows status)
- ✅ One less microcontroller to manage

### From ESP8266 to ESP32-C3

**Why upgrade:**
- ESP32-C3 has more RAM (400KB vs 80KB)
- Faster processor (160MHz RISC-V)
- Better WiFi stack
- mDNS support
- Preferences API for persistent storage
- More GPIO pins available

**Migration:**
Same as above - add level shifters and use direct driver firmware.

## Building Legacy Versions

**D1 Mini with Arduino:**
```bash
# Flash ESP32-C3/D1 Mini
platformio run -e xiao_esp32c3 --target upload

# Flash Arduino Uno (disconnect TX/RX first!)
platformio run -d arduino_uno_project --target upload
```

**D1 Mini standalone:**
```bash
platformio run -e d1_mini_web --target upload
```

## Legacy Documentation

- **[WIRING.md](WIRING.md)** - Original D1 Mini wiring guide
- **[WIRING_XIAO_ESP32C3.md](WIRING_XIAO_ESP32C3.md)** - ESP32-to-Arduino wiring
- **[DEBUGGING.md](DEBUGGING.md)** - Debugging serial communication
- **[Pinout-UNOrev3_latest.png](Pinout-UNOrev3_latest.png)** - Arduino Uno pinout

## Serial Protocol (Two-Chip Version)

**ESP32/D1 Mini → Arduino:**
```
TEXT:your message here\n
```

**Arduino → ESP32/D1 Mini:**
```
ACK:your message here\n
```

The Arduino echoes back what it received as an ACK.

## Pin Mappings

### Arduino Uno (Legacy)
- Pin 2 - Shift register clock
- Pin 3 - Shift register data
- Pins 4-10 - Row drivers (7 rows)

### D1 Mini (Legacy Standalone)
- D5 (GPIO14) - Shift register clock
- D6 (GPIO12) - Shift register data
- D0, D1, D2, D3, D4, D7, D8 - Row drivers

### ESP32-C3 with Arduino (Legacy Two-Chip)
- D6 (GPIO6/TX) → Arduino RX (Pin 0)
- D7 (GPIO7/RX) → Arduino TX (Pin 1)
- GND → GND
- 5V → 5V (powers Arduino, Arduino powers LED bar)

## Troubleshooting Legacy Issues

### Serial Communication Problems

**Arduino shows ESP32 boot messages:**
- Normal - ESP32 ROM bootloader outputs on all UARTs
- Wait 2-3 seconds for main program to start
- If stuck, check Serial1 pin mapping in ESP32 code

**No ACK received:**
- Check TX/RX connections (D6→RX, D7←TX)
- Verify both use 115200 baud
- Add debug output to both sides
- Try loopback test (connect TX to RX on ESP32)

**Text backwards/upside down:**
- Check row pin order (should be pinRij[6-rij] in display code)
- Verify column order (normal, not reversed)
- Check character iteration (forward, not reversed)

### Why We Abandoned Serial Communication

Serial between two microcontrollers turned out to be unreliable:
1. **Boot messages** - ESP32 ROM sends garbage before main code runs
2. **Timing issues** - Had to wait for ESP32 to fully boot
3. **Flash conflicts** - Can't flash Arduino while TX/RX connected
4. **Debugging complexity** - Two serial monitors to watch
5. **Power sequencing** - Which boots first matters
6. **No error recovery** - If Arduino misses a byte, display is wrong

The direct approach solves all of these.

## Historical Context

**2017** - Original project by M. Wyns
- Arduino Uno standalone driving LED bar
- Simple, worked reliably

**~2020** - WiFi added with D1 Mini
- D1 Mini (ESP8266) standalone attempt
- 3.3V logic issues

**~2022** - Two-chip architecture
- D1 Mini for WiFi, Arduino for LED driving
- Worked but had serial communication headaches

**2025** - ESP32-C3 direct driver
- Modern ESP32-C3 with enough power to do both jobs
- Level shifters solve voltage issue
- Web configuration, MQTT, Home Assistant support
- **Current recommended approach**

## Support for Legacy Versions

**Q: Can you help me debug my two-chip setup?**
A: I recommend migrating to the current direct driver instead. It will save you time in the long run.

**Q: My D1 Mini isn't working with the LED bar**
A: The 3.3V outputs may not reliably drive 5V logic. Use ESP32-C3 with level shifters instead.

**Q: I already built the two-chip version and it works**
A: Great! If it's working for you, no need to change. But the direct driver is better for new builds.

**Q: Where can I get help?**
A: For legacy versions, check the old documentation files in this directory. For current version, see [main README](../README.md).

## License

Same as main project - MIT License

---

**For current documentation, see [../README.md](../README.md)**
