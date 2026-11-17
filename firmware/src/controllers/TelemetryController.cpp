#include "controllers/TelemetryController.h"
#include <Arduino.h>
#include "Config.h"
#include "Controllers.h"
#include "TuningConfig.h"

namespace {
const int kPrintEvery = 5;
}

void TelemetryController::begin() { tick_ = 0; }

bool TelemetryController::enabled() const { return Config::ENABLE_TELEMETRY; }

void TelemetryController::publish(const float motion[6], int buttonBits,
                                  bool hidReportSent) {
  if (!enabled()) {
    return;
  }

  tick_++;
  if ((tick_ % kPrintEvery) != 0) {
    return;
  }

  // Repeated in each telemetry frame: TinyUSB CDC drops output sent
  // before the host opens the port, so a one-time boot print is never seen.
  Serial.print(">tuning_status:");
  Serial.println(static_cast<int>(tuningStatus()));
  Serial.print(">sensor_fast_mode:");
  Serial.println(sensorController.fastModeSensors());
  Serial.print(">X:");
  Serial.println(motion[0]);
  Serial.print(">Y:");
  Serial.println(motion[1]);
  Serial.print(">Z:");
  Serial.println(motion[2]);
  Serial.print(">Rx:");
  Serial.println(motion[3]);
  Serial.print(">Ry:");
  Serial.println(motion[4]);
  Serial.print(">Rz:");
  Serial.println(motion[5]);
  Serial.print(">btn:");
  Serial.println(buttonBits & 0x0003);
  Serial.print(">hid:");
  Serial.println(hidReportSent ? 1 : 0);
}
