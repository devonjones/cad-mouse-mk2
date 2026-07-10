#include "controllers/HIDController.h"

#include <math.h>

#include "Config.h"

namespace {
// 3Dconnexion-compatible HID report descriptor, matching the report layout
// of a genuine SpaceMouse Compact:
//   Report 1: Translation (X, Y, Z) — 3 × int16, logical range ±350
//   Report 2: Rotation (Rx, Ry, Rz) — 3 × int16, logical range ±350
//   Report 3: Buttons — 2 buttons + padding, 4-byte report
const uint8_t kHidReportDescriptor[] PROGMEM = {
    0x05, 0x01,        // USAGE_PAGE (Generic Desktop)
    0x09, 0x08,        // USAGE (Multi-axis Controller)
    0xA1, 0x01,        // COLLECTION (Application)
    0xA1, 0x02,        // COLLECTION (Logical)
    0x85, 0x01,        // REPORT_ID (1) Translation
    0x16, 0xA2, 0xFE,  // LOGICAL_MINIMUM  (-350)
    0x26, 0x5E, 0x01,  // LOGICAL_MAXIMUM  (350)
    0x09, 0x30,        // USAGE (X)
    0x09, 0x31,        // USAGE (Y)
    0x09, 0x32,        // USAGE (Z)
    0x75, 0x10,        // REPORT_SIZE (16)
    0x95, 0x03,        // REPORT_COUNT (3)
    0x81, 0x02,        // INPUT (Data,Var,Abs)
    0xC0,              // END_COLLECTION
    0xA1, 0x02,        // COLLECTION (Logical)
    0x85, 0x02,        // REPORT_ID (2) Rotation
    0x16, 0xA2, 0xFE,  // LOGICAL_MINIMUM  (-350)
    0x26, 0x5E, 0x01,  // LOGICAL_MAXIMUM  (350)
    0x09, 0x33,        // USAGE (Rx)
    0x09, 0x34,        // USAGE (Ry)
    0x09, 0x35,        // USAGE (Rz)
    0x75, 0x10,        // REPORT_SIZE (16)
    0x95, 0x03,        // REPORT_COUNT (3)
    0x81, 0x02,        // INPUT (Data,Var,Abs)
    0xC0,              // END_COLLECTION
    0xA1, 0x02,        // COLLECTION (Logical)
    0x85, 0x03,        // REPORT_ID (3) Buttons
    0x05, 0x09,        // USAGE_PAGE (Button)
    0x19, 0x01,        // USAGE_MINIMUM (Button 1)
    0x29, 0x02,        // USAGE_MAXIMUM (Button 2)
    0x15, 0x00,        // LOGICAL_MINIMUM (0)
    0x25, 0x01,        // LOGICAL_MAXIMUM (1)
    0x75, 0x01,        // REPORT_SIZE (1)
    0x95, 0x02,        // REPORT_COUNT (2)
    0x81, 0x02,        // INPUT (Data,Var,Abs)
    0x95, 0x1E,        // REPORT_COUNT (30) padding — 4-byte total report
    0x81, 0x01,        // INPUT (Const,Array,Abs)
    0xC0,              // END_COLLECTION
    0xC0               // END_COLLECTION
};
}  // namespace

void HIDController::begin() {
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }
  // Adafruit_USBD_Device::begin() unconditionally registers a CDC serial
  // interface alongside HID. A real SpaceMouse Compact is a pure-HID
  // device; strip CDC before adding our HID interface so the composite
  // descriptor presented to the driver matches the genuine hardware.
  // Debug builds keep CDC — Serial.begin() cannot re-add an interface
  // TinyUSB already considers registered, so it must never be stripped.
#ifndef ENABLE_DEBUG_SERIAL
  TinyUSBDevice.clearConfiguration();
#endif

  usbHid_.setReportDescriptor(kHidReportDescriptor, sizeof(kHidReportDescriptor));
  usbHid_.setPollInterval(1);
  usbHid_.begin();

  // The core may have attached the default composite (CDC+HID) descriptor
  // before setup() ran; force a re-enumeration unconditionally so the host
  // always reads the final HID-only descriptor.
  TinyUSBDevice.detach();
  delay(100);  // long enough for hubs/hosts to register the disconnect
  TinyUSBDevice.attach();
}

void HIDController::task() { TinyUSBDevice.task(); }

HIDController::ReportTranslation HIDController::makeTranslationReport(
    const float motion[6]) {
  ReportTranslation t{};
  t.x = static_cast<int16_t>(motion[0]);
  t.y = static_cast<int16_t>(motion[1]);
  t.z = static_cast<int16_t>(motion[2]);
  return t;
}

HIDController::ReportRotation HIDController::makeRotationReport(
    const float motion[6]) {
  ReportRotation r{};
  r.rx = static_cast<int16_t>(motion[3]);
  r.ry = static_cast<int16_t>(motion[4]);
  r.rz = static_cast<int16_t>(motion[5]);
  return r;
}

bool HIDController::translationChanged(const ReportTranslation& t) const {
  return t.x != lastSentT_.x ||
         t.y != lastSentT_.y ||
         t.z != lastSentT_.z;
}

bool HIDController::rotationChanged(const ReportRotation& r) const {
  return r.rx != lastSentR_.rx ||
         r.ry != lastSentR_.ry ||
         r.rz != lastSentR_.rz;
}

bool HIDController::sendReports(const float motion[6], uint16_t buttonBits) {
  const ReportTranslation t = makeTranslationReport(motion);
  const ReportRotation r = makeRotationReport(motion);
  const unsigned long now = millis();

  // Drivers expect a deflected SpaceMouse to keep streaming, so a report
  // is due when its values changed or when it is non-zero and stale.
  const bool tActive = t.x != 0 || t.y != 0 || t.z != 0;
  const bool rActive = r.rx != 0 || r.ry != 0 || r.rz != 0;
  const bool needT =
      translationChanged(t) ||
      (tActive && (now - lastSentTMs_) >= Config::HID_KEEPALIVE_MS);
  const bool needR =
      rotationChanged(r) ||
      (rActive && (now - lastSentRMs_) >= Config::HID_KEEPALIVE_MS);
  const bool needButtons = (buttonBits != buttonBitsSent_);

  if (!needT && !needR && !needButtons) return false;
  if (!usbHid_.ready()) return false;

  // TinyUSB claims the IN endpoint per report; further sendReport() calls
  // in the same loop iteration fail until tud_task() runs. Dispatch at
  // most one report per call and cycle nextReportSlot_ so translation,
  // rotation, and buttons don't starve each other under combined motion.
  for (uint8_t step = 0; step < 3; step++) {
    const uint8_t slot = (nextReportSlot_ + step) % 3;
    switch (slot) {
      case 0:
        if (needT && usbHid_.sendReport(0x01, &t, sizeof(t))) {
          lastSentT_ = t;
          lastSentTMs_ = now;
          nextReportSlot_ = 1;
          return true;
        }
        break;
      case 1:
        if (needR && usbHid_.sendReport(0x02, &r, sizeof(r))) {
          lastSentR_ = r;
          lastSentRMs_ = now;
          nextReportSlot_ = 2;
          return true;
        }
        break;
      case 2: {
        if (!needButtons) break;
        ReportButtons btn{};
        btn.bits = buttonBits & 0x0003;
        if (usbHid_.sendReport(0x03, &btn, sizeof(btn))) {
          buttonBitsSent_ = buttonBits;
          nextReportSlot_ = 0;
          return true;
        }
        break;
      }
    }
  }
  return false;
}
