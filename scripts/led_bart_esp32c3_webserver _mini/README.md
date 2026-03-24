# LED BARt - ESP32-C3 Web Server

Accepts text commands over HTTP on the LAN and logs them to Serial.
Next step: forward received text to the Arduino Uno via UART.

## Usage

```bash
curl -X POST http://<IP>/text -H "Content-Type: text/plain" -d "hello world"
```

The IP is printed to Serial on boot (115200 baud).

## Endpoint

| Method | Path    | Body          | Response |
|--------|---------|---------------|----------|
| POST   | `/text` | plain text    | `ok`     |
