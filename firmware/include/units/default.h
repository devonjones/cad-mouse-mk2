#pragma once

// Compiled-default tuning: safe bootstrap values used only when no valid
// per-unit config block is present in flash (see TuningConfig.h and
// tools/config/README.md — real units get their calibration deployed as a
// config UF2, not compiled in). The diagonal matches fit_matrix.py's
// GAINS_OLD, so a calibration capture taken on these defaults feeds the
// fit directly.
//
// Axis order: TX, TY, TZ, RX, RY, RZ. Output units: AXIS_LIMIT (350) =
// full deflection. Pipeline semantics live on decouple()/applyCrossFix()
// in MotionController.cpp.
inline constexpr int SIGN_AXIS[6] = {+1, -1, +1, -1, -1, -1};
inline constexpr float DECOUPLE[6][6] = {
    {48.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 48.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 42.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 18.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 30.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 20.0}
};
inline constexpr float TRIM_POS[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
inline constexpr float TRIM_NEG[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

inline constexpr float DEAD_AXIS[6] = {25.0, 25.0, 40.0, 12.0, 16.0, 12.0};

// Linear until the unit's response is measured.
inline constexpr float CURVE_EXPO[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

inline constexpr float CROSS_POS[6][6] = {
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};
inline constexpr float CROSS_NEG[6][6] = {
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

// Default idle LED color (index into LED_IDLE_PALETTE): purple.
inline constexpr int LED_DEFAULT_COLOR_INDEX = 0;
