#pragma once

#include <Arduino.h>
#include <stddef.h>

// Per-unit tuning data, deployed as a UF2 block written to TUNING_FLASH_ADDR
// (see tools/config/README.md) so one compiled binary serves every unit.
// At boot loadTuning() validates magic/version/CRC and value sanity; on any
// failure the compiled defaults from units/default.h are used instead.
//
// Layout is fixed and shared with tools/config/make_config_uf2.py — bump
// TUNING_VERSION whenever it changes. All multi-byte fields little-endian;
// axis order TX, TY, TZ, RX, RY, RZ; output units AXIS_LIMIT (350) = full
// deflection.
// aligned(4): every float sits at a 4-byte offset, so with a word-aligned
// base the compiler can use word loads — packed alone forces byte-wise
// access on the Cortex-M0+ (no unaligned loads) in the per-sample loops.
struct __attribute__((packed, aligned(4))) TuningConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  int8_t sign_axis[6];
  uint8_t pad0[2];
  float decouple[6][6];
  float trim_pos[6];
  float trim_neg[6];
  float dead_axis[6];
  float curve_expo[6];
  float cross_pos[6][6];
  float cross_neg[6][6];
  float smooth_tau_s;
  uint8_t led_default_color_index;
  uint8_t pad1[3];
  uint32_t crc32;  // over all preceding bytes
};
static_assert(sizeof(TuningConfig) == 556, "layout is shared with tooling");
static_assert(offsetof(TuningConfig, decouple) == 16, "layout is shared with tooling");
static_assert(offsetof(TuningConfig, smooth_tau_s) == 544, "layout is shared with tooling");
static_assert(offsetof(TuningConfig, crc32) == 552, "layout is shared with tooling");

const uint32_t TUNING_MAGIC = 0x4D444143;  // 'CADM'
const uint16_t TUNING_VERSION = 1;
// Second-to-last 4KB flash sector (the last one, 0x101FF000, is arduino-pico
// EEPROM emulation). The linker's FLASH region still covers this sector, so
// the firmware image must stay below it.
const uint32_t TUNING_FLASH_ADDR = 0x101FE000;

// Bound on |decouple| and |cross| entries. Fitted values are < 200; a
// huge finite coefficient would overflow the float pipeline to inf/NaN.
const float TUNING_MAX_COEFF = 1000.0f;

enum class TuningStatus : uint8_t {
  Loaded,      // valid block in flash is active
  Absent,      // no block (magic mismatch, e.g. erased flash)
  BadVersion,  // block from a different TUNING_VERSION
  BadCrc,
  BadValues,   // CRC ok but valuesSane() rejected it
};

// Loads and validates the flash block; on failure the compiled defaults
// stay active. Call once, before anything consumes tuning().
void loadTuning();
TuningStatus tuningStatus();

// The active tuning. Holds the compiled defaults until loadTuning()
// accepts a flash block, so it is never uninitialised.
const TuningConfig& tuning();
