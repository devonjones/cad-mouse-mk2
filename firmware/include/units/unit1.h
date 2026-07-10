#pragma once

// Unit 1 — purple case. Calibration fitted 2026-07-09 from guided
// single-axis serial telemetry of this specific unit (see
// tools/calibration/README.md; fit_matrix.py ridge regression toward a
// diagonal prior, lambda=10).
//
// Pipeline per sample: v = SIGN_AXIS .* sensed, out = DECOUPLE @ v,
// then per-direction TRIM so full deflection lands at AXIS_LIMIT in
// both directions despite the asymmetric magnet/sensor response.
// DECOUPLE's off-diagonal terms cancel cross-axis bleed (untreated,
// rocking the cap produced an X-pan up to 4x the roll signal).
// Axis order: TX, TY, TZ, RX, RY, RZ.
inline constexpr int SIGN_AXIS[6] = {+1, -1, +1, -1, -1, -1};
inline constexpr float DECOUPLE[6][6] = {
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
inline constexpr float TRIM_POS[6] = {0.984, 1.187, 1.023, 0.740, 0.897, 0.992};
inline constexpr float TRIM_NEG[6] = {1.453, 1.384, 2.000, 0.950, 1.450, 1.336};

// Soft dead zones, per axis (same order), in output units where 350 =
// full deflection. Output ramps from zero at the edge of the zone (no
// cliff), so these sit just above each axis's post-matrix noise floor.
// TZ drifts far more than the others and needs the widest zone.
inline constexpr float DEAD_AXIS[6] = {40.0, 40.0, 80.0, 11.0, 14.0, 12.0};

// Sensitivity curve exponent per axis, applied after the soft dead
// zone: >1 gives fine control at light deflection while full deflection
// still reaches AXIS_LIMIT. 1.0 = linear. RX (pitch) runs steeper to
// tame its oversensitive mid-range.
inline constexpr float CURVE_EXPO[6] = {1.7, 1.7, 1.7, 2.1, 1.7, 1.7};

// Residual cross-coupling corrections the linear DECOUPLE matrix can't
// express (the bleed is direction-dependent): after trims, each target
// axis subtracts CROSS_*[source][target] * source. Fitted from the
// 2026-07-09 verification capture; only rotation->translation and
// translation->TZ terms above 0.15 are corrected — untreated, rocking
// the cap (roll) dragged up to 76% of its value into Y-pan.
inline constexpr float CROSS_POS[6][6] = {
    {0.0000, 0.0000, 0.5193, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, 0.5983, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000},
    {-0.4859, -0.2842, 0.4140, 0.0000, 0.0000, 0.0000},
    {0.1739, -0.3665, 0.0000, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, 0.2749, 0.0000, 0.0000, 0.0000}
};
inline constexpr float CROSS_NEG[6][6] = {
    {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, -0.2084, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000},
    {0.0000, -0.1770, -0.2602, 0.0000, 0.0000, 0.0000},
    {0.4203, 0.7631, 0.0000, 0.0000, 0.0000, 0.0000},
    {0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000}
};

// Default idle LED color (index into LED_IDLE_PALETTE): purple.
inline constexpr int LED_DEFAULT_COLOR_INDEX = 0;
