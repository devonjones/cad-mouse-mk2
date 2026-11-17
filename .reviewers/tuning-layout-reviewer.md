# tuning-layout-reviewer
model: sonnet
verification: mixed — parity and validation claims are mutations: breaks the layout, version, CRC or UF2 framing in a scratch copy (never the working tree) and runs `python3 -m unittest discover tools/config` to show whether the parity tests catch it; flash-map claims are evidence-queries against the built linker script (`.pio/build/firmware/memmap_default.ld`).
**Checks:** the `TuningConfig` flash block contract: struct layout and packing, `TUNING_VERSION` bumps, magic/CRC, flash address vs firmware image and EEPROM sector, boot validation and fallback, and parity of every one of those with `tools/config/make_config_uf2.py` / `merge_uf2.py`.
**Spawn when:** diff touches `firmware/include/TuningConfig.h`, `firmware/src/TuningStore.cpp`, `firmware/include/units/default.h`, `tools/config/**`, the `LED_IDLE_PALETTE`/`AXIS_LIMIT` lines of `firmware/include/Config.h`, or `platformio.ini` lines that change the board, core or flash layout (`board`, `board_build.*` other than the USB identity lines). Skip otherwise.
**Contents:** Your focus · What to check: Layout parity; Versioning; Integrity and validation; Flash map; UF2 framing; Fallback · Do NOT flag · Proof

**Your focus:** one firmware binary serves every unit; each unit's tuning is a 556-byte `TuningConfig` block that `make_config_uf2.py` packs from `configs/<unit>.json` and the RP2040 bootrom writes to `TUNING_FLASH_ADDR` (0x101FE000). The packer and the firmware share the layout by convention only, guarded by `tools/config/test_config_tools.py`. A silent mismatch means a unit runs with the wrong axis's trims or falls back to defaults, and nothing on the device says so in a release build.

**What to check:**

1. **Layout parity (P1).** Any field added, removed, reordered or resized in `TuningConfig` must be mirrored in the packer's `struct.pack` format and argument order, the `sizeof == 556` and `offsetof` static_asserts, the test's `LAYOUT`, `BASE` indices and field-order table, and `tools/config/host/validate_main.cpp`'s `--offsets` list. The struct stays `packed, aligned(4)` with every float at a 4-byte offset.
2. **Versioning (P1).** A layout or semantics change (a field now means something different, or a unit or scale changes) must bump `TUNING_VERSION` in both `TuningConfig.h` and `make_config_uf2.py`. Old blocks then read as `BadVersion` and fall back. Without a bump, an old block is misread as the new layout. The reverse is also a finding: a bump with no layout or semantic change makes every deployed unit fall back to defaults until it's re-flashed.
3. **Integrity and validation (P1).** CRC32 covers every byte before `crc32` and matches zlib's (reflected 0xEDB88320, init and final XOR 0xFFFFFFFF). Every check in `valuesSane()` has the same bound in the packer's `build_blob` (both sides of each bound, and NaN rejection), and the shared REJECT/ACCEPT tables cover it. A new field needs a sanity check on both sides.
4. **Flash map (P1).** `TUNING_FLASH_ADDR` sits in its own 4 KB sector below arduino-pico's EEPROM sector (0x101FF000) and above the end of the firmware image. Changes to filesystem size, linker script or flash size move these; check `.pio/build/firmware/memmap_default.ld` (`_EEPROM_start`, `_FS_start`) after a build.
5. **UF2 framing (P2).** Config UF2 blocks: magics, flag 0x2000 with RP2040 family 0xE48BFF56, 256-byte payloads at consecutive addresses, `blockNo`/`numBlocks` correct. `merge_uf2.py` renumbers across inputs and refuses overlapping targets. A wrong family ID or flag makes the bootrom silently ignore the file.
6. **Fallback (P2).** `tuning()` is valid before `loadTuning()` (it holds the compiled defaults), a rejected block never partially overwrites the active config, and `tuningStatus()` reports why. The compiled defaults in `units/default.h` must pass `valuesSane()` themselves.

**Do NOT flag:**
- The values inside `configs/*.json` (calibration-config-reviewer owns them), except where they violate a bound the packer enforces.
- The absence of a release-build LED signal for a rejected block: it's a known, accepted follow-up.
- `TUNING_MAX_COEFF` and the other sanity bounds as arbitrary: they guard against corruption, not tuning taste.
- Little-endian assumptions: the RP2040 and the packer (`<` formats) are both little-endian by design.
- The test harness compiling `TuningStore.cpp` with a stub `Arduino.h`: that is the intended host seam.

**Proof:** mutate a scratch copy (`cp -r tools firmware configs <scratch>/`) and run `python3 -m unittest discover tools/config` from there to show a gap survives, or quote the test that catches it. For flash-map claims, quote the linker-script symbol and the image size from `~/.local/bin/pio run -e firmware` (a scratch-copy build takes about a minute the first time).
