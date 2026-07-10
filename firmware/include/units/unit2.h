#pragma once

// Unit 2 — black case. NOT YET CALIBRATED: these are bootstrap values
// for the initial guided capture (tools/calibration/README.md). The
// diagonal matches fit_matrix.py's GAINS_OLD, so a capture taken on
// this config feeds the fit directly. Replace with fitted values.
const int SIGN_AXIS[6] = {+1, -1, +1, -1, -1, -1};
const float DECOUPLE[6][6] = {
    {48.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 48.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 42.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 18.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 30.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 20.0}
};
const float TRIM_POS[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
const float TRIM_NEG[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

const float DEAD_AXIS[6] = {25.0, 25.0, 40.0, 12.0, 16.0, 12.0};

// Linear until this unit's response is measured.
const float CURVE_EXPO[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

const float CROSS_POS[6][6] = {
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};
const float CROSS_NEG[6][6] = {
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
};

// Default idle LED color (index into LED_IDLE_PALETTE): green.
const int LED_DEFAULT_COLOR_INDEX = 1;
