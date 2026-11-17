# Configuration notes

Gemini Code Assist and Cursor Bugbot are both off (`config.json` `bots`). Cursor has never reviewed this repo, and Gemini has not reviewed any PR since #1. The agent pack is the whole review, so the loop must not trigger or wait for either bot.

The repo has no CI. Before posting a round's fixes, the orchestrator runs `~/.local/bin/pio run -e firmware`, `pio run -e diag`, the debug build (`PLATFORMIO_BUILD_FLAGS=-DENABLE_DEBUG_SERIAL`), and `python3 -m unittest discover tools/config`.

# Roster

| Reviewer | Lens |
|---|---|
| hid-protocol-reviewer | SpaceMouse Compact emulation fidelity, TinyUSB contracts, pure-HID builds |
| sensor-i2c-reviewer | TLx493D bring-up and addressing, I2C cost, read failures |
| tuning-layout-reviewer | `TuningConfig` flash block: layout, version, CRC, UF2, packer parity |
| calibration-config-reviewer | `configs/*.json` provenance and invariants, capture/fit tooling |
| realtime-loop-reviewer | blocking, flash writes, timing arithmetic, state machine |
| memory-and-types-reviewer | heap, `double` creep, narrowing, alignment, bounds, size |
| complexity-reviewer | firmware function size and responsibilities |
| clarity-reviewer | Markdown docs only |
| code-reviewer | broad bugs plus the house rules it carries (there is no CLAUDE.md) |
| pr-test-analyzer | mutation-proven coverage gaps in `tools/config` and the host-compiled firmware |
| silent-failure-hunter | swallowed errors and fallbacks |
| comment-analyzer | code comments and docstrings |
| code-simplifier | redundant code in firmware and tools |

`type-design-analyzer` is not carried over. Its `spawn_when` names no C/C++ extensions, and `tuning-layout-reviewer` covers the one struct whose design matters.

# Guidelines

- **Domain ownership.** Comment findings belong to comment-analyzer and doc findings to clarity-reviewer. TuningConfig layout belongs to tuning-layout-reviewer and calibration values to calibration-config-reviewer. Other reviewers don't file in those lanes unless the finding is a symptom of their own.
- **Hardware isn't available to reviewers.** Findings about on-device behaviour (driver binding, sensor electrical faults, feel) are judgement unless shown in a host test or from the code path. When a change can only be verified on hardware, the orchestrator asks Devon to flash it and prints the full step list (unplug → hold both buttons 10 s → copy the UF2 to `RPI-RP2` → capture with `tools/calibration/capture-serial.ps1` on the device's Windows COM port, COM3 on artemis).
- **Settled decisions (don't re-litigate):**
  - Falling back to the compiled defaults on an invalid config block is the design, and a release-build LED signal for it is a tracked follow-up.
  - `curve_expo` 0.5–5 and `TUNING_MAX_COEFF` are sanity bounds, not tuning conventions.
  - The image-below-`0x101FE000` constraint has no build-time check. tuning-layout-reviewer verifies it when a diff could grow the image or move flash regions (today about 127 KB of 2 MB).

# Context

DIY 6-DOF input device (sb-ocr's cad-mouse-mk2). This fork's firmware emulates a 3Dconnexion SpaceMouse Compact (VID 0x256F, PID 0xC635) so that 3DxWare and spacenavd bind to it natively.

- **Sensing:** three TLx493D magnetometers on one I2C bus sense a magnet-bearing knob.
- **Motion pipeline:** baseline subtraction → decoupling matrix → per-direction trims → cross-coupling correction → soft dead zone and sensitivity curve → low-pass. Output is ±350 HID reports.
- **Per-unit tuning:** one firmware build serves every unit. Each unit's tuning lives in `configs/<unit>.json`. `tools/config/make_config_uf2.py` packs it into a 556-byte `TuningConfig` block at flash 0x101FE000, and it is flashed alongside or separately from the firmware UF2.
- **Units:** unit 1 (purple) is calibrated. Unit 2 (black) is awaiting solder rework. A third unit is planned.
- **Development:** done in WSL2. The device is tested live on the Windows host.
