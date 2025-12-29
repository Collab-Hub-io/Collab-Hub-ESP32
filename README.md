# Collab-Hub ESP32 Client

This ESP32 sketch connects to a Collab-Hub server over WebSockets (Socket.IO framing), identifies with a username starting `ESP-XYZ`, joins the `iot` room, publishes periodic `control` messages, and listens to `control/event/chat`.

## Quick Install

### List Serial Ports (macOS/Linux)

To find which serial port your ESP32 is connected to, use:

```bash
# PlatformIO CLI
python3 -m platformio device list

# macOS Terminal
ls /dev/cu.*

# Linux Terminal
ls /dev/ttyUSB*
ls /dev/ttyACM*
```

Typical ESP32 ports:

- `/dev/cu.SLAB_USBtoUART` (CP210x)
- `/dev/cu.wchusbserial*` (CH34x)
- `/dev/ttyUSB0` or `/dev/ttyACM0` (Linux)

Use the detected port for upload and monitoring.

### PlatformIO (Recommended)

1. Install PlatformIO CLI (`python3 -m pip install --user platformio`) or VS Code PlatformIO IDE extension.
2. Clone this repo and open the folder in VS Code or terminal.
3. Edit `arduino/CollabHubESP32/config.h` with your WiFi and hub settings.
4. Connect your ESP32 board via USB. Install drivers if needed (see below).
5. Build & upload:

- CLI: `python3 -m platformio run -t upload --upload-port /dev/cu.SLAB_USBtoUART`
- VS Code: Use PlatformIO IDE buttons (Build, Upload, Monitor).

6. Open Serial Monitor at 115200 baud to view output.

### Arduino IDE

1. Install ESP32 core: Tools → Board → Boards Manager → search "esp32" by Espressif Systems → Install.
2. Open the sketch folder: `arduino/CollabHubESP32`.
3. Install ArduinoJson library (v6) via Tools → Manage Libraries…
4. Edit `config.h` for WiFi and hub settings.
5. Select your board and correct `/dev/cu.*` port.
6. Upload and open Serial Monitor at 115200 baud.

### USB Drivers (macOS)

- CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- CH34x: https://www.wch.cn/downloads/CH34XSER_MAC_ZIP.html

### Troubleshooting

- No serial ports: Install USB driver, allow extension, reboot, replug board.
- Upload stalls: Hold BOOT, tap EN (RST), release BOOT during connection.
- Wrong port: Set `--upload-port` or select correct port in IDE.
- Hub not receiving: Check WiFi, IP, and port settings.

See below for more details and advanced configuration.

## Requirements

- ESP32 Dev Module (USB serial)
- macOS drivers (if needed):
  - CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
  - CH34x: https://www.wch.cn/downloads/CH34XSER_MAC_ZIP.html
- PlatformIO CLI or VS Code PlatformIO IDE extension

## Configure

Edit `arduino/CollabHubESP32/config.h`:

- `WIFI_SSID` / `WIFI_PASS`
- `HUB_HOST` / `HUB_PORT` (use your hub IP; non-TLS `ws://`)
- `HUB_NAMESPACE` (typically `/hub`)
- `IOT_ROOM` (default `iot`)

To change the event trigger pin, edit `EVENT_PIN` in `CollabHubESP32.ino`:

```cpp
#define EVENT_PIN 0 // Change to desired GPIO pin
```

**Wiring:**
- Connect one side of a pushbutton to the selected GPIO pin (e.g., GPIO 0).
- Connect the other side of the button to GND.
- The pin is set as INPUT_PULLUP, so pressing the button pulls it LOW and triggers the event.

## Build & Upload (CLI)

```bash
python3 -m pip install --user platformio
python3 -m platformio --version
python3 -m platformio device list
# Replace the upload port below with your actual /dev/cu.* port
python3 -m platformio run -t upload --upload-port /dev/cu.SLAB_USBtoUART
python3 -m platformio device monitor -b 115200
```

Notes:

- Determine your upload port via `platformio device list`.
  Common names: `/dev/cu.usbserial-...`, `/dev/cu.SLAB_USBtoUART`, `/dev/cu.wchusbserial...`.
- If upload stalls, hold BOOT, tap EN (RST), release BOOT during connection.

## Build & Upload (VS Code)

- Install "PlatformIO IDE" extension.
- Open `Collab-Hub-ESP32` folder.
- Select env `esp32dev` and use "Build" → "Upload" → "Monitor".
- Optionally set `upload_port` and `monitor_port` in [Collab-Hub-ESP32/platformio.ini](Collab-Hub-ESP32/platformio.ini).

## Runtime

On serial monitor you should see:

- WiFi connected (IP address)
- Namespace open, username sent (e.g., `ESP-ABC`)
- Joined room `iot`
- Periodic `control` messages with `sensor` readings
- Event messages sent when a button is pressed on the configured GPIO pin

Server interactions:

- Send `control` with `header: "led"` and `values: 0|1` to toggle the onboard LED.
- Client listens to `control`, `event`, and `chat` frames.
- Pressing a button connected to the configured GPIO pin (default: GPIO 0) sends an event:
  - `{ "header": "ESP-Event1", "target": "all", "mode": "push" }`

## Transport

- Current transport uses non-TLS `ws://` for reliability.
- If your hub requires `wss://`, we can add a secure variant using `WiFiClientSecure` and a CA certificate.

## Files

- Sketch: `arduino/CollabHubESP32/CollabHubESP32.ino`
- Socket.IO shim: `arduino/CollabHubESP32/SioClient.h/.cpp`
- Minimal WebSocket: `arduino/CollabHubESP32/WsClient.h/.cpp`
- Config: `arduino/CollabHubESP32/config.h`
- Build config: `platformio.ini`

## Troubleshooting

- No serial ports listed: install USB driver and replug the board.
- Upload uses wrong port (Bluetooth): set `--upload-port` explicitly.
- Hub not receiving messages: verify `HUB_HOST` is reachable from ESP32 WiFi network and `HUB_PORT` matches.
- TLS errors: use non-TLS `ws://` or provide CA certs for `wss://` support.

## Arduino IDE Setup

- Install ESP32 core: Tools → Board → Boards Manager → search "esp32" by Espressif Systems → Install.
- Open the sketch folder: `arduino/CollabHubESP32` (file name matches folder).
- Install dependencies via Tools → Manage Libraries…:
  - ArduinoJson by Benoit Blanchon (v6).
- Select your board (ESP32 Dev Module) and the correct `/dev/cu.*` port.
- Edit `arduino/CollabHubESP32/config.h` with your WiFi and hub settings.
- Upload, then open Serial Monitor at 115200.

Notes:

- No extra Socket.IO libraries are required; the sketch includes a minimal WebSocket client (`WsClient`) and a small Socket.IO framing shim (`SioClient`).
- If your Mac shows only Bluetooth ports, install CP210x or CH34x driver, allow the system extension, reboot, and replug the board.
