# Temperature sensor firmware

The current ESPHome firmware is in [esphome/](esphome/README.md), including
all five device configurations, custom components, local web UI and fault tests.
Fleet build/update automation and history migration remain separate.

The Arduino files below are preserved as legacy firmware and recovery source.
The legacy `compile.sh` builds those sketches, not the ESPHome firmware.

---

# Arduino Temp Sensor

This project is a temperature, humidity, and pressure monitoring sensor utilizing an ESP8266 (NodeMCU) and a BME280 sensor.

## Project Environment & Dependencies

For future AI agents or developers working on this project, ensure the following core and libraries are installed:

### 1. Arduino Core
- **Platform:** `esp8266:esp8266` (Version `3.1.2`)

### 2. Required Libraries
- **WiFiManager** (Version `2.0.17`)
- **Adafruit BME280 Library** (Version `2.3.0` / configured for `2.2.4`)
- **Adafruit Unified Sensor** (Version `1.1.15` / configured for `1.1.14`)
- **Adafruit BusIO** (Version `1.17.4`)

---

## Compilation Settings & Parameters

When compiling or uploading, configure the board with the following parameters:

- **Board:** NodeMCU 1.0 (ESP-12E Module)
- **FQBN:** `esp8266:esp8266:nodemcuv2`
- **Flash Size:** `4MB (FS:1MB / OTA:~1019KB)` -> `eesz=4M1M` (essential for LittleFS storage of configuration parameters)
- **Upload Speed / Serial Speed:** `115200` baud

### Compilation Script (`compile.sh`)
To compile this project and export the compiled binary files to the `bin/` directory in the workspace, run the helper script:
```bash
./compile.sh
```
Or run `arduino-cli` directly:
```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2:eesz=4M1M,baud=115200 --output-dir bin .
```
