#pragma once

class MotionController {
 public:
  void reset();
  void compute(const float raw[9], const float* baseline, float dt, float out[6]);
  bool hasMotionActivity() const;
  // Signed+gained axis values before smoothing, dead zone, and clamping.
  // Telemetry publishes these in debug builds so gains can be calibrated
  // against true full-deflection magnitudes.
  const float* gained() const { return gained_; }

 private:
  static float clampf(float v, float lo, float hi);
  static float hardZero(float v, float thr);
  static float shapeAxis(float v, float dead, float expo);
  static float lowpass(float prev, float x, float dt, float tau);
  static float axisBaseDead(int i);
  float filt_[6] = {};
  float gained_[6] = {};
  bool motionActive_ = false;
};
