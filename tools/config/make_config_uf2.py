#!/usr/bin/env python3
"""Build a per-unit config UF2 from a configs/<unit>.json file.

The output UF2 writes a TuningConfig block (see firmware/include/
TuningConfig.h — layouts must match, version-gated) to TUNING_FLASH_ADDR.
Deploy by copying it to the RPI-RP2 bootloader drive, either after the
firmware UF2 or merged into one file with merge_uf2.py.

Usage: python3 make_config_uf2.py <config.json> <out.uf2>
"""
import json
import math
import struct
import sys
import zlib

TUNING_MAGIC = 0x4D444143  # 'CADM'
TUNING_VERSION = 1
TUNING_FLASH_ADDR = 0x101FE000
AXIS_LIMIT = 350.0

UF2_MAGIC0 = 0x0A324655
UF2_MAGIC1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID = 0x00002000
RP2040_FAMILY_ID = 0xE48BFF56


# Must match LED_IDLE_PALETTE_COUNT in firmware/include/Config.h; the
# firmware rejects the whole block (falls back to defaults) if it's out of
# range, so catch it here instead.
LED_PALETTE_COUNT = 7

FIELDS = ("sign_axis", "decouple", "trim_pos", "trim_neg", "dead_axis",
          "curve_expo", "cross_pos", "cross_neg", "smooth_tau_s",
          "led_default_color_index")


class ConfigError(Exception):
    pass


def check(cond, msg):
    # Explicit checks, not assert: these must still run under python -O.
    if not cond:
        raise ConfigError(msg)


def finite(x):
    return isinstance(x, (int, float)) and not isinstance(x, bool) \
        and math.isfinite(x)


def vec(cfg, key, n=6):
    v = cfg[key]
    check(isinstance(v, list) and len(v) == n, f"{key}: expected {n} values")
    check(all(finite(x) for x in v), f"{key}: non-finite or non-numeric value")
    return v


def flat(cfg, key):
    m = cfg[key]
    check(isinstance(m, list) and len(m) == 6, f"{key}: expected 6 rows")
    return [x for i in range(6) for x in vec({key: m[i]}, key)]


def build_blob(cfg):
    keys = {k for k in cfg if not k.startswith("_")}
    check(keys == set(FIELDS),
          f"keys: missing {sorted(set(FIELDS) - keys)}, "
          f"unknown {sorted(keys - set(FIELDS))}")
    signs = vec(cfg, "sign_axis")
    check(all(s in (1, -1) for s in signs), "sign_axis entries must be +/-1")
    dead = vec(cfg, "dead_axis")
    check(all(0.0 <= d < AXIS_LIMIT for d in dead), "dead zone out of range")
    expo = vec(cfg, "curve_expo")
    check(all(0.5 <= e <= 5.0 for e in expo), "curve expo out of range")
    for key in ("trim_pos", "trim_neg"):
        check(all(0.0 < t < 10.0 for t in vec(cfg, key)), f"{key} out of range")
    tau = cfg["smooth_tau_s"]
    check(finite(tau) and 0.0 <= tau <= 1.0, "smooth_tau_s out of range")
    led = cfg["led_default_color_index"]
    check(isinstance(led, int) and not isinstance(led, bool)
          and 0 <= led < LED_PALETTE_COUNT,
          f"led_default_color_index must be 0..{LED_PALETTE_COUNT - 1}")

    body = struct.pack(
        "<IHH6b2x36f6f6f6f6f36f36ffB3x",
        TUNING_MAGIC,
        TUNING_VERSION,
        0,
        *signs,
        *flat(cfg, "decouple"),
        *vec(cfg, "trim_pos"),
        *vec(cfg, "trim_neg"),
        *dead,
        *expo,
        *flat(cfg, "cross_pos"),
        *flat(cfg, "cross_neg"),
        tau,
        led,
    )
    crc = zlib.crc32(body) & 0xFFFFFFFF
    blob = body + struct.pack("<I", crc)
    check(len(blob) == 556, f"blob is {len(blob)} bytes, expected 556")
    return blob


def to_uf2(blob, base_addr):
    payload_per_block = 256
    blocks = [blob[i:i + payload_per_block]
              for i in range(0, len(blob), payload_per_block)]
    out = bytearray()
    for i, chunk in enumerate(blocks):
        padded = chunk + b"\x00" * (payload_per_block - len(chunk))
        header = struct.pack(
            "<IIIIIIII",
            UF2_MAGIC0, UF2_MAGIC1, UF2_FLAG_FAMILY_ID,
            base_addr + i * payload_per_block,
            payload_per_block, i, len(blocks), RP2040_FAMILY_ID)
        block = header + padded
        block += b"\x00" * (508 - len(block))
        block += struct.pack("<I", UF2_MAGIC_END)
        assert len(block) == 512
        out += block
    return bytes(out)


def main():
    if len(sys.argv) != 3:
        print(f"Usage: python3 {sys.argv[0]} <config.json> <out.uf2>")
        sys.exit(1)
    with open(sys.argv[1]) as f:
        cfg = json.load(f)
    try:
        blob = build_blob(cfg)
    except (ConfigError, KeyError) as e:
        sys.exit(f"{sys.argv[1]}: invalid config: {e}")
    uf2 = to_uf2(blob, TUNING_FLASH_ADDR)
    with open(sys.argv[2], "wb") as f:
        f.write(uf2)
    print(f"{sys.argv[2]}: {len(uf2)} bytes "
          f"({len(uf2) // 512} blocks at 0x{TUNING_FLASH_ADDR:08X}), "
          f"crc32 0x{struct.unpack('<I', blob[-4:])[0]:08X}")


if __name__ == "__main__":
    main()
