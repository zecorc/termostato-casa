# termostato-casa

Open-source heating thermostat: ESP32 hub + WiFi room sensors.

## Hardware (v1 dev setup)

Both hub and sensors use **ESP32-WROOM** on **HW-394** devkit for now.

### Hub — display GMT028-05 (2.8" SPI 240×320, ILI9341, no touch)

| Display | ESP32 (HW-394) |
|---------|----------------|
| VCC | 3V3 |
| GND | GND |
| SDA | D23 (MOSI) |
| SCK | D18 |
| CS | D15 |
| DC | D21 |
| RST | D4 |

Relay (dry contact): **D26** (GPIO26).

Pin map and TFT_eSPI config: `firmware/hub/include/board_config.h`, `pins.h`.

### Sensor — DS18B20

| DS18B20 | ESP32 |
|---------|-------|
| VCC | 3V3 |
| GND | GND |
| DATA | D4 (+ 4.7 kΩ to 3V3) |

## Toolchain

- [PlatformIO](https://platformio.org/) 6.x
- ESP32 core (`espressif32`)

```bash
pip install -U platformio
```

## Build

```bash
pio run -d firmware/hub
pio run -d firmware/sensor
```

Or from the repository root: `pio run`

## Test senza hardware

Puoi simulare l'hub sul PC (stessa Web UI e API del firmware ESP32):

```bash
pip install -r tools/mock_hub/requirements.txt
python tools/mock_hub/mock_hub.py
```

Apri http://127.0.0.1:8080/ e, in un altro terminale:

```bash
python tools/mock_hub/simulate_sensor.py --temp 18.0
```

Dettagli ed esempi curl: [tools/mock_hub/README.md](tools/mock_hub/README.md).

## Layout

```
firmware/
├── hub/      ESP32 — display, relay, web server
├── sensor/   ESP32 — room temperature nodes
└── common/   Shared headers (API protocol)
tools/
└── mock_hub/ Mock hub HTTP for testing without ESP32
```
