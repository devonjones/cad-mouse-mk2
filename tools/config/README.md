# Per-unit config deployment

One compiled firmware serves every unit; each unit's calibration lives in
`configs/<unit>.json` and is deployed as a small UF2 written to a reserved
flash block (`TUNING_FLASH_ADDR`, see `firmware/include/TuningConfig.h`).
At boot the firmware validates the block (magic, version, CRC32, value
sanity) and falls back to the safe compiled defaults in
`firmware/include/units/default.h` if it's missing or invalid.

## Deploy a unit

```bash
pio run -e firmware
python3 tools/config/make_config_uf2.py configs/unit1.json unit1-config.uf2
python3 tools/config/merge_uf2.py unit1-full.uf2 \
    .pio/build/firmware/firmware.uf2 unit1-config.uf2
```

Enter the bootloader (hold both buttons 10s → `RPI-RP2` drive) and copy
`unit1-full.uf2` onto it. Config-only updates (tuning tweaks, no firmware
change) can copy just `<unit>-config.uf2` — flashing the config block does
not disturb the firmware, and vice versa: flashing only `firmware.uf2`
preserves the config block.

## Notes

- The JSON keys map 1:1 to the `TuningConfig` fields; `_unit` and
  `_provenance` are documentation and ignored by the packer.
- The packer sanity-checks values (dead zones vs full scale, trim/expo
  ranges, sign values) and refuses to build a bad block; the firmware
  re-checks at boot and falls back to defaults rather than run bad data.
- If the `TuningConfig` layout changes, bump `TUNING_VERSION` in both the
  header and `make_config_uf2.py` — old blocks then fail validation and
  the defaults apply until a new config is deployed.
- The user's LED color choice (right-button-3s cycle) is stored separately
  in EEPROM and survives both firmware and config flashes;
  `led_default_color_index` only sets the out-of-box color.
