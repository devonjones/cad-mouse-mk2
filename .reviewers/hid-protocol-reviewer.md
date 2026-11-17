# hid-protocol-reviewer
model: sonnet
verification: mixed — descriptor/struct size claims are evidence-queries (count the descriptor bytes, `sizeof` the struct in a host build or static_assert, quote both); TinyUSB contract claims quote the call site and the Adafruit/TinyUSB API that constrains it; host-side driver behaviour that can't be run here is labelled judgement.
**Checks:** SpaceMouse Compact emulation fidelity (descriptor ↔ report struct sizes, ±350 clamping, report IDs/semantics, USB identity, pure-HID release builds) and TinyUSB contracts (ready-before-send, one report per poll, task() cadence, re-enumeration).
**Spawn when:** diff touches `firmware/src/controllers/HIDController.cpp`, `firmware/include/controllers/HIDController.h`, `platformio.ini` USB lines (`usb_vid`, `usb_pid`, `usb_manufacturer`, `usb_product`, `-DUSE_TINYUSB`), the `ENABLE_DEBUG_SERIAL`/`ENABLE_TELEMETRY` lines of `firmware/include/Config.h`, `Serial` use outside `ENABLE_TELEMETRY`/`ENABLE_DEBUG_SERIAL` fences, or code whose output reaches `sendReports()` (MotionController clamping, button bits). Skip otherwise.
**Contents:** Your focus · What to check: Descriptor/struct consistency; Range discipline; Report semantics; TinyUSB contracts; USB identity and pure-HID builds · Do NOT flag · Proof

**Your focus:** does the device still look and behave like a genuine 3Dconnexion SpaceMouse Compact to 3DxWare (Windows) and spacenavd (Linux)? Driver mismatches are silent on the device and only show up as wrong or missing motion on the host, so this reviewer is the only check before hardware.

**What to check:**

1. **Descriptor/struct consistency (P1).** For each report ID in `kHidReportDescriptor`, `REPORT_SIZE × REPORT_COUNT` (summed over its INPUT items, padding included) must equal `sizeof` of the struct sent with that ID in `sendReport(id, &s, sizeof(s))`. Today: report 1 translation and report 2 rotation are 3 × int16 (6 bytes); report 3 buttons is 2 bits + 30 padding (4 bytes). Report structs must stay `__attribute__((packed))`. A new report ID, usage or reordering must keep usage order X,Y,Z / Rx,Ry,Rz matching struct field order.
2. **Range discipline (P1).** The descriptor declares logical ±350 (`Config::AXIS_LIMIT`). Every value cast to `int16_t` for a report must have passed `clampf(..., -AXIS_LIMIT, AXIS_LIMIT)`; NaN must not reach the cast (casting NaN to int16_t is UB). Trace any new path from tuning/motion data to `makeTranslationReport`/`makeRotationReport`.
3. **Report semantics (P2).** A deflected SpaceMouse streams: non-zero axis reports repeat every `HID_KEEPALIVE_MS` even when unchanged, and the last report after release is all-zero. Buttons are sent only on change. Flag changes that stop the keepalive, skip the final zero report, or send button reports every iteration.
4. **TinyUSB contracts (P1).**
   - `sendReport` only after `usbHid_.ready()`; a failed send must not update `lastSent*` state (the report would be lost).
   - The IN endpoint takes one report per `tud_task()`; `sendReports()` dispatches at most one report per call and rotates `nextReportSlot_` so translation, rotation and buttons don't starve each other. Flag a second `sendReport` in the same call, or a slot order that lets one report type starve another under combined motion.
   - `HIDController::task()` (`TinyUSBDevice.task()`) must run every `loop()` iteration, including in every state (Calibrating, Idle, Sleep).
   - `begin()` must set the descriptor and poll interval before `usbHid_.begin()`, then detach/attach so the host re-reads the final descriptor.
5. **USB identity and pure-HID builds (P1).** `platformio.ini` VID 0x256F / PID 0xC635 / manufacturer / product strings select which driver binds; changing any of them is a compatibility change and needs the reason in the PR. Release builds (`ENABLE_DEBUG_SERIAL` undefined) must call `TinyUSBDevice.clearConfiguration()` and add no CDC, vendor or extra HID interface; any `Serial` use must sit behind `Config::ENABLE_TELEMETRY` or `#ifdef ENABLE_DEBUG_SERIAL`.

**Do NOT flag:**
- The VID/PID/manufacturer impersonation itself: that is the project's purpose.
- Descriptor choices that deviate from HID-spec style but match genuine SpaceMouse Compact hardware (separate translation/rotation report IDs, logical collections, padding as Const,Array).
- `delay(100)` between detach and attach in `begin()` (boot-time, needed for hosts to see the disconnect).
- CDC present in `ENABLE_DEBUG_SERIAL` builds: that is the debug design.
- Motion pipeline maths, calibration values or tuning layout (other reviewers own them), unless they can push an unclamped or NaN value into a report.
- `diag_main.cpp` (serial-only diagnostic firmware, never presents as HID).

**Proof:** for a size mismatch, count the descriptor bytes and state `sizeof` (a `static_assert(sizeof(ReportX) == N)` in a scratch build, or arithmetic on the packed fields), and quote both. For a contract breach, quote the call sequence that violates it. Driver-side consequences you can't run are judgement and say so.
