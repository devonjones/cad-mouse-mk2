# calibration-config-reviewer
**Checks:** TODO
**Spawn when:** TODO
**Contents:** What to check: Provenance; Structural consistency across unit headers; Value sanity; EEPROM layout; Units and conventions documented; Tooling round-trip · Do NOT flag

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
