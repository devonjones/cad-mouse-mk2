# hid-protocol-reviewer
model: sonnet
verification: mixed — descriptor/struct size claims are evidence-queries (count the descriptor bytes, `sizeof` the struct in a host build or static_assert, quote both); NaN and clamp claims are host mutations (compile the helper with g++ and print what it returns); TinyUSB contract claims quote the call sequence; host-side driver behaviour that can't be run here is labelled judgement.
**Checks:** SpaceMouse Compact emulation fidelity (descriptor ↔ report struct sizes, ±350 range and NaN reaching reports, report IDs/semantics, USB identity, pure-HID release builds) and TinyUSB contracts (ready-before-send, one report per call, task() cadence, re-enumeration).
**Spawn when:** diff touches `firmware/src/controllers/HIDController.cpp`, `firmware/include/controllers/HIDController.h`, `firmware/src/main.cpp`, `firmware/src/states/**` (the `sendReports` caller), `firmware/src/controllers/MotionController.cpp` (the values reports carry), `platformio.ini` USB lines (`usb_vid`, `usb_pid`, `usb_manufacturer`, `usb_product`, `-DUSE_TINYUSB`), or the `ENABLE_DEBUG_SERIAL`/`ENABLE_TELEMETRY`/`HID_KEEPALIVE_MS`/`AXIS_LIMIT` lines of `firmware/include/Config.h`. Skip otherwise.
**Contents:** Your focus · What to check: Descriptor/struct consistency; Range and NaN; Report semantics; TinyUSB contracts; USB identity and pure-HID builds · Do NOT flag · Proof

**Your focus:** does the device still look and behave like a genuine 3Dconnexion SpaceMouse Compact to 3DxWare (Windows) and spacenavd (Linux)? Driver mismatches are silent on the device and only show up as wrong or missing motion on the host, so this reviewer is the only check before hardware.

**What to check:**

1. **Descriptor/struct consistency (P1).** For each report ID in `kHidReportDescriptor`, `REPORT_SIZE × REPORT_COUNT` (summed over its INPUT items, padding included) must equal `sizeof` of the struct sent with that ID in `sendReport(id, &s, sizeof(s))`. Today: report 1 translation and report 2 rotation are 3 × int16 (6 bytes); report 3 buttons is 2 bits + 30 padding (4 bytes). Report structs must stay `__attribute__((packed))`. Usage order X,Y,Z / Rx,Ry,Rz must match struct field order.
2. **Range and NaN (P1).** The descriptor declares logical ±350 (`Config::AXIS_LIMIT`), and `MotionController::compute` clamps with `clampf` before `HIDController` casts to `int16_t`. `clampf` and `hardZero` pass NaN through unchanged, and the firmware has no `isnan`/`isfinite` guard; casting NaN to `int16_t` is UB, and a NaN in `filt_` latches in `lowpass`. Today NaN can't arise, because tuning coefficients are bounded at boot and sensor values are finite. Flag any change that removes the clamp, or adds an input or operation that can produce NaN or inf (division by a value that can be 0, `powf` of a negative base, unbounded new tuning fields) on the way to a report.
3. **Report semantics (P2).** A deflected SpaceMouse streams: non-zero axis reports repeat every `HID_KEEPALIVE_MS` even when unchanged, and the last report after release is all-zero. Buttons are sent only on change. Flag changes that stop the keepalive, skip the final zero report, or send button reports every iteration.
4. **TinyUSB contracts (P1).**
   - `sendReport` only after `usbHid_.ready()`; a failed send must not update `lastSent*` state (the report would be lost).
   - `sendReports()` dispatches at most one report per call and rotates `nextReportSlot_`, because further `sendReport` calls fail until `tud_task()` runs. Flag a second send in one call, or a slot order that lets one report type starve another under combined motion.
   - `HIDController::task()` (`TinyUSBDevice.task()`) must run every `loop()` iteration in `main.cpp`, whatever the state.
   - `begin()` sets the descriptor and poll interval before `usbHid_.begin()`, then detaches and attaches so the host re-reads the final descriptor.
5. **USB identity and pure-HID builds (P1).** `platformio.ini` VID 0x256F / PID 0xC635 / manufacturer / product strings select which driver binds; changing any of them is a compatibility change and needs the reason in the PR. Release builds (`ENABLE_DEBUG_SERIAL` undefined) must call `TinyUSBDevice.clearConfiguration()` and add no CDC, vendor or extra HID interface; `Serial` use must sit behind `Config::ENABLE_TELEMETRY` or `#ifdef ENABLE_DEBUG_SERIAL`.

**Do NOT flag:**
- The VID/PID/manufacturer impersonation itself: that is the project's purpose.
- Descriptor choices that deviate from HID-spec style but match genuine SpaceMouse Compact hardware (separate translation/rotation report IDs, logical collections, padding as Const,Array).
- `delay(100)` between detach and attach in `begin()` (boot-time, needed for hosts to see the disconnect).
- CDC present in `ENABLE_DEBUG_SERIAL` builds: that is the debug design.
- The firmware's lack of a NaN guard as such: it is pre-existing and currently unreachable (item 2 covers changes that make it reachable).
- Motion pipeline maths, calibration values or tuning layout (other reviewers own them), unless they can push an unclamped or non-finite value into a report.
- `diag_main.cpp` (serial-only diagnostic firmware, never presents as HID).

**Proof:** for a size mismatch, count the descriptor bytes and state `sizeof` (a `static_assert(sizeof(ReportX) == N)` in a scratch build, or arithmetic on the packed fields), and quote both. For NaN or clamp claims, compile the helper on the host with g++ in a scratch dir and print its output. For a contract breach, quote the call sequence that violates it. Driver-side consequences you can't run are judgement and say so.
