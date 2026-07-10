#pragma once

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

class HIDController {
 public:
  void begin();
  void task();
  bool sendReports(const float motion[6], uint16_t buttonBits);

 private:
  // Report ID 1: Translation (X, Y, Z)
  struct __attribute__((packed)) ReportTranslation {
    int16_t x, y, z;
  };

  // Report ID 2: Rotation (Rx, Ry, Rz)
  struct __attribute__((packed)) ReportRotation {
    int16_t rx, ry, rz;
  };

  struct __attribute__((packed)) ReportButtons {
    uint32_t bits;
  };

  static ReportTranslation makeTranslationReport(const float motion[6]);
  static ReportRotation makeRotationReport(const float motion[6]);
  bool translationChanged(const ReportTranslation& t) const;
  bool rotationChanged(const ReportRotation& r) const;

  Adafruit_USBD_HID usbHid_;
  uint16_t buttonBitsSent_ = 0;
  ReportTranslation lastSentT_{};
  ReportRotation lastSentR_{};
  unsigned long lastSentTMs_ = 0;
  unsigned long lastSentRMs_ = 0;
  // Round-robin cursor: the HID IN endpoint carries exactly one report per
  // USB polling interval, so sendReports() delivers at most one report per
  // call and rotates which slot gets first chance.
  uint8_t nextReportSlot_ = 0;
};
