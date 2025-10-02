Gagguino ESP – ESP32 Firmware for Gaggia Classic
================================================

An ESP32-based controller for the Gaggia Classic espresso machine featuring PID temperature control (MAX31865 + PT100), flow/pressure/shot timing, an ESP-NOW control link to the front-panel display, and robust OTA updates gated by the operator.

Features
--------
- PID temperature control using MAX31865 (PT100) with anti-windup and derivative-on-measurement.
- Heater control via time-proportioning PWM windowing.
- Flow pulses → volume, pressure sampling with moving average, and shot timing.
- ESP-NOW telemetry/control channel orchestrated by the display, including automatic channel renegotiation when Wi-Fi roams.
- Wi‑Fi used solely for OTA updates (opened on demand via the display/Home Assistant bridge).
- Safe fallback defaults (heater on, pump normal @ 95%, steam dictated by hardware switch) whenever ESP-NOW traffic is lost.

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
- Libraries are managed by PlatformIO via `platformio.ini` (`Adafruit MAX31865`, `RBDDimmer`).

2) Configure secrets
- Edit `src/secrets.h` and set:
  - `WIFI_SSID`, `WIFI_PASSWORD`
  - `GAGGIA_ID` (unique per machine for MQTT topics shared with the display)
  - MQTT credentials (`MQTT_HOST`, `MQTT_PORT`, `MQTT_USER`, etc.). These values are consumed by the display firmware for Home Assistant integration; the controller ignores them but the definitions must remain present when building the shared secrets header.
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
- OTA windows are disabled by default. Enable OTA via the display UI or the mirrored Home Assistant switch to open a ~5 minute window before uploading.

Display / Home Assistant Bridge
-------------------------------
- The display module owns the MQTT connection to Home Assistant. It mirrors HA entities to its UI controls and forwards the resulting control packets to the controller over ESP-NOW.
- The display discovers the current Wi-Fi channel for its STA interface, advertises that channel during the ESP-NOW handshake, and resends handshakes until the controller acknowledges. This keeps the radios aligned even when the display roams between APs.
- Control packets carry heater/steam toggles, brew & steam setpoints, PID parameters, pump mode, and pump power. Each packet includes a revision counter so stale commands are ignored.
- Sensor packets flow from the controller → display every 250 ms (or faster if required by HA limits). Each packet is acknowledged by the display; missing acknowledgements force the controller back to its safe defaults.

Telemetry & Safety
------------------
- Brew setpoint limits: 90–99 °C. Steam setpoint limits: 145–155 °C (default 152 °C).
- PID defaults (overridable via ESP-NOW control packets): `P=15.0`, `I=0.35`, `D=60.0`, Guard=`10.0`, derivative filter τ=`0.8 s`.
- Pressure: analog read with linear conversion; intercept is auto‑zeroed at boot if near 0 bar.
- Heater: time‑proportioning window (`PWM_CYCLE`) with dynamic ON time from PID result.
- Loss of ESP-NOW communication forces: heater ON, pump mode NORMAL, pump power 95%, steam governed by hardware switch. OTA access is also revoked when the link drops.

Troubleshooting
---------------
- Serial monitor at `115200` shows boot logs, Wi‑Fi status, ESP-NOW channel sync, and optional periodic diagnostics.
- MAX31865 diagnostics: firmware logs faults and raw/temperature reads to help validate wiring.
- OTA: Device hostname is derived from MAC (e.g. `gaggia-ABCDEF`). During OTA the heater is forced off and other work is throttled.

Project Layout
--------------
- `src/gagguino.cpp` – main firmware logic, OTA gating, ESP-NOW handling, PID, sensors.
- `src/gagguino.h` – public entry points for `setup()`/`loop()` in the `gag` namespace.
- `src/main.cpp` – minimal sketch bridging Arduino to `gag::setup/loop`.
- `platformio.ini` – environments, dependencies, and OTA settings.

License
-------
See `LICENSE`.
