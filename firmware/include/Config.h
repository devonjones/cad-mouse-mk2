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

// Motion calibration, fitted 2026-07-09 from guided single-axis serial
// telemetry of this specific unit (fit tooling: fit_matrix.py, ridge
// regression toward a diagonal prior, lambda=10).
//
// Pipeline per sample: v = SIGN_AXIS .* sensed, out = DECOUPLE @ v,
// then per-direction TRIM so full deflection lands at AXIS_LIMIT in
// both directions despite the asymmetric magnet/sensor response.
// DECOUPLE's off-diagonal terms cancel cross-axis bleed (untreated,
// rocking the cap produced an X-pan up to 4x the roll signal).
// Axis order: TX, TY, TZ, RX, RY, RZ.
const int SIGN_AXIS[6] = {+1, -1, +1, -1, -1, -1};
const float DECOUPLE[6][6] = {
    {116.6878, -19.5656, 6.0574, -2.0605, -31.5003, -8.7592},
    {-11.9184, 114.6113, 21.8577, 28.8205, 3.6550, 6.7970},
    {-4.1276, 1.5738, 55.7754, 3.7509, -2.2491, 2.4423},
    {-10.2949, 0.7805, -1.4303, 20.4812, 7.9810, 9.7798},
    {9.6118, 10.6552, 0.1247, 6.4573, 8.8003, 4.7020},
    {5.4705, 2.4895, -3.4123, -1.1035, -5.7354, 25.8114}
};
// Trims half-corrected (log-space) against the 2026-07-09 verification
// run to average out run-to-run deflection variance. RX (pitch) is
// additionally scaled ~0.85x — it felt oversensitive at equal scaling.
const float TRIM_POS[6] = {0.984, 1.187, 1.023, 0.740, 0.897, 0.992};
const float TRIM_NEG[6] = {1.453, 1.384, 2.000, 0.950, 1.450, 1.336};

// Soft dead zones, per axis (same order), in output units where 350 =
// full deflection. Output ramps from zero at the edge of the zone (no
// cliff), so these sit just above each axis's post-matrix noise floor.
// TZ drifts far more than the others and needs the widest zone.
const float DEAD_AXIS[6] = {40.0, 40.0, 80.0, 11.0, 14.0, 12.0};

// Sensitivity curve exponent per axis, applied after the soft dead
// zone: >1 gives fine control at light deflection while full deflection
// still reaches AXIS_LIMIT. 1.0 = linear. RX (pitch) runs steeper to
// tame its oversensitive mid-range.
const float CURVE_EXPO[6] = {1.7, 1.7, 1.7, 2.1, 1.7, 1.7};

// Residual cross-coupling corrections the linear DECOUPLE matrix can't
// express (the bleed is direction-dependent): after trims, each target
// axis subtracts CROSS_*[source][target] * source. Fitted from the
// 2026-07-09 verification capture; only rotation->translation and
// translation->TZ terms above 0.15 are corrected — untreated, rocking
// the cap (roll) dragged up to 76% of its value into Y-pan.
const float CROSS_POS[6][6] = {
    {0.0000, 0.0000, 0.5193, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, 0.5983, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000},
    {-0.4859, -0.2842, 0.4140, 0.0000, 0.0000, 0.0000},
    {0.1739, -0.3665, 0.0000, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, 0.2749, 0.0000, 0.0000, 0.0000}
};
const float CROSS_NEG[6][6] = {
    {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, -0.2084, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000},
    {0.0000, -0.1770, -0.2602, 0.0000, 0.0000, 0.0000},
    {0.4203, 0.7631, 0.0000, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000}
};

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
const unsigned long LED_CALIBRATING_COLOR = 0x0000FF;

// FSM timing
const long IDLE_SLEEP_TIMEOUT_MS = 2 * 60 * 1000;

}  // namespace Config
