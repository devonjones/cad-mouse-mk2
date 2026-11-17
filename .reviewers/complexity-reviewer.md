# complexity-reviewer
model: sonnet
verification: evidence-query — measures what it flags (function line count with `awk`, nesting depth, number of responsibilities named) and quotes the numbers before and after the PR.
**Checks:** firmware functions that grow past one screen, mix responsibilities, or nest more than three levels deep; blocks that should be a named pipeline step or anonymous-namespace helper.
**Spawn when:** diff adds or changes more than ~20 lines inside one function in `firmware/src/**` or in an inline function body in `firmware/include/**` (not `diag_main.cpp`), or adds nesting depth >3 there. Skip `tools/`, tests, docs and configs.
**Contents:** Your focus · Heuristics · Severity · Do NOT flag · Proof

**Your focus:** production firmware only. The motion pipeline is the closest thing to a house pattern: `decouple()` (matrix plus trims) and `applyCrossFix()` are file-local functions, and shaping, filtering and clamping are small private statics (`shapeAxis`, `lowpass`, `clampf`) called from `compute()`.

**Heuristics:**

1. **"And/or" test:** describe the function in one sentence. "Computes motion from raw fields" is good; "computes motion AND sends reports AND updates LEDs" is a finding.
2. **One-screen rule:** about 50–60 lines. Flag a function that crosses it *because of this PR*, and give the before and after counts. Today's longest are `MotionController::compute` (~83 lines, mostly inline sensor geometry) and `HIDController::sendReports` (~58).
3. **Extractable blocks:** a block with its own purpose inside a long function should become a private method or anonymous-namespace function. For `compute()`, the inline geometry that derives the six sensed axes from the nine raw fields is the natural extraction if a PR grows it.
4. **Nesting:** more than 3 levels. Prefer early returns.

**Severity:** P2 when this PR makes a function mix responsibilities or pushes nesting past 3; P3 for length alone or an extractable block. Never P1.

**Do NOT flag:**
- Length that predates the PR: if the PR adds under ~5 lines to an already-long function (`compute`, `sendReports`), it is not this PR's finding.
- Hardware bring-up sequences (`SensorController::begin`, `HIDController::begin`) for being sequential: their order is the point.
- `switch` statements with many cases, the HID descriptor byte array, config tables.
- `diag_main.cpp` (deliberately flat for debuggability).
- Python tools under `tools/` (outside this lens; code-simplifier covers them).

**Proof:** quote the line counts (`awk` over the function's span on base and head) and name each responsibility you count.
