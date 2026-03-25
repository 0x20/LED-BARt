# LED BARt - ESP32-C3 Web Server

Accepts text commands over HTTP on the LAN and forwards them to the Arduino Uno via UART.

## Wiring

| ESP32-C3 | Arduino Uno |
|----------|-------------|
| D6 (TX)  | pin 0 (RX)  |
| GND      | GND         |

> Disconnect the wire from Uno pin 0 before uploading a sketch, reconnect after.

## Usage

```bash
 curl -X POST http://10.51.1.141/text -H "Content-Type: text/plain" -d "SAMPLE TEXT"
```

The IP is printed to Serial on boot (115200 baud).
Max 19 characters — longer text is truncated, shorter is padded with spaces.

## Endpoint

| Method | Path    | Body          | Response |
|--------|---------|---------------|----------|
| POST   | `/text` | plain text    | `ok`     |
