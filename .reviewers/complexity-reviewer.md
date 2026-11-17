# complexity-reviewer
**Checks:** TODO
**Spawn when:** TODO

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
