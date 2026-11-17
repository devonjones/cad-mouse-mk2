#include <Arduino.h>

#include "Config.h"
#include "Controllers.h"
#include "StateMachine.h"
#include "TuningConfig.h"

InputController inputController;
LEDController ledController;
SensorController sensorController;
MotionController motionController;
HIDController hidController;
TelemetryController telemetryController;

void setup() {
  loadTuning();

  // Initialize USB HID first
  hidController.begin();

  if (Config::ENABLE_TELEMETRY) {
    Serial.begin(115200);
    delay(200);
    // 0 Loaded, 1 Absent, 2 BadVersion, 3 BadCrc, 4 BadValues.
    Serial.print(">tuning_status:");
    Serial.println(static_cast<int>(tuningStatus()));
    const uint32_t* raw = reinterpret_cast<const uint32_t*>(TUNING_FLASH_ADDR);
    Serial.print(">flash_magic:");
    Serial.println(raw[0], HEX);
    Serial.print(">active_decouple00:");
    Serial.println(tuning().decouple[0][0]);
  }

  inputController.begin();
  ledController.begin();
  sensorController.begin();
  motionController.reset();
  telemetryController.begin();

  stateMachine.changeState(&StateMachine::calibratingState);
}

void loop() {
  hidController.task();
  stateMachine.update();
}
