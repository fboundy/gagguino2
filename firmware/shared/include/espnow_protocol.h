#pragma once

#include <stdint.h>

// Common ESP-NOW protocol constants shared by the display and controller.

// Handshake request emitted by the display. When the controller receives the
// request it should switch to the supplied Wi-Fi channel (second byte) and reply
// with ESPNOW_HANDSHAKE_ACK.
#define ESPNOW_HANDSHAKE_REQ 0xAA

// Handshake acknowledgement sent by the controller back to the display. The
// second byte contains the controller's view of the active channel so the
// display can detect mismatches and re-negotiate.
#define ESPNOW_HANDSHAKE_ACK 0x55

// Sent by the display after successfully processing a sensor packet.
#define ESPNOW_SENSOR_ACK 0x5A

// Identifier for control payloads pushed from the display to the controller.
#define ESPNOW_CONTROL_PACKET 0xC0

// Bit flags embedded in EspNowControlPacket::flags.
#define ESPNOW_CONTROL_FLAG_HEATER 0x01
#define ESPNOW_CONTROL_FLAG_STEAM 0x02
#define ESPNOW_CONTROL_FLAG_PUMP_PRESSURE 0x04

// Pump operating modes understood by the controller. The display always sends
// one of these values in EspNowControlPacket::pumpMode.
typedef enum
{
    ESPNOW_PUMP_MODE_NORMAL = 0,
    ESPNOW_PUMP_MODE_PREINFUSE = 1,
    ESPNOW_PUMP_MODE_MANUAL = 2,
} EspNowPumpMode;

// Packet describing brew/steam state for ESP-NOW transport. This struct must
// remain byte-for-byte compatible with the legacy implementation so that both
// ends can cast the payload directly.
typedef struct __attribute__((packed)) EspNowPacket
{
    uint8_t shotFlag;          //!< 1 if a shot is in progress
    uint8_t steamFlag;         //!< 1 if the machine is in steam mode
    uint8_t heaterSwitch;      //!< Heater switch state (1=on)
    uint32_t shotTimeMs;       //!< Shot duration in milliseconds
    float shotVolumeMl;        //!< Volume pulled in milliliters
    float setTempC;            //!< Currently configured temperature setpoint
    float currentTempC;        //!< Current sensed temperature in °C
    float pressureBar;         //!< Brew pressure in bar
    float steamSetpointC;      //!< Steam temperature setpoint in °C
    float brewSetpointC;       //!< Brew temperature setpoint in °C
    float pressureSetpointBar; //!< Target brew pressure in bar
    uint8_t pumpPressureMode;  //!< 1 if pressure limiting mode is active
    uint8_t reserved[3];       //!< Reserved for future use / alignment
    float pidPTerm;            //!< Proportional contribution of the temperature PID
    float pidITerm;            //!< Integral contribution of the temperature PID
    float pidDTerm;            //!< Derivative contribution of the temperature PID
} EspNowPacket;

// Control payload mirrored between Home Assistant, the display and the
// controller. The first byte must always be ESPNOW_CONTROL_PACKET.
typedef struct __attribute__((packed)) EspNowControlPacket
{
    uint8_t type;      //!< Constant ESPNOW_CONTROL_PACKET
    uint8_t flags;     //!< Bitmask of ESPNOW_CONTROL_FLAG_*
    uint8_t pumpMode;  //!< EspNowPumpMode value
    uint8_t reserved;  //!< Reserved for future use / alignment
    uint32_t revision; //!< Monotonic revision to detect stale commands
    float brewSetpointC;
    float steamSetpointC;
    float pidP;
    float pidI;
    float pidD;
    float pidGuard;
    float dTau;
    float pumpPowerPercent;
    float pressureSetpointBar;
} EspNowControlPacket;
