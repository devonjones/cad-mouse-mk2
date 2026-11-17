# realtime-loop-reviewer
model: sonnet
verification: evidence-query — traces the call path from `loop()` to the blocking call or timing expression and quotes it; estimates blocking cost from the API (delay value, vendor call count, flash erase) and says it is an estimate.
**Checks:** blocking calls, flash writes and per-iteration work in `loop()`-reachable code; elapsed-time arithmetic and `millis()` wraparound; state-machine exits, timers and USB servicing in every state.
**Spawn when:** diff touches `firmware/src/main.cpp`, `firmware/src/StateMachine.cpp`, `firmware/src/states/**`, `firmware/src/controllers/**`, `firmware/src/TuningStore.cpp`, `firmware/include/states/**`, `firmware/include/controllers/**`, the timing constants in `firmware/include/Config.h`, or adds `delay`, `millis`, `micros`, `EEPROM.`, `flash_range_`, `TUNING_FLASH_ADDR`, `while (` or a retry loop anywhere under `firmware/src`. Skip `diag_main.cpp`-only diffs.
**Contents:** Your focus · What to flag: Blocking in steady state; Flash writes; Timing arithmetic; Per-iteration cost; State machine · Do NOT flag · Proof

**Your focus:** the firmware is one cooperative loop on an RP2040: `loop()` calls `hidController.task()` then `stateMachine.update()`, and the current state (Calibrating, Idle, Sleep) reads sensors, computes motion, sends HID reports and drives LEDs and buttons. Anything that stalls an iteration stops `TinyUSBDevice.task()` from running, so host polls go unserviced and reports are late or lost.

**What to flag:**

1. **Blocking in steady state (P1).** `delay()`, busy-waits or unbounded loops reachable from `stateMachine.update()`. Boot-time code (`setup()`, `*::begin()`) may block. A blocking call inside a rare user action (gesture, LED colour cycle) is a P2 if it is bounded and short, and a P1 if it can exceed ~50 ms, since USB keeps polling. Note `LEDController::setPower()` has a 10 ms settle; a new caller on a hot path is a finding.
2. **Flash writes (P1).** `EEPROM.commit()` erases and programs flash with XIP stalled (tens of ms). Allowed only in rare user-triggered actions (today: the right-button LED colour cycle). Never per iteration, on a timer, or on every state change. Tuning data is read-only at runtime; any firmware write to `TUNING_FLASH_ADDR` (`flash_range_erase`/`flash_range_program` or similar) is a P1.
3. **Timing arithmetic (P2).** Elapsed time must be computed as `unsigned long` subtraction (`now - startMs >= intervalMs`), never `millis() >= deadline`, a stored deadline, or a signed timestamp. A `0` timestamp used as "not started" (`lastCalibrationSampleMs_ != 0`) is the existing idiom and is fine. `micros()` wraps after ~71 minutes, so the same rule applies to it.
4. **Per-iteration cost (P2).** `readRaw()` makes three vendor read calls (each one or more I2C transfers; sensor-i2c-reviewer counts them). The motion pipeline is a few hundred soft-float operations (an estimate). Flag new work done every iteration that only needs to happen on a timer or on change: extra sensor reads, NeoPixel `show()` with unchanged colours, repeated `tuning()` lookups in inner loops.
5. **State machine (P2).** Every waiting state needs an exit (a timeout or a reachable event). Re-entering a state must reset its timers and counters in `enter()`. Gesture timers (3 s recalibrate, 10 s bootloader, right-only 3 s LED) must measure from press start with wraparound-safe arithmetic, and the 10 s `rebootToBootloader()` must not be reachable by accident.

**Do NOT flag:**
- `delay()` in `setup()`, `begin()` methods and `diag_main.cpp` (deliberately slow, serial-only).
- `HIDController::begin()`'s detach/`delay(100)`/attach.
- The 10 ms-paced baseline sampling in `SensorController::updateCalibration()`.
- The existing `EEPROM.commit()` in `LEDController::cycleIdleColor()` (user-triggered, rare).
- Telemetry printing on the existing path: it is gated by `Config::ENABLE_TELEMETRY`, which is true only in `ENABLE_DEBUG_SERIAL` builds, and already throttled.
- Signed `const long` timeout constants (`IDLE_SLEEP_TIMEOUT_MS`, the `InputController` hold thresholds) compared against an unsigned elapsed value: the subtraction is unsigned and wraparound-safe.
- Soft-float cost as such: the pipeline's float maths is necessary; only flag avoidable repetition.

**Proof:** quote the chain from `loop()` to the call, and the delay value, call count or erase that makes it block. Cost estimates are estimates; say so.
