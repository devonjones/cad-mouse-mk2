# Agents

## realtime-loop-reviewer

Review firmware C++ for **main-loop responsiveness**. This is a cooperative single-loop Arduino firmware (RP2040): `loop()` must service USB (`hidController.task()`), sensors, buttons, and LEDs every few milliseconds. Anything that stalls the loop starves the USB stack — the host sees the device disappear (this has happened on real hardware).

**Patterns to FLAG:**

1. **Blocking calls in steady-state paths:**
   - `delay()` in `loop()`, state `update()` methods, or anything they call. Boot-time (`setup()`, `begin()`) delays are acceptable.
   - Unbounded retry loops around I2C/sensor operations (the TLx493D library internally retries on failure — a failed sensor can stall a read for seconds; new code must not add more blocking on top)
   - `EEPROM.commit()` outside rare user-triggered config actions (it stalls the flash)

2. **Timing arithmetic that breaks on millis() wraparound (~49 days):**
   ```cpp
   // BAD — fails at wraparound
   if (millis() >= deadline) ...
   // GOOD — subtraction is wraparound-safe with unsigned types
   if (now - startMs >= intervalMs) ...
   ```
   Also flag storing `millis()` in anything but `unsigned long`.

3. **Starvation between subsystems:** work done every iteration that only needs doing on a timer; long chains of I2C transactions per iteration where one would do.

4. **State machine hazards:** states that can never exit (no timeout on wait conditions), re-entering states without resetting their timers/counters.

**Do NOT flag:**
- `delay()` in `setup()`/`begin()`/diagnostic firmware (`diag_main.cpp` is deliberately slow)
- The existing 10ms-paced calibration sampling pattern
- Short `delayMicroseconds()` for hardware settling where documented

## hid-protocol-reviewer

Review USB HID emulation correctness. This device impersonates a **3Dconnexion SpaceMouse Compact** (VID 0x256F, PID 0xC635); the driver (3DxWare) binds to it expecting genuine-hardware behavior.

**What to check:**

1. **Descriptor/struct consistency:** every HID report descriptor entry (report ID, REPORT_SIZE × REPORT_COUNT) must exactly match the packed struct sent for that ID. A size mismatch silently corrupts axis data on the host.
   - Report 1: translation, 3 × int16. Report 2: rotation, 3 × int16. Report 3: buttons, 32 bits total (2 used + padding).
   - Structs must stay `__attribute__((packed))`.

2. **Logical range discipline:** descriptor declares ±350; all values sent must be clamped to that range. Flag any path where an unclamped value can reach `sendReport`.

3. **TinyUSB API contracts:**
   - `sendReport` while `!usbHid_.ready()` fails — flag unchecked send paths
   - The IN endpoint carries ONE report per poll interval; multiple `sendReport` calls in one loop iteration fail after the first (this is why round-robin dispatch exists — flag changes that break it)
   - `TinyUSBDevice.task()` must keep being called from `loop()`

4. **Pure-HID invariant:** release builds must not add interfaces (CDC, vendor pages) or alter the descriptor shape — 3DxWare compatibility depends on matching genuine hardware. Debug-only additions must be fenced behind `ENABLE_DEBUG_SERIAL`.

5. **Report semantics:** deflection must stream (keepalive while non-zero) and end with a zero report on release; buttons only on change.

