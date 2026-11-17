# clarity-reviewer
**Checks:** TODO
**Spawn when:** TODO
**Contents:** What to check: Read the full file, not just the diff · Code comment rules · Flag issues if · Do NOT flag

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
