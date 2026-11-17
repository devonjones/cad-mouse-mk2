#include "TuningConfig.h"

#include <math.h>
#include <string.h>

#include "Config.h"

namespace {

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc >> 1) ^ (0xEDB88320 & (-(int32_t)(crc & 1)));
    }
  }
  return ~crc;
}

TuningConfig makeDefaults() {
  TuningConfig c;
  memset(&c, 0, sizeof(c));
  c.magic = TUNING_MAGIC;
  c.version = TUNING_VERSION;
  for (int i = 0; i < 6; i++) {
    c.sign_axis[i] = Config::SIGN_AXIS[i];
    c.trim_pos[i] = Config::TRIM_POS[i];
    c.trim_neg[i] = Config::TRIM_NEG[i];
    c.dead_axis[i] = Config::DEAD_AXIS[i];
    c.curve_expo[i] = Config::CURVE_EXPO[i];
    for (int j = 0; j < 6; j++) {
      c.decouple[i][j] = Config::DECOUPLE[i][j];
      c.cross_pos[i][j] = Config::CROSS_POS[i][j];
      c.cross_neg[i][j] = Config::CROSS_NEG[i][j];
    }
  }
  c.smooth_tau_s = Config::SMOOTH_TAU_S;
  c.led_default_color_index = Config::LED_DEFAULT_COLOR_INDEX;
  return c;
}

// Dynamic init runs before setup(), so tuning() is valid from the start.
TuningConfig active_ = makeDefaults();
TuningStatus status_ = TuningStatus::Absent;

bool coeffOk(float x) { return isfinite(x) && fabsf(x) <= TUNING_MAX_COEFF; }

bool valuesSane(const TuningConfig& c) {
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 6; j++) {
      if (!coeffOk(c.decouple[i][j]) || !coeffOk(c.cross_pos[i][j]) ||
          !coeffOk(c.cross_neg[i][j])) {
        return false;
      }
    }
    if (c.sign_axis[i] != 1 && c.sign_axis[i] != -1) return false;
    if (!(c.dead_axis[i] >= 0.0f && c.dead_axis[i] < Config::AXIS_LIMIT)) return false;
    if (!(c.curve_expo[i] >= 0.5f && c.curve_expo[i] <= 5.0f)) return false;
    if (!(c.trim_pos[i] > 0.0f && c.trim_pos[i] < 10.0f)) return false;
    if (!(c.trim_neg[i] > 0.0f && c.trim_neg[i] < 10.0f)) return false;
  }
  if (!(c.smooth_tau_s >= 0.0f && c.smooth_tau_s <= 1.0f)) return false;
  if (c.led_default_color_index >= Config::LED_IDLE_PALETTE_COUNT) return false;
  return true;
}

TuningStatus validate(const TuningConfig& c) {
  if (c.magic != TUNING_MAGIC) return TuningStatus::Absent;
  if (c.version != TUNING_VERSION) return TuningStatus::BadVersion;
  const uint32_t expected = crc32(reinterpret_cast<const uint8_t*>(&c),
                                  sizeof(c) - sizeof(c.crc32));
  if (c.crc32 != expected) return TuningStatus::BadCrc;
  if (!valuesSane(c)) return TuningStatus::BadValues;
  return TuningStatus::Loaded;
}
}  // namespace

void loadTuning() {
  // XIP flash is memory-mapped; copy the candidate block into RAM before
  // validating so all later reads are RAM-speed and tear-free.
  TuningConfig candidate;
  memcpy(&candidate, reinterpret_cast<const void*>(TUNING_FLASH_ADDR),
         sizeof(candidate));
  status_ = validate(candidate);
  if (status_ == TuningStatus::Loaded) active_ = candidate;
}

TuningStatus tuningStatus() { return status_; }

const TuningConfig& tuning() { return active_; }
