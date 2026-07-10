#include "controllers/MotionController.h"

#include <Arduino.h>
#include <math.h>

#include "Config.h"

namespace {
enum RawIndex {
  RAW_MAG1_X = 0,
  RAW_MAG1_Y,
  RAW_MAG1_Z,
  RAW_MAG2_X,
  RAW_MAG2_Y,
  RAW_MAG2_Z,
  RAW_MAG3_X,
  RAW_MAG3_Y,
  RAW_MAG3_Z
};

enum AxisIndex {
  AXIS_TX = 0,
  AXIS_TY,
  AXIS_TZ,
  AXIS_RX,
  AXIS_RY,
  AXIS_RZ
};

// Sign fixes, fitted decoupling matrix (gain + cross-axis cancellation),
// then per-direction trims for the asymmetric magnet/sensor response.
void decouple(const float sensed[6], float y[6]) {
  float v[6];
  for (int i = 0; i < 6; i++) {
    v[i] = Config::SIGN_AXIS[i] * sensed[i];
  }
  for (int i = 0; i < 6; i++) {
    float acc = 0.0f;
    for (int j = 0; j < 6; j++) {
      acc += Config::DECOUPLE[i][j] * v[j];
    }
    y[i] = acc * (acc >= 0.0f ? Config::TRIM_POS[i] : Config::TRIM_NEG[i]);
  }
}

// Second-stage correction for the direction-dependent coupling the
// linear matrix leaves behind (e.g. roll dragging pan along).
void applyCrossFix(const float y[6], float yc[6]) {
  for (int i = 0; i < 6; i++) {
    float acc = y[i];
    for (int j = 0; j < 6; j++) {
      if (j == i) continue;
      const float c = (y[j] >= 0.0f) ? Config::CROSS_POS[j][i]
                                     : Config::CROSS_NEG[j][i];
      acc -= c * y[j];
    }
    yc[i] = acc;
  }
}
}  // namespace

void MotionController::reset() {
  for (int i = 0; i < 6; i++) {
    filt_[i] = 0.0f;
  }
  motionActive_ = false;
}

float MotionController::clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float MotionController::hardZero(float v, float thr) {
  return (fabsf(v) < thr) ? 0.0f : v;
}

float MotionController::lowpass(float prev, float x, float dt, float tau) {
  if (tau <= 0.0f) return x;
  const float a = dt / (tau + dt);
  return prev + a * (x - prev);
}

float MotionController::axisBaseDead(int i) {
  return Config::DEAD_AXIS[i];
}

void MotionController::compute(const float raw[9], const float* baseline, float dt,
                               float out[6]) {
  // Baseline subtraction converts magnetic deltas around the calibrated rest pose.
  const float mag1x = raw[RAW_MAG1_X] - baseline[RAW_MAG1_X];
  const float mag1y = raw[RAW_MAG1_Y] - baseline[RAW_MAG1_Y];
  const float mag1z = raw[RAW_MAG1_Z] - baseline[RAW_MAG1_Z];
  const float mag2x = raw[RAW_MAG2_X] - baseline[RAW_MAG2_X];
  const float mag2y = raw[RAW_MAG2_Y] - baseline[RAW_MAG2_Y];
  const float mag2z = raw[RAW_MAG2_Z] - baseline[RAW_MAG2_Z];
  const float mag3x = raw[RAW_MAG3_X] - baseline[RAW_MAG3_X];
  const float mag3y = raw[RAW_MAG3_Y] - baseline[RAW_MAG3_Y];
  const float mag3z = raw[RAW_MAG3_Z] - baseline[RAW_MAG3_Z];

  // Translation:
  //   Tx = (mag1x + mag2x + mag3x) / 3
  //   Ty = (mag1y + mag2y + mag3y) / 3
  //   Tz = (mag1z + mag2z + mag3z) / 3
  const float tx = (mag1x + mag2x + mag3x) / 3.0f;
  const float ty = (mag1y + mag2y + mag3y) / 3.0f;
  const float tz = (mag1z + mag2z + mag3z) / 3.0f;

  // Physical PCB layout:
  // MAG2 = top left, MAG3 = top right, MAG1 = bottom.
  const float mag2PosX = -0.5f;
  const float mag2PosY = sqrtf(3.0f) / 6.0f;

  const float mag3PosX = 0.5f;
  const float mag3PosY = sqrtf(3.0f) / 6.0f;

  const float mag1PosX = 0.0f;
  const float mag1PosY = -sqrtf(3.0f) / 3.0f;

  // Rotation estimates:
  //   Ry = mag3z - mag2z
  //     right sensor minus left sensor
  //     -> side to side tilt across the top edge
  //
  //   Rx = sqrt(3) * (mag2z + mag3z - 2 * mag1z) / 3
  //     top pair minus bottom sensor
  //     -> front/back tilt of the triangle
  const float rx = (sqrtf(3.0f) * (mag2z + mag3z - 2.0f * mag1z)) / 3.0f;
  const float ry = (mag3z - mag2z);

  //   Rz = sum_i (posXi * magYi - posYi * magXi)
  // Each sensor contributes according to its x/y position in the triangle.
  const float swirlNum =
      (mag2PosX * mag2y - mag2PosY * mag2x) +
      (mag3PosX * mag3y - mag3PosY * mag3x) +
      (mag1PosX * mag1y - mag1PosY * mag1x);
  const float rz = swirlNum;

  const float sensed[6] = {tx, ty, tz, rx, ry, rz};
  float y[6];
  decouple(sensed, y);

  float yc[6];
  applyCrossFix(y, yc);

  for (int i = 0; i < 6; i++) {
    gained_[i] = yc[i];
  }

  // Soft dead zone + sensitivity curve, then filter and clamp. The
  // shaped value ramps smoothly from zero at the dead-zone edge, so a
  // light touch produces small-but-present motion instead of nothing.
  motionActive_ = false;
  for (int i = 0; i < 6; i++) {
    const float shaped =
        shapeAxis(yc[i], axisBaseDead(i), Config::CURVE_EXPO[i]);

    filt_[i] = lowpass(filt_[i], shaped, dt, Config::SMOOTH_TAU_S);

    const float limited =
        clampf(filt_[i], -Config::AXIS_LIMIT, Config::AXIS_LIMIT);
    // Epsilon cutoff so the filter tail decays to a true zero.
    out[i] = hardZero(limited, 1.0f);
    if (out[i] != 0.0f) {
      motionActive_ = true;
    }
  }
}

float MotionController::shapeAxis(float v, float dead, float expo) {
  const float mag = fabsf(v);
  if (mag <= dead) {
    return 0.0f;
  }
  float m = (mag - dead) / (Config::AXIS_LIMIT - dead);
  if (m > 1.0f) m = 1.0f;
  const float curved = powf(m, expo) * Config::AXIS_LIMIT;
  return (v < 0.0f) ? -curved : curved;
}

bool MotionController::hasMotionActivity() const { return motionActive_; }