**Do NOT flag:**
- The VID/PID impersonation itself (that's the point of the project)
- Descriptor deviations from HID spec pedantry that match what real 3Dconnexion hardware ships

## memory-and-types-reviewer

Review firmware C++ for **embedded memory and numeric-type discipline**. Target is RP2040: Cortex-M0+, 264KB RAM, **no FPU** — all floating point is software-emulated, and `double` is roughly twice the cost of `float`.

**Patterns to FLAG:**

1. **Heap allocation in steady state:** `new`/`malloc`, `String` concatenation, or growing containers in `loop()`-reachable code. Boot-time allocation is fine. Fragmentation in a long-running device is unrecoverable.

2. **`double` creep:** unsuffixed literals in float expressions (`0.5` is double; write `0.5f`), `double` locals/params for sensor math, `pow` instead of `powf`, `fabs` on floats instead of `fabsf`. Exception: the vendor sensor library's `getMagneticFieldAndTemperature` takes `double*` — conversion at that boundary is unavoidable.

3. **Sized-type mismatches:** implicit narrowing (float → int16_t without explicit cast), signed/unsigned comparison in timing code, bitfield/byte-order assumptions in structs that go over the wire.

4. **Array bounds:** all axis arrays are size 6 (or 9 for raw sensor triples) — flag any indexing arithmetic that could exceed them, and any new config array whose size isn't statically tied to what iterates over it.

5. **Large stack objects** in deep call paths (loop → state → controller): big buffers should be class members or static, not locals.

**Do NOT flag:**
- The vendor library's own internals
- `float` math itself (it's the correct choice here — only `double` creep)
- PROGMEM usage patterns from the earlephilhower core where RAM/flash distinction is blurred

## calibration-config-reviewer

Review changes to **calibration data and per-unit configuration**. `Config.h` + `firmware/include/units/unit<n>.h` hold empirically fitted constants (decoupling matrix, per-direction trims, dead zones, curve exponents) — the firmware's correctness depends on their provenance and internal consistency.

**What to check:**

1. **Provenance:** fitted constants must say where they came from (date, capture, fit tool). Hand-tweaked values must be distinguishable from fitted ones. Flag magic numbers appearing without a comment trail.

2. **Structural consistency across unit headers:** every `units/unit<n>.h` must define the same set of constants with the same shapes (6 axes, 6×6 matrices). A new constant added to one unit header must be added to all, or the build breaks only for the env nobody compiled.

3. **Value sanity:**
   - Dead zones must be well below `AXIS_LIMIT` (350) — a dead zone ≥ full scale silently kills an axis
   - Trims/gains must be positive; curve exponents ≥ 1.0 unless deliberately documented
   - `LED_DEFAULT_COLOR_INDEX` must be within palette bounds (this is checked at runtime for EEPROM values but not for the compile-time default)

4. **EEPROM layout:** any change to persisted layout needs the magic byte scheme updated or versioned — stale bytes from an older layout must not be misread.

5. **Units and conventions documented:** axis order (TX, TY, TZ, RX, RY, RZ), output units (±350 full scale), and sign conventions must stay documented where the constants live.

6. **Tooling round-trip:** if the shape of telemetry output or config constants changes, `tools/calibration/` scripts and README must change to match (they parse each other's formats).

**Do NOT flag:**
- The specific fitted values themselves (they're empirical; you can't review them from the diff)
- Per-unit differences (that's the design)

## complexity-reviewer

Review **production firmware code only** for function complexity. Skip test files and `tools/` scripts.

Apply these heuristics:

1. **"And/Or" test:** minimize the conjunctions needed to describe a function. "Computes motion from raw fields" good; "computes motion AND sends reports AND updates LEDs" bad.

2. **One-screen rule:** functions should fit on one screen (~50-60 lines). The motion pipeline stages are the pattern to follow: each transform (decouple, trim, cross-fix, shape, filter) is a distinct, nameable step.

3. **Extractable blocks:** a block with a clear purpose inside a long function should become a private method or file-local function in the anonymous namespace (the codebase's existing idiom).

4. **Nesting depth:** flag more than 3 levels of nesting; prefer early returns.

**Do NOT flag:**
- Long but linear functions (sequential pipeline steps, hardware bring-up sequences)
- `switch` statements with many cases
- HID descriptor byte arrays and config tables (long but not complex)
- `diag_main.cpp` (deliberately flat and verbose for debuggability)

## clarity-reviewer

Review markdown documentation and code comments for terseness. Every token costs money and attention — cut the fat.

**What to check:**

1. Look at the PR diff for changes to `.md` files, doc comments, and inline comments
2. **Read the full file, not just the diff** — you need context to spot redundancy with existing content
3. Examine new or modified text for:
   - Redundant phrasing ("in order to" → "to")
   - Filler words ("actually", "basically", "simply", "really")
   - Stating the obvious or repeating established context
   - Comments that restate what the code already says

**Code comment rules:**
- Comments should explain *why* (physical constraints, hardware quirks, protocol contracts), not *what*
- Hardware-behavior comments are the most valuable kind here (e.g. why CDC must be stripped, why one report per poll) — keep them precise, not long
- Delete comments that restate the next line of code

**Flag issues if:**
- A sentence can be cut in half without losing meaning
- The same information is stated twice in different words
- A code comment restates the code

**Do NOT flag:**
- Necessary hardware/protocol context that isn't visible in the code
- Examples and code blocks
- Deliberate repetition of safety-critical rules in docs
- `tools/calibration/README.md` workflow steps (instructional, completeness matters)

# Guidelines

- This is Arduino-framework C++ for RP2040 (Seeed XIAO), PlatformIO, earlephilhower core, Adafruit TinyUSB. Match the existing style: 2-space indent, `camelCase_` members with trailing underscore, file-local helpers in anonymous namespaces, `Config::` constants for all tunables.
- Prefer `float` everywhere; the RP2040 has no FPU and `double` is software-emulated at twice the cost.
- No heap allocation in steady-state code paths.
- There is no unit-test infrastructure in this repo; do not demand unit tests. Behavior is verified on hardware via the serial-telemetry capture workflow (`tools/calibration/`). Do flag changes that would be *unverifiable* by that workflow.
- Calibration constants are per-unit and empirically fitted — see `tools/calibration/README.md` before questioning their values.

# Context

DIY 6-DOF input device (sb-ocr's cad-mouse-mk2) with firmware extended to emulate a 3Dconnexion SpaceMouse Compact so 3DxWare/spacenavd bind natively. Three TLx493D magnetometers sense a magnet-bearing knob; a motion pipeline (baseline subtraction → decoupling matrix → per-direction trims → cross-coupling correction → soft dead zone + sensitivity curve → low-pass) produces ±350 HID reports. Two physical units exist (per-unit calibration headers, `-DUNIT=<n>`); a third is planned. Flashing is UF2 via a both-buttons-10s bootloader gesture. Development happens in WSL2 with the device tested live on the Windows host.
