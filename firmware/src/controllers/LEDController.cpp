#include "controllers/LEDController.h"

#include <EEPROM.h>

#include "Config.h"

namespace {
// Flash-persisted settings layout (RP2040 EEPROM emulation).
const int kEepromSize = 256;
const int kMagicAddr = 0;
const int kColorIndexAddr = 1;
const uint8_t kMagic = 0x42;
}

LEDController::LEDController()
    : ring_(Config::LED_COUNT, Config::PIN_LED_DATA,
            NEO_GRB + NEO_KHZ800) {}

void LEDController::fillAll(unsigned long color) {
  for (int i = 0; i < ring_.numPixels(); i++) {
    ring_.setPixelColor(i, color);
  }
}

unsigned long LEDController::toNeoColor(unsigned long color) {
  int r = (color >> 16) & 0xFF;
  int g = (color >> 8) & 0xFF;
  int b = color & 0xFF;
  return ring_.Color(r, g, b);
}

void LEDController::begin() {
  pinMode(Config::PIN_LED_LS, OUTPUT);
  digitalWrite(Config::PIN_LED_LS, LOW);

  idleColorIndex_ = Config::LED_DEFAULT_COLOR_INDEX;
  EEPROM.begin(kEepromSize);
  if (EEPROM.read(kMagicAddr) == kMagic) {
    const uint8_t idx = EEPROM.read(kColorIndexAddr);
    if (idx < Config::LED_IDLE_PALETTE_COUNT) {
      idleColorIndex_ = idx;
    }
  }

  ring_.begin();
  ring_.setBrightness(Config::LED_BRIGHTNESS);
  ring_.show();
}

unsigned long LEDController::idleColor() const {
  return Config::LED_IDLE_PALETTE[idleColorIndex_];
}

void LEDController::cycleIdleColor() {
  idleColorIndex_ = (idleColorIndex_ + 1) % Config::LED_IDLE_PALETTE_COUNT;
  EEPROM.write(kMagicAddr, kMagic);
  EEPROM.write(kColorIndexAddr, static_cast<uint8_t>(idleColorIndex_));
  EEPROM.commit();
  setSolid(idleColor());
}

void LEDController::setPower(bool enabled) {
  if (enabled == isPowered_) {
    return;
  }

  isPowered_ = enabled;
  digitalWrite(Config::PIN_LED_LS, enabled ? HIGH : LOW);
  delay(10);
  
}

void LEDController::setSolid(unsigned long color) {
  mode_ = Mode::Solid;
  color_ = toNeoColor(color);
  setPower(true);
  fillAll(color_);
  ring_.show();
}

void LEDController::startSpinner(unsigned long color) {
  mode_ = Mode::Spinner;
  color_ = toNeoColor(color);
  spinnerIndex_ = 0;
  lastSpinnerStepMs_ = 0;
  setPower(true);
}

void LEDController::updateSpinner() {
  if (mode_ != Mode::Spinner) {
    return;
  }

  const unsigned long now = millis();
  if (lastSpinnerStepMs_ != 0 && (now - lastSpinnerStepMs_) < 60) {
    return;
  }
  lastSpinnerStepMs_ = now;

  fillAll(0);
  int pixelCount = ring_.numPixels();
  ring_.setPixelColor(spinnerIndex_, color_);
  ring_.show();

  spinnerIndex_++;
  if (spinnerIndex_ >= pixelCount) {
    spinnerIndex_ = 0;
  }
}

void LEDController::off() {
  mode_ = Mode::Off;
  fillAll(0);
  ring_.show();
  setPower(false);
}
