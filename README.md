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
- **Effects** — animated effects running on the ESP32 itself (scroll, blink, wave, rain, sparkle, Game of Life, inverted, pulse) with adjustable speed — effects persist even when the browser is closed
- **Pixel Editor** — click/drag to draw pixels directly on the display in real time

The connection indicator shows green when the WebSocket is connected for live streaming.

## Usage

Send text to the display via the web interface or directly:

```bash
curl -X POST http://ledbart.local/text -H "Content-Type: text/plain" -d "HELLO WORLD"
```

Reachable via mDNS at `ledbart.local`. Max 19 characters — longer text is truncated, shorter is padded with spaces.

## API

| Endpoint | Method | Body | Description |
|----------|--------|------|-------------|
| `/text` | POST | plain text | Send text to display (max 19 chars, 5x7 font) |
| `/pixels` | POST | 190 hex chars | Send raw pixel data (95 column bytes) |
| `/effect` | POST | JSON `{"effect":"scroll","text":"hello","speed":80}` | Start a server-side effect |
| `/effect` | GET | — | Get current effect status |
| `/effect` | DELETE | — | Stop effect, clear display |
| `/log` | GET | — | View activity log |
| `/help` | GET | — | API help text |
| WebSocket `:81` | binary | 95 bytes | Real-time pixel streaming |

Effects: `scroll`, `blink`, `wave`, `rain`, `sparkle`, `gol`, `inverted`, `pulse`

Effects run on the ESP32 and persist without a client connected. Any direct interaction (text, pixels, WebSocket) automatically stops a running effect.

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


## Scripts

| Folder | Chip | Role |
|--------|------|------|
| `scripts/led_bart_esp32c3_webserver/` | Xiao ESP32-C3 | HTTP → UART bridge + mDNS (`ledbart.local`) |
| `scripts/led_bart_arduino_uno_uart_display/` | Arduino Uno | UART → LED bar driver (active) |
| `scripts/gol_ledbar.py` | — | Game of Life via WebSocket (standalone Python) |
| `scripts/font_preview.py` | — | Preview 5x7 font glyphs in the terminal |
| `scripts/legacy/led_bart_arduino_uno_og/` | Arduino Uno | original reference code |
