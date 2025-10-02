Gagguino ESP – ESP32 Firmware for Gaggia Classic
================================================

An ESP32-based controller for the Gaggia Classic espresso machine featuring PID temperature control (MAX31865 + PT100), flow/pressure/shot timing, and a robust ESP-NOW link to the companion display (which exposes MQTT/Home Assistant integration) alongside OTA updates.

Features
--------
- PID temperature control using MAX31865 (PT100) with anti-windup and derivative on measurement.
- Heater control via time-proportioning PWM windowing.
- Flow pulses → volume, pressure sampling with moving average, and shot timing.
- Wi‑Fi + ESP-NOW telemetry/control link to the display (display handles MQTT/Home Assistant discovery).
- ArduinoOTA with safety handling (heater disabled during OTA).

Hardware / Pinout (ESP32 dev board defaults)
-------------------------------------------
- `FLOW_PIN` 26: Flow sensor input (interrupt on CHANGE)
- `ZC_PIN` 25: AC zero‑cross detect (interrupt on RISING)
- `HEAT_PIN` 27: Boiler relay/SSR output (time‑proportioning)
- `PUMP_PIN` 17: Pump power control via RBDDimmer triac (0–100%)
- `AC_SENS` 14: Steam switch sense (digital input)
- `MAX_CS` 16: MAX31865 SPI chip‑select
- `PRESS_PIN` 35: Analog pressure sensor input

Getting Started
---------------
1) Prerequisites
- PlatformIO (VS Code extension or CLI)
- Libraries are managed by PlatformIO via `platformio.ini` (e.g. `Adafruit MAX31865`).

2) Configure secrets
- Edit `src/secrets.h` and set:
  - `WIFI_SSID`, `WIFI_PASSWORD`
  - (Optional, consumed by the display firmware) `MQTT_HOST`, `MQTT_PORT`, `MQTT_CLIENTID`, `MQTT_USER`, `MQTT_PASS`
- Tip: Avoid committing real credentials. Consider ignoring or templating this file in your fork.

3) Build and upload (USB)
- Set board/environment in `platformio.ini` (default `esp32dev`).
- Build: `pio run`
- Upload (USB): `pio run -t upload`
- Monitor: `pio device monitor -b 115200`

4) OTA uploads (Wi‑Fi)
- `platformio.ini` includes an `esp32dev_ota` env with `upload_protocol = espota`.
- Set your device IP in `upload_port` (e.g. `192.168.4.99`).
- Upload over Wi‑Fi: `pio run -e esp32dev_ota -t upload`
- Optional OTA password:
  - In `platformio.ini` add build flag: `-D OTA_PASSWORD="your-password"` (or `OTA_PASSWORD_HASH`)
  - Match the `--auth` flag under `upload_flags` for `espota`.
- Enable OTA via the display before uploading (opens a ~5 min window and pauses normal telemetry).

Tuning & Behavior
-----------------
- Brew setpoint limits: 90–99 °C. Steam setpoint limits: 145–155 °C (default 152 °C).
- PID defaults (overridable via display/ESP-NOW controls): `P=20.0`, `I=1.0`, `D=100.0`, `Guard=20.0`.
- Pressure: analog read with linear conversion; intercept is auto‑zeroed at boot if near 0 bar.
- Heater: time‑proportioning window (`PWM_CYCLE`) with dynamic ON time from PID result.

Troubleshooting
---------------
- Serial monitor at `115200` shows boot logs, Wi‑Fi status, and optional periodic diagnostics.
- MAX31865 diagnostics: firmware logs faults and raw/temperature reads to help validate wiring.
- OTA: Device hostname is derived from MAC (e.g. `gaggia-ABCDEF`). During OTA the heater is forced off and other work is throttled.

Safety
------
- Mains voltage is dangerous. Ensure proper isolation, fusing, and enclosure.
- Verify all pin mappings, voltages, and grounds before powering the machine.

Project Layout
--------------
- `src/gagguino.cpp` – main firmware logic, ESP-NOW, OTA, PID, sensors.
- `src/gagguino.h` – public entry points for `setup()`/`loop()` in the `gag` namespace.
- `src/main.cpp` – minimal sketch bridging Arduino to `gag::setup/loop`.
- `src/secrets.h` – Wi‑Fi (and shared MQTT credentials for the display).
- `platformio.ini` – environments, dependencies, and OTA settings.

License
-------
See `LICENSE`.

