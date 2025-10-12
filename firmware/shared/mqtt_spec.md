# MQTT Topic Specification

Both controller (`gagguino_esp`) and display (`Gaggia Display IDF`) share the same
MQTT topic hierarchy.  All topics are rooted at:

```
 gaggia_classic/<GAGGIA_ID>/
```

`<GAGGIA_ID>` is a 6‑character hexadecimal identifier defined in `secrets.h`.

## Common topics

| Topic suffix | Direction | Description |
|--------------|-----------|-------------|
| `heater/state` | pub by controller, sub by display | Heater on/off state |
| `heater/set` | cmd to controller (display publishes) | Command to toggle heater |
| `steam/state` | pub by controller | Steam switch state |
| `steam/set` | cmd to controller (display publishes) | Command to toggle steam mode |
| `current_temp/state` | pub by controller | Boiler temperature |
| `set_temp/state` | pub by controller | Active temperature setpoint |
| `pressure/state` | pub by controller | Boiler pressure (bar) |
| `shot_volume/state` | pub by controller | Shot volume (mL) |
| `shot/state` | pub by controller | Shot active flag |
| `ota/enable` | reserved | Former OTA control (unused) |
| `ota/status` | reserved | Former OTA status (unused) |
| `espnow/channel` | pub/sub | ESP‑NOW channel coordination |
| `espnow/mac` | pub/sub | ESP‑NOW peer MAC |
| `espnow/cmd` | pub/sub | ESP‑NOW control commands |
| `brew_setpoint/set` & `.../state` | cmd/state | Brew temperature setpoint |
| `steam_setpoint/set` & `.../state` | cmd/state | Steam temperature setpoint |
| `pid_p`, `pid_i`, `pid_d`, `pid_guard`, `pid_d_tau` | cmd/state | PID tuning parameters (`pid_guard` range 0–100) |
| `pid_p_term/state`, `pid_i_term/state`, `pid_d_term/state` | pub by controller | Live PID contributions reported over ESP-NOW |
| `pressure_setpoint/set` & `.../state` | cmd/state | Brew pressure setpoint in bar |
| `pump_pressure_mode/set` & `.../state` | cmd/state | Enable pump pressure limiting mode |
| `status` | pub by controller & display | Availability ("online"/"offline") |
| `error` | pub by controller | Aggregated error log |

This document reflects the topic layout used by both projects so that the
components can interoperate via a shared MQTT broker.
