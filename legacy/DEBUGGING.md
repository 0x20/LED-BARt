# LED-BARt Debugging Guide

## Current Setup

**D1 Mini:** Web server version with Access Point mode
**Arduino Uno:** LED bar controller with startup message

## How to Debug

### Step 1: Check D1 Mini WiFi

The D1 Mini creates its own WiFi network if it can't connect to your WiFi:

**Access Point Details:**
- SSID: `LED-BARt`
- Password: `ledbart123`
- Default IP: `192.168.4.1`

**To connect:**
1. Look for WiFi network "LED-BARt" on your phone/laptop
2. Connect using password: `ledbart123`
3. Open browser to: `http://192.168.4.1`

### Step 2: Check Serial Output

To see what the D1 Mini is doing:

```bash
# Using screen (press Ctrl+A then K to exit)
screen /dev/cu.usbserial-A50285BI 115200

# Or using cu
cu -l /dev/cu.usbserial-A50285BI -s 115200
```

**What to look for:**
```
=================================
LED-BARt Web Server Starting...
=================================
Connecting to WiFi: YOUR_WIFI_SSID
..................
WiFi connection failed! Starting Access Point...
Access Point started!
SSID: LED-BARt
Password: ledbart123
IP address: 192.168.4.1
Web server started!
Access at: http://192.168.4.1
=================================
Setup complete!
=================================

TEXT:LED-BARt Ready!
```

### Step 3: Check Arduino Display

**On power-up, the Arduino should show:**
1. "Arduino UNO OK!" for 3 seconds
2. Then "Waiting for D1..." until D1 Mini sends data

**If you don't see anything on the LED bar:**
- Check all 9 wires from Arduino to LED bar
- Verify LED bar has power
- Check pins 2-8, 12, 13 on Arduino

### Step 4: Test Web Interface

Once connected to `LED-BARt` WiFi, go to `http://192.168.4.1`:

**You should see:**
- Current display text
- Input box to set custom text
- System status (uptime, serial messages sent, etc.)
- Quick test buttons

**Try setting text:**
1. Type "Hello World!" in the input box
2. Click "Update Display"
3. Check if "TEXT:Hello World!" appears in serial monitor
4. LED bar should update

### Step 5: Check Serial Communication

**D1 Mini sends text in this format:**
```
TEXT:your message here\n
```

**Arduino expects:**
- 115200 baud
- Format: `TEXT:` followed by text and newline

**Common issues:**
- TX/RX not connected (D1 TX → Arduino RX pin 0)
- Wrong baud rate
- TX wire disconnected during flash (reconnect it!)

### Step 6: Verify Connections

**Between D1 Mini and Arduino:**
```
D1 Mini TX (GPIO1) → Arduino RX (Pin 0)
D1 Mini GND        → Arduino GND
D1 Mini 5V         → Arduino 5V
```

**From Arduino to LED Bar:**
```
Arduino Pin 13 → Shift Register Clock
Arduino Pin 12 → Shift Register Data
Arduino Pin 2  → Row 1
Arduino Pin 3  → Row 2
Arduino Pin 4  → Row 3
Arduino Pin 5  → Row 4
Arduino Pin 6  → Row 5
Arduino Pin 7  → Row 6
Arduino Pin 8  → Row 7
```

## Quick Tests

### Test 1: Arduino Only
1. Disconnect D1 Mini TX wire
2. Power Arduino via USB
3. Should show "Arduino UNO OK!" for 3 seconds
4. Then "Waiting for D1..."

### Test 2: D1 Mini Serial Output
1. Connect to D1 Mini via serial monitor
2. Should see startup messages
3. Should see "TEXT:..." messages being sent
4. Count should increment in web interface

### Test 3: Full System
1. Connect everything
2. Power Arduino (which powers D1 Mini)
3. D1 Mini starts sending text
4. Arduino displays it
5. Web interface shows stats

## Web Interface Features

### Main Page
- **Current Display:** Shows what's currently on the LED bar
- **Set Display Text:** Type custom text to display
- **System Status:**
  - WiFi mode (AP or Station)
  - IP address
  - MQTT connected (if configured)
  - Serial messages sent
  - MQTT messages received
  - Web requests
  - Uptime
  - Free heap memory

### Status Endpoint
JSON API available at: `http://192.168.4.1/status`

Returns:
```json
{
  "display_text": "LED-BARt Ready!",
  "wifi_mode": "AP",
  "ip": "192.168.4.1",
  "mqtt_connected": false,
  "serial_sent": 123,
  "mqtt_received": 0,
  "web_requests": 5,
  "uptime": 456,
  "free_heap": 35000
}
```

## Troubleshooting

### "Site unreachable"
- Make sure you're connected to "LED-BARt" WiFi
- Try `192.168.4.1` instead of domain name
- Check D1 Mini has power (LED should be on)
- Wait 10 seconds after power-up for AP to start

### "No display on LED bar"
- Check Arduino shows "Arduino UNO OK!" on startup
- Verify all 9 wires from Arduino to LED bar
- Check LED bar has power
- Measure voltage on LED bar pins

### "D1 Mini not creating WiFi network"
- Check D1 Mini has power
- Look for blue LED blinking
- Try reflashing the D1 Mini
- Check serial output for errors

### "Serial messages sent = 0"
- D1 Mini might not be running
- Check power connections
- Reflash D1 Mini
- Monitor serial output

### "Arduino not receiving"
- Check TX wire is connected (D1 TX → Arduino RX)
- Verify GND connection
- Check baud rate is 115200 on both
- Try disconnecting/reconnecting TX wire

## Serial Monitor Commands

To monitor D1 Mini:
```bash
screen /dev/cu.usbserial-A50285BI 115200
# Press Ctrl+A then K to exit
```

To monitor Arduino (if D1 not connected):
```bash
screen /dev/cu.wchusbserial130 115200
# Press Ctrl+A then K to exit
```

## Re-flashing

**D1 Mini:**
```bash
# Disconnect TX wire first!
platformio run -e d1_mini_web --target upload
```

**Arduino:**
```bash
# Disconnect TX wire first!
platformio run -d arduino_uno_project --target upload
```

## Configuration

Edit WiFi settings in `src/main_webserver.cpp`:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

If WiFi fails, it automatically creates AP mode (LED-BARt network).
