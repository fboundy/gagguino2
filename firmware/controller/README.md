Gagguino ESP – ESP32 Firmware for Gaggia Classic
================================================

This firmware drives a Gaggia Classic espresso machine using an ESP32.
It handles PID temperature control, heater PWM, flow and pressure
monitoring and communicates with a remote display via ESP‑NOW on
channel 6.

Features
--------
- PID temperature control using MAX31865 (PT100).
- Heater control via time‑proportioning PWM windowing.
- Flow pulses → volume, pressure sampling with moving average, and shot timing.
- ESP‑NOW telemetry broadcast (channel 6).
- ArduinoOTA with safety handling (heater disabled during OTA).

Configuration
-------------
Edit `secrets.h` and set:
- `WIFI_SSID`, `WIFI_PASSWORD` (optional, only for OTA)
- `CONTROLLER_MAC`, `DISPLAY_MAC` (peer addresses)
- `ESPNOW_CHANNEL` (default 6)

Build and upload with PlatformIO:
```
pio run
pio run -t upload
```

License
-------
See `LICENSE`.
