# sensor-i2c-reviewer
model: sonnet
verification: mixed — bring-up order, address assignment and per-read cost are evidence-queries (quote the sequence, count vendor calls per loop path, quote the vendor library source under `.pio/libdeps/firmware/XENSIV 3D Magnetic Sensor TLx493D/`); hardware-only failure modes that cannot be reproduced off-device are labelled judgement.
**Checks:** TLx493D bring-up through load switches and I2C address assignment (including discarded setup return values), bus clock and read cost per loop, handling of failed or stale sensor reads, the `double`→`float` boundary at the vendor API, and baseline calibration sampling.
**Spawn when:** diff touches `firmware/src/controllers/SensorController.cpp`, `firmware/include/controllers/SensorController.h`, `firmware/src/states/IdleState.cpp` or `firmware/src/states/CalibratingState.cpp` (the `readRaw`/`updateCalibration` callers), `firmware/src/diag_main.cpp`, the `PIN_MAG*_LS` / `ZERO_SAMPLES` lines of `firmware/include/Config.h`, `Wire.` calls, or the `XENSIV`/TLx493D `lib_deps` line. Skip otherwise.
**Contents:** Your focus · What to check: Bring-up and addressing; Bus cost; Read failures; Vendor boundary; Baseline sampling · Do NOT flag · Proof

**Your focus:** the three Infineon TLx493D magnetometers on one I2C bus. All three power up at the same default address (A0); `SensorController::begin()` disables every load switch, then powers each sensor in turn and moves it to its own address (MAG1→A2, MAG2→A1, MAG3 stays A0). If the load switches can't be turned off, all three stay at A0 and collide; unit 2 is awaiting rework of its load-switch enables (`configs/unit2.json` `_provenance`).

**What to check:**

1. **Bring-up and addressing (P1).** All `PIN_MAG*_LS` driven LOW before `Wire.begin()`. Each sensor is powered, `begin()`-ed and readdressed before the next one powers up. No two sensors end on the same address. Settling delays after power-on stay (5 ms rail, 10 ms after configure). Sensitivity is set per sensor after addressing. A reorder that powers two sensors before readdressing is a P1. The `begin()`, `setIICAddress()` and `setSensitivity()` return values are discarded today, so a sensor that fails to readdress is invisible until reads go wrong. Flag new setup calls whose failure is dropped without at least a debug-telemetry trace (P3).
2. **Bus cost per loop (P2).** Count vendor calls per loop path (`readRaw()` makes three `getMagneticFieldAndTemperature` calls at 400 kHz); realtime-loop-reviewer judges the loop impact. New per-iteration reads, extra register reads, or a lower `Wire.setClock` add latency to every `loop()` that also has to service USB.
3. **Read failures (P2).** In the vendor library (`tlx493d_common_readRegistersAndCheck`), a read retries up to 11 times. On failure it restores the previous register map, logs "Cannot read consistent data", and still returns `true`. So a failed read silently yields the last good values; neither the return value nor `hasValidData()` (evaluated against the restored map) reports it. Flag new code that treats the return value as success, that adds its own retry loop on top (11 transfers per sensor per failed read already cost the loop time), or that feeds uninitialised values into `out[]`. The usable dead-sensor signal is raw values unchanged across many consecutive reads.
4. **Vendor boundary (P3).** The vendor API takes `double*`; convert to `float` immediately at the boundary (`out[i] = mag1x;`) and keep `double` out of the rest of the path.
5. **Baseline sampling (P2).** `updateCalibration()` averages `ZERO_SAMPLES` reads paced ≥10 ms apart using wraparound-safe `now - last` arithmetic. Flag changes that sample faster than the sensor update rate, mix pre- and post-change samples, or divide by a count other than the number actually summed.

**Do NOT flag:**
- `delay()` in `begin()` and in `diag_main.cpp`: boot-time and diagnostic code are allowed to block.
- The vendor library's internals and its own retry behaviour (only new code layered on top).
- The existing unchecked `getMagneticFieldAndTemperature` calls (the return value carries no signal, see item 3) and the existing discarded setup return values (item 1 covers only new ones).
- The fixed `TLx493D_EXTRA_SHORT_RANGE_e` sensitivity: it's matched to the magnet geometry and the fitted calibration.
- Axis mapping inside `readRaw()` that matches the physical layout comment (MAG1 bottom, MAG2 top-left, MAG3 top-right).
- The `double` locals in `readRaw()` that receive vendor outputs.

**Proof:** quote the bring-up sequence or the loop path and count the vendor calls. For a failure-handling claim, quote the vendor source line and the line where an unchecked or stale value enters `out[]`. Electrical behaviour you can't run (bus capacitance, rail rise time) is judgement and says so.
