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
change) can copy just `<unit>-config.uf2`; the firmware and config occupy
separate flash regions, so either can be reflashed alone.

`pio run -e firmware-generic` builds the same firmware under USB ID
`256F:C6F0`, which 3Dconnexion's Windows driver doesn't claim. Under the
real Compact ID, that driver throttles the reports (see `platformio.ini`).
Merge its `.pio/build/firmware-generic/firmware.uf2` the same way.

`merge_uf2.py` pads each 4 KB sector to 16 UF2 blocks. The RP2040 bootrom
decides which sector to erase from a block's sequence number, not its
address, so a plain concatenation leaves the config sector unerased and the
new block is ANDed onto the old one (`>tuning_status:3`, bad CRC).

## Notes

- The JSON keys map 1:1 to the `TuningConfig` fields; keys starting with
  `_` (`_unit`, `_provenance`, `_notes`) are documentation and ignored by
  the packer.
- The packer applies the firmware's boot-time checks to the float32-rounded
  values, so a block it builds is not rejected at boot. A debug-serial
  build reports the block's status in each telemetry frame:
  `>tuning_status:` is 0 loaded, 1 absent, 2 wrong version, 3 bad CRC,
  4 bad values.
- `python3 -m unittest discover tools/config` runs the packer/merge tests,
  which also check the packer's constants against the firmware headers.
- If the `TuningConfig` layout changes, bump `TUNING_VERSION` in both the
  header and `make_config_uf2.py` — old blocks then fail validation and
  the defaults apply until a new config is deployed.
- The user's LED color choice (right-button-3s cycle) is stored separately
  in EEPROM and survives both firmware and config flashes;
  `led_default_color_index` only sets the out-of-box color.
