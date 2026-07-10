// Hardware diagnostic firmware — NOT a SpaceMouse. Build with the
// `diag` environment (pio run -e diag). Enumerates as USB serial only
// and repeatedly reports, for each magnetometer and both load-switch
// polarities, whether the sensor responds on I2C. The LED ring's load
// switch alternates polarity each pass so a bright ring identifies the
// switch variant (AP22816A = enable HIGH, AP22816B = enable LOW).
//
// How to read the output:
//   - Ring BRIGHT while "LED_LS=HIGH"  -> A-variant switches (stock logic)
//   - Ring BRIGHT while "LED_LS=LOW"   -> B-variant switches (inverted)
//   - Ring dim/dark in BOTH            -> LED power joint or ring supply
//   - Sensor OK under exactly one polarity -> that switch works, note variant
//   - Sensor FAIL under both polarities    -> sensor/I2C joint on that board
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <Wire.h>

#include "Config.h"
#include "TLx493D_inc.hpp"

using namespace ifx::tlx493d;

namespace {
const int kMagPins[3] = {Config::PIN_MAG1_LS, Config::PIN_MAG2_LS,
                         Config::PIN_MAG3_LS};
const char* kMagNames[3] = {"MAG1(bottom)", "MAG2(top-left)",
                            "MAG3(top-right)"};

Adafruit_NeoPixel ring(Config::LED_COUNT, Config::PIN_LED_DATA,
                       NEO_GRB + NEO_KHZ800);

void setAll(int level) {
  for (int i = 0; i < 3; i++) {
    digitalWrite(kMagPins[i], level);
  }
}

// Probe one sensor with a given "on" level: everything off, one on,
// then attempt an I2C init and a field read at the default address.
void probeSensor(int idx, int onLevel) {
  const int offLevel = (onLevel == HIGH) ? LOW : HIGH;
  setAll(offLevel);
  delay(20);
  digitalWrite(kMagPins[idx], onLevel);
  delay(20);

  TLx493D_A2B6 sensor(Wire, TLx493D_IIC_ADDR_A0_e);
  const bool beginOk = sensor.begin(true, false, false, true);

  double x = 0, y = 0, z = 0, t = 0;
  bool readOk = false;
  if (beginOk) {
    readOk = sensor.getMagneticFieldAndTemperature(&x, &y, &z, &t);
  }

  Serial.print("  ");
  Serial.print(kMagNames[idx]);
  Serial.print(" on=");
  Serial.print(onLevel == HIGH ? "HIGH" : "LOW ");
  Serial.print(" begin=");
  Serial.print(beginOk ? "OK " : "FAIL");
  Serial.print(" read=");
  if (readOk) {
    Serial.print("OK  B=(");
    Serial.print(x, 2);
    Serial.print(", ");
    Serial.print(y, 2);
    Serial.print(", ");
    Serial.print(z, 2);
    Serial.println(") mT");
  } else {
    Serial.println("FAIL");
  }

  digitalWrite(kMagPins[idx], offLevel);
  delay(10);
}
}  // namespace

void setup() {
  Serial.begin(115200);

  pinMode(Config::PIN_LED_LS, OUTPUT);
  for (int i = 0; i < 3; i++) {
    pinMode(kMagPins[i], OUTPUT);
  }

  ring.begin();
  ring.setBrightness(Config::LED_BRIGHTNESS);

  Wire.begin();
  Wire.setClock(100000);  // conservative bus speed for diagnostics
}

void loop() {
  static int pass = 0;
  pass++;

  // Alternate the LED switch level each pass; always push a solid blue
  // frame so a working rail is obviously bright.
  const int ledLevel = (pass % 2 == 1) ? HIGH : LOW;
  digitalWrite(Config::PIN_LED_LS, ledLevel);
  delay(20);
  for (int i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, ring.Color(0, 0, 255));
  }
  ring.show();

  Serial.print("=== DIAG pass ");
  Serial.print(pass);
  Serial.print("  LED_LS=");
  Serial.print(ledLevel == HIGH ? "HIGH" : "LOW");
  Serial.println("  (is the ring bright right now?) ===");

  for (int onLevel : {HIGH, LOW}) {
    for (int i = 0; i < 3; i++) {
      probeSensor(i, onLevel);
    }
  }

  Serial.println();
  delay(3000);
}
