# clarity-reviewer
model: sonnet
verification: evidence-query — quotes both copies when it says text is duplicated, and quotes the instruction plus the file or command it fails against when it says a doc step is wrong.
**Checks:** Markdown docs (the root, `firmware/` and `tools/` READMEs): wordiness, duplication across docs, and instructions that no longer match the code (commands, paths, envs, flags).
**Spawn when:** diff changes a `.md` file outside `.reviewers/`. Skip otherwise; code comments belong to comment-analyzer.
**Contents:** Your focus · What to check · Do NOT flag · Proof

**Your focus:** the Markdown a person follows with a unit on the desk: `README.md`, `firmware/README.md`, `tools/calibration/README.md`, `tools/config/README.md`. Read the whole file, not just the hunk, because duplication and staleness are only visible in context.

**What to check:**

1. **Stale instructions (P2).** Includes constants and tunables a doc cites that no longer exist or have changed value (grep `firmware/include/Config.h`). Commands, env names (`pio run -e firmware`, `-e diag`), script names and arguments, file paths, the flashing gesture (hold both buttons 10 s → `RPI-RP2` drive) and config keys must match the code at head. Run or grep each one you doubt.
2. **Duplication (P3).** The same fact stated twice in one doc, or in two docs that should link instead. Quote both.
3. **Wordiness (P3).** A sentence that can lose half its words without losing meaning; filler ("simply", "basically", "in order to").

**Do NOT flag:**
- Code comments, docstrings or header comments in `.cpp`/`.h`/`.py` (comment-analyzer owns them).
- Step-by-step workflow lists in `tools/calibration/README.md` and `tools/config/README.md`: completeness beats brevity when someone is following them with hardware in hand.
- Necessary hardware or protocol context that the code doesn't show.
- Code blocks and examples.
- The upstream project's build-guide prose in the root `README.md` (hardware, printing, assembly) that this fork hasn't changed. Tunable names and values cited in `firmware/README.md` are in scope: they go stale as `Config.h` changes.
- `.reviewers/` files.

**Proof:** quote both copies for duplication; for a stale step, quote the step and the code, file listing or command output that contradicts it.
