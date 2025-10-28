#pragma once

// Shared constants used by both the controller and display firmware. Keeping the
// values centralized in this header avoids drift between the two codebases.

// Temperature setpoint limits (degrees Celsius)
#define GAG_BREW_SETPOINT_MIN_C 87.0f
#define GAG_BREW_SETPOINT_MAX_C 97.0f
#define GAG_STEAM_SETPOINT_MIN_C 145.0f
#define GAG_STEAM_SETPOINT_MAX_C 155.0f

// Default setpoints (degrees Celsius)
#define GAG_BREW_SETPOINT_DEFAULT_C 92.0f
#define GAG_STEAM_SETPOINT_DEFAULT_C 152.0f

// Default PID tuning parameters for brew temperature control
#define GAG_PID_P_DEFAULT 8.0f
#define GAG_PID_I_DEFAULT 0.40f
#define GAG_PID_D_DEFAULT 17.0f
#define GAG_PID_GUARD_DEFAULT 25.0f
#define GAG_PID_DTAU_DEFAULT 0.8f

// Default pump control parameters
#define GAG_PUMP_POWER_DEFAULT_PERCENT 95.0f
#define GAG_PRESSURE_SETPOINT_DEFAULT_BAR 9.0f
#define GAG_PRESSURE_SETPOINT_MIN_BAR 0.0f
#define GAG_PRESSURE_SETPOINT_MAX_BAR 12.0f

// ESP-NOW watchdog timing (milliseconds)
#define GAG_ESPNOW_LINK_TIMEOUT_MS 5000UL

