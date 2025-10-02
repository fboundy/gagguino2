#pragma once
#include <Arduino.h>

/**
 * @file gagguino.h
 * @brief Public entry points for the Gagguino firmware.
 *
 * This header exposes a thin wrapper around Arduino's global
 * `setup()` and `loop()` so the main sketch stays minimal and
 * the firmware logic can live in `gagguino.cpp`.
 */

/**
 * @namespace gag
 * @brief Namespace containing the firmware entry points.
 */
namespace gag {

/**
 * @brief Initialize hardware, connectivity and discovery.
 *
 * Responsibilities:
 * - Configure pins and peripherals (MAX31865, ADC, etc.).
 * - Start Wi‑Fi and establish the ESP-NOW link to the display.
 * - Initialize Over‑The‑Air (OTA) update handling once Wi‑Fi is ready.
 * - Calibrate/zero pressure intercept on boot if near atmospheric.
 */
void setup();

/**
 * @brief Main control loop.
 *
 * Runs frequently to:
 * - Update PID and PWM based heater control.
 * - Track flow, pressure, shot timing and steam state.
 * - Maintain Wi‑Fi, ESP-NOW and OTA sessions.
 * - Exchange telemetry with the display over ESP-NOW.
 */
void loop();

}  // namespace gag
