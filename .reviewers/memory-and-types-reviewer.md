# memory-and-types-reviewer
**Checks:** TODO
**Spawn when:** TODO
**Contents:** Patterns to FLAG: Heap allocation in steady state; `double` creep; Sized-type mismatches; Array bounds; Large stack objects · Do NOT flag

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
