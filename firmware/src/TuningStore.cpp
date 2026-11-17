#include "TuningConfig.h"

#include <string.h>

#include "Config.h"

namespace {
TuningConfig active_;

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

void loadDefaults(TuningConfig& c) {
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
}

bool valuesSane(const TuningConfig& c) {
  for (int i = 0; i < 6; i++) {
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
}  // namespace

void TuningStore::begin() {
  // XIP flash is memory-mapped; copy the candidate block into RAM before
  // validating so all later reads are RAM-speed and tear-free.
  TuningConfig candidate;
  memcpy(&candidate, reinterpret_cast<const void*>(TUNING_FLASH_ADDR),
         sizeof(candidate));

  const uint32_t expected =
      crc32(reinterpret_cast<const uint8_t*>(&candidate),
            sizeof(candidate) - sizeof(candidate.crc32));

  if (candidate.magic == TUNING_MAGIC &&
      candidate.version == TUNING_VERSION &&
      candidate.crc32 == expected && valuesSane(candidate)) {
    active_ = candidate;
    fromFlash_ = true;
    return;
  }

  loadDefaults(active_);
  fromFlash_ = false;
}

const TuningConfig& tuning() { return active_; }
