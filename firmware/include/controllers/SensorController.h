#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "TLx493D_inc.hpp"

class SensorController {
 public:
  SensorController();

  void begin();
  void readRaw(float out[9]);

  void beginCalibration();
  void updateCalibration();
  bool calibrationDone() const;

  const float* baseline() const;
  // How many sensors accepted fast mode (3 expected); debug telemetry
  // reports it because a boot-time print is never seen.
  int fastModeSensors() const { return fastModeSensors_; }

 private:
  static void powerOff(int pin);
  static void powerOn(int pin);

  ifx::tlx493d::TLx493D_A2B6 mag1Sensor_;
  ifx::tlx493d::TLx493D_A2B6 mag2Sensor_;
  ifx::tlx493d::TLx493D_A2B6 mag3Sensor_;

  bool calibrationActive_ = false;
  bool calibrationDone_ = false;
  int calibrationSamples_ = 0;
  unsigned long lastCalibrationSampleMs_ = 0;
  float calibrationSum_[9] = {};
  float baseline_[9] = {};
  int fastModeSensors_ = 0;
};
