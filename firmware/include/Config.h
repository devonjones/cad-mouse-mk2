#pragma once

#include <Arduino.h>

// Uncomment to build a debug firmware with USB CDC serial telemetry.
// Enabling this re-adds the CDC interface that HIDController::begin()
// strips, so the device no longer enumerates as the pure-HID shape a
// real SpaceMouse Compact presents. Leave undefined for normal use.
// #define ENABLE_DEBUG_SERIAL

namespace Config {

#ifdef ENABLE_DEBUG_SERIAL
const bool ENABLE_TELEMETRY = true;
#else
const bool ENABLE_TELEMETRY = false;
#endif

// Hardware pins (XIAO RP2040)
const int PIN_RIGHT_BTN = D0;
const int PIN_LEFT_BTN = D2;
const int PIN_LED_DATA = D3;
const int PIN_LED_LS = D1;
const int PIN_MAG1_LS = D10;
const int PIN_MAG2_LS = D9;
const int PIN_MAG3_LS = D8;

// Samples for calibration offset
const int ZERO_SAMPLES = 200;

// Per-unit calibration (DECOUPLE, TRIM, DEAD_AXIS, CURVE_EXPO, CROSS,
// LED default). Each physical unit has its own fitted header — select
// with -DUNIT=<n> (platformio.ini defines one env per unit).
#ifndef UNIT
#define UNIT 1
#endif
#if UNIT == 1
#include "units/unit1.h"
#elif UNIT == 2
#include "units/unit2.h"
#else
#error "Unknown UNIT - add firmware/include/units/unit<n>.h"
#endif

// Smoothing
const float SMOOTH_TAU_S = 0.09;

// Final axis output range
const float AXIS_LIMIT = 350.0;

// A real SpaceMouse streams reports while deflected; refresh unchanged
// non-zero axis reports at least this often so drivers don't see the
// device go quiet mid-motion.
const unsigned long HID_KEEPALIVE_MS = 50;

// RGB LEDs
const int LED_COUNT = 8;
const int LED_BRIGHTNESS = 40;
// Idle color palette. Cycle at runtime by holding ONLY the right button
// for 3s while idle; the selection persists in flash across power
// cycles. First entry is the factory default.
const unsigned long LED_IDLE_PALETTE[] = {
    0x9400D3,  // purple
    0x00FF00,  // green
    0x0080FF,  // sky blue
    0x00FFFF,  // cyan
    0xFF00FF,  // magenta
    0xFF4000,  // orange
    0xFFFFFF,  // white
};
const int LED_IDLE_PALETTE_COUNT =
    sizeof(LED_IDLE_PALETTE) / sizeof(LED_IDLE_PALETTE[0]);
static_assert(LED_DEFAULT_COLOR_INDEX >= 0 &&
                  LED_DEFAULT_COLOR_INDEX < LED_IDLE_PALETTE_COUNT,
              "unit header's LED_DEFAULT_COLOR_INDEX is outside the palette");
const unsigned long LED_CALIBRATING_COLOR = 0x0000FF;

// FSM timing
const long IDLE_SLEEP_TIMEOUT_MS = 2 * 60 * 1000;

}  // namespace Config
