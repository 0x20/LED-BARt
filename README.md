# LED BARt

WiFi-connected LED bar display for the hackerspace.

## Architecture

```
LAN
 │
 ▼
Xiao ESP32-C3          Arduino Uno
┌─────────────┐  UART  ┌──────────────┐
│  WiFi/HTTP  │──────▶ │  LED bar     │
│  webserver  │  9600  │  controller  │
│  3.3V logic │  baud  │  5V logic    │
└─────────────┘        └──────────────┘
```

**Why two chips?** The LED bar needs 5V logic — the Arduino Uno provides that. The ESP32-C3 handles WiFi/Bluetooth but runs at 3.3V, so it can't drive the bar directly. They communicate over UART.

## Web Interface

Live at https://0x20.github.io/LED-BARt/

![LED-BARt web interface](images/frontend.png)

The `website/` folder contains a web frontend with a pixel-accurate preview using the same 5x7 font as the hardware. It has three modes:

- **Text** — type a message (max 19 chars), preview it live, send via HTTP POST
- **Effects** — animated effects streamed over WebSocket (scroll, blink, wave, rain, sparkle, Game of Life, inverted, pulse) with adjustable speed
- **Pixel Editor** — click/drag to draw pixels directly on the display in real time

The connection indicator shows green when the WebSocket is connected for live streaming.

## Usage

Send text to the display via the web interface or directly:

```bash
curl -X POST http://ledbart.local/text -H "Content-Type: text/plain" -d "HELLO WORLD"
```

Send raw pixels (95 hex-encoded column bytes, each byte = 7 rows):

```bash
curl -X POST http://ledbart.local/pixels -H "Content-Type: text/plain" -d "7e111111117f494949..."
```

Real-time streaming via WebSocket on port 81 (95-byte binary frames).

Reachable via mDNS at `ledbart.local`. The IP is also printed to Serial (115200 baud) on boot.

Max 19 characters for text mode — longer text is truncated, shorter is padded with spaces. The scroll effect removes this limit.

## Flashing

### ESP32-C3 (webserver)

Board: **Seeed XIAO ESP32C3** (install [Seeed boards package](https://wiki.seeedstudio.com/xiao_esp32c3_getting_started/) in Arduino IDE)

Libraries:
- [WebSockets](https://github.com/Links2004/arduinoWebSockets) by Markus Sattler (v2.4.0+)

Built-in (included with ESP32 board package): `WiFi`, `WebServer`, `ESPmDNS`, `HardwareSerial`

### Arduino Uno (LED driver)

No external libraries needed — uses only `Arduino.h`.

> Disconnect the wire from Uno pin 0 (RX) before uploading, reconnect after.

## Wiring

| ESP32-C3 | Arduino Uno |
|----------|-------------|
| D6 (TX)  | pin 0 (RX)  |
| GND      | GND         |

> Disconnect the wire from Uno pin 0 before uploading a sketch, reconnect after.

## Scripts

| Folder | Chip | Role |
|--------|------|------|
| `scripts/led_bart_esp32c3_webserver/` | Xiao ESP32-C3 | HTTP → UART bridge + mDNS (`ledbart.local`) |
| `scripts/led_bart_arduino_uno_uart_display/` | Arduino Uno | UART → LED bar driver (active) |
| `scripts/gol_ledbar.py` | — | Game of Life via WebSocket (standalone Python) |
| `scripts/font_preview.py` | — | Preview 5x7 font glyphs in the terminal |
| `scripts/legacy/led_bart_arduino_uno_og/` | Arduino Uno | original reference code |
