# realtime-loop-reviewer
**Checks:** TODO
**Spawn when:** TODO
**Contents:** Patterns to FLAG: Blocking calls in steady-state paths; Timing arithmetic that breaks on millis() wraparound; Starvation between subsystems; State machine hazards · Do NOT flag

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
