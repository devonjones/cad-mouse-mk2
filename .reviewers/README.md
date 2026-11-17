# Guidelines

- This is Arduino-framework C++ for RP2040 (Seeed XIAO), PlatformIO, earlephilhower core, Adafruit TinyUSB. Match the existing style: 2-space indent, `camelCase_` members with trailing underscore, file-local helpers in anonymous namespaces, `Config::` constants for all tunables.
- Prefer `float` everywhere; the RP2040 has no FPU and `double` is software-emulated at twice the cost.
- No heap allocation in steady-state code paths.
- There is no unit-test infrastructure in this repo; do not demand unit tests. Behavior is verified on hardware via the serial-telemetry capture workflow (`tools/calibration/`). Do flag changes that would be *unverifiable* by that workflow.
- Calibration constants are per-unit and empirically fitted — see `tools/calibration/README.md` before questioning their values.

# Context

DIY 6-DOF input device (sb-ocr's cad-mouse-mk2) with firmware extended to emulate a 3Dconnexion SpaceMouse Compact so 3DxWare/spacenavd bind natively. Three TLx493D magnetometers sense a magnet-bearing knob; a motion pipeline (baseline subtraction → decoupling matrix → per-direction trims → cross-coupling correction → soft dead zone + sensitivity curve → low-pass) produces ±350 HID reports. Two physical units exist (per-unit calibration headers, `-DUNIT=<n>`); a third is planned. Flashing is UF2 via a both-buttons-10s bootloader gesture. Development happens in WSL2 with the device tested live on the Windows host.
