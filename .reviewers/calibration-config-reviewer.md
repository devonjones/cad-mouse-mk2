# calibration-config-reviewer
model: sonnet
verification: mixed — provenance and cross-file consistency are evidence-queries (grep the JSON, the fit scripts and the READMEs, and quote them); tool round-trip claims are mutations (run the emitting code on synthetic input in a scratch dir and show the packer accepting or rejecting the result); whether a hand override is wise is judgement.
**Checks:** per-unit tuning data in `configs/*.json` (provenance, hand-override rationale, the trim/cross rescaling invariant, conventions) and the calibration tooling round-trip (telemetry capture → `analyze.py` and fit scripts → JSON fragments → packer keys), including the README workflows.
**Spawn when:** diff touches `configs/**`, `tools/calibration/**`, `firmware/include/units/default.h`, the `FIELDS` tuple or key handling in `tools/config/make_config_uf2.py`, `tools/config/README.md`, `firmware/src/controllers/TelemetryController.cpp`, or the `ENABLE_TELEMETRY`/`SMOOTH_TAU_S` lines of `firmware/include/Config.h`. Skip otherwise.
**Contents:** Your focus · What to check: Provenance; Rescaling invariant; Conventions; Tool round-trip; Workflow docs · Do NOT flag · Proof

**Your focus:** each physical unit's calibration is fitted from guided serial captures (`tools/calibration/`), then sometimes hand-tuned on hardware, and lives in `configs/<unit>.json`. Fitted numbers can't be reviewed from a diff, but their provenance, internal consistency and the tooling that produces them can.

**What to check:**

1. **Provenance (P2).** A changed fitted value in `configs/<unit>.json` needs the fit or capture that produced it in `_provenance` or `_notes` (date, capture, script). A hand override must say what felt wrong and by how much. Values that change with no trail are a finding, and so are notes whose numbers don't match the values they describe.
2. **Rescaling invariant (P1).** CROSS corrections are fitted against the trims at capture time. When a source axis's trim changes after fitting, that axis's `cross_pos`/`cross_neg` *source row* must be rescaled by `old_trim / new_trim`, or the correction changes in absolute terms. Check every trim change in the diff for a matching row rescale or a stated exception (unit1's RX→TY terms are a documented deliberate exception). Also check `fit_crossfix.py`'s `OLD_*`/`NEW_*` trims match the unit being fitted.
3. **Conventions (P2).** Axis order TX, TY, TZ, RX, RY, RZ; `sign_axis` ±1 per axis; output scale ±350 (`AXIS_LIMIT`); `cross_*[source][target]`. A new unit's JSON should start from the bootstrap values (`configs/unit2.json` mirrors `units/default.h` except its LED colour, with a diagonal matching `fit_matrix.py`'s `GAINS_OLD`). A change to `default.h` or to the packer's `FIELDS` must be reflected in every `configs/*.json`.
4. **Tool round-trip (P2).** `fit_matrix.py` (`emit_json`) and `fit_crossfix.py` (`jmat` plus module-level prints) print JSON fragments for `configs/<unit>.json`; they must stay valid JSON with the exact keys the packer's `FIELDS` expects. If the telemetry line format changes (`TelemetryController`'s `>name:value` lines, the `#t=` stamps `capture-serial.ps1` adds), `analyze.py` and the fit scripts' parsers must change with it.
5. **Workflow docs (P3).** `tools/calibration/README.md` and `tools/config/README.md` steps must still work end to end: script names, arguments, the packer and merge commands, and the flashing sequence (clarity-reviewer covers wording and duplication).

**Do NOT flag:**
- The specific fitted numbers, or differences between units: that's the design.
- unit2's uncalibrated bootstrap values: unit 2 is awaiting hardware rework.
- Hand overrides that have a written rationale, even if they diverge from the fit (the hand overrides the capture when they conflict).
- Hard-coded unit-1 trims in `fit_crossfix.py` that carry the "update before fitting another unit" warning, and hand-edited phase windows in the fit scripts (they are per-capture by design).
- The COM port in capture examples: it is per host.
- Layout, CRC and packer validation bounds (tuning-layout-reviewer owns them).

**Proof:** quote the JSON lines and the provenance or note that is missing or wrong. For the rescaling invariant, show the old trim, new trim, old row and new row, and the arithmetic. For a round-trip break: the fit scripts run their analysis at import and need numpy (`~/.venvs/cad-mouse/bin/python`). For `fit_matrix.py`, extract `jarr`/`emit_json` with `ast` and call them on a synthetic matrix. For `fit_crossfix.py`, extract `jmat` and replay its two module-level `print` lines. Wrap the printed fragment in `{}` and show `json.loads` (or the packer) accepting or rejecting it.
