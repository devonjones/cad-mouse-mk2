import re
import sys

if len(sys.argv) < 2:
    print(f"Usage: python3 {sys.argv[0]} <capture_file>")
    sys.exit(1)
import numpy as np

AXES = ["X", "Y", "Z", "Rx", "Ry", "Rz"]
# Gains active during capture3 (symmetric, pre-per-direction era).
GAINS_OLD = np.array([48.0, 48.0, 42.0, 18.0, 30.0, 20.0])
TARGET = 350.0

def parse(path):
    samples = []
    t = 0.0
    cur = {}
    for line in open(path):
        line = line.strip()
        m = re.match(r"#t=([\d.]+)", line)
        if m:
            t = float(m.group(1))
            continue
        m = re.match(r">(X|Y|Z|Rx|Ry|Rz):(-?[\d.]+)", line)
        if m:
            cur[m.group(1)] = float(m.group(2))
            if m.group(1) == "Rz":
                if len(cur) == 6:
                    samples.append((t, np.array([cur[a] for a in AXES])))
                cur = {}
    return samples

samples = parse(sys.argv[1])

# Labeled single-axis phases from the guided sequence (times from the
# timeline analysis). axis index, sign, window.
PHASES = [
    (0, +1, 58.8, 61.0),   # slide right
    (0, -1, 61.0, 63.0),   # slide left
    (1, -1, 65.5, 67.0),   # slide away
    (1, +1, 67.0, 69.0),   # slide toward
    (2, -1, 69.0, 72.0),   # pull up
    (2, +1, 72.0, 74.0),   # press down
    (3, +1, 74.0, 76.0),   # tilt back
    (3, -1, 76.0, 78.2),   # tilt forward
    (4, -1, 78.5, 81.0),   # rock right
    (4, +1, 81.0, 83.5),   # rock left
    (5, +1, 83.8, 86.0),   # twist cw
    (5, -1, 86.3, 89.0),   # twist ccw
]

S = []  # sensed raw vectors (signed, old gains divided out)
T = []  # target vectors
print("== phase plateau means (raw sensed units) ==")
for axis, sign, t0, t1 in PHASES:
    win = [v for (t, v) in samples if t0 <= t < t1]
    if not win:
        raise SystemExit(f"empty window {t0}-{t1}")
    arr = np.array(win)
    dom = arr[:, axis] * sign  # dominant axis, oriented positive
    peak = dom.max()
    if peak <= 0:
        raise SystemExit(
            f"Error: no positive deflection on {AXES[axis]} in window "
            f"{t0}-{t1}s — check the capture or phase windows.")
    plateau = arr[(dom > 0.6 * peak)]
    mean_raw = plateau.mean(axis=0) / GAINS_OLD
    S.append(mean_raw)
    tgt = np.zeros(6)
    tgt[axis] = sign * TARGET
    T.append(tgt)
    lbl = f"{'+' if sign>0 else '-'}{AXES[axis]}"
    print(f"{lbl:>4} n={len(plateau):3d}  " +
          " ".join(f"{a}:{m:7.2f}" for a, m in zip(AXES, mean_raw)))

S = np.array(S).T  # 6 x 12
T = np.array(T).T  # 6 x 12

# Least squares per output row: M @ S ~= T
M = T @ np.linalg.pinv(S)

print("\n== fitted decoupling matrix (row = output axis) ==")
for i, a in enumerate(AXES):
    print(f"{a:>3}: " + " ".join(f"{v:9.3f}" for v in M[i]))

# Per-direction trim so each phase lands exactly on 350 despite asymmetry
print("\n== decoupled outputs per phase & per-direction trims ==")
trims_pos = np.ones(6)
trims_neg = np.ones(6)
for k, (axis, sign, *_ ) in enumerate(PHASES):
    out = M @ S[:, k]
    achieved = out[axis]
    trim = (sign * TARGET) / achieved if achieved != 0 else 1.0
    if sign > 0:
        trims_pos[axis] = trim
    else:
        trims_neg[axis] = trim
    off = " ".join(f"{a}:{o:6.1f}" for a, o in zip(AXES, out) if a != AXES[axis] and abs(o) > 20)
    print(f"{'+' if sign>0 else '-'}{AXES[axis]:>3}: achieved {achieved:7.1f} trim {trim:5.3f}   residual coupling: {off or 'none >20'}")

# Post-matrix rest noise floor (first 4s), with trims applied
rest = np.array([v for (t, v) in samples if t < 4.0]) / GAINS_OLD
rest_out = (M @ rest.T)
floor = np.abs(rest_out).max(axis=1)
floor_trimmed = floor * np.maximum(trims_pos, trims_neg)
print("\n== post-matrix rest noise floor (trimmed) ==")
for a, f in zip(AXES, floor_trimmed):
    print(f"{a:>3}: {f:6.1f}")

# Emit C config
def carr(v):
    return "{" + ", ".join(f"{x:.4f}" for x in v) + "}"

print("\n== Config.h snippets ==")
print("const float DECOUPLE[6][6] = {")
for i in range(6):
    print("    " + carr(M[i]) + ("," if i < 5 else ""))
print("};")
print(f"const float TRIM_POS[6] = {carr(trims_pos)};")
print(f"const float TRIM_NEG[6] = {carr(trims_neg)};")
dead = np.maximum(np.round(floor_trimmed * 1.7), 10)
print(f"suggested DEAD_AXIS = {carr(dead)}")

# ---- Ridge fit toward diagonal prior, lambda sweep ----
print("\n\n==== RIDGE SWEEP (shrink toward per-axis diagonal gains) ====")
diag_prior = np.zeros((6, 6))
for i in range(6):
    mags = [abs(S[i, k]) for k, (ax, sg, *_) in enumerate(PHASES) if ax == i]
    diag_prior[i, i] = TARGET / np.mean(mags)

rest_raw = (np.array([v for (t, v) in samples if t < 4.0]) / GAINS_OLD).T

def evaluate(lam):
    Mr = np.zeros((6, 6))
    A = S @ S.T + lam * np.eye(6)
    for i in range(6):
        b = S @ T[i] + lam * diag_prior[i]
        Mr[i] = np.linalg.solve(A, b)
    # trims
    tp, tn = np.ones(6), np.ones(6)
    worst = 0.0
    for k, (axis, sign, *_ ) in enumerate(PHASES):
        out = Mr @ S[:, k]
        ach = out[axis]
        tr = (sign * TARGET) / ach
        if sign > 0: tp[axis] = tr
        else: tn[axis] = tr
    trim_max = np.maximum(tp, tn)
    for k, (axis, sign, *_ ) in enumerate(PHASES):
        out = (Mr @ S[:, k]) * trim_max  # worst-case trim view
        for j in range(6):
            if j != axis:
                worst = max(worst, abs(out[j]))
    floor = np.abs(Mr @ rest_raw).max(axis=1) * trim_max
    return Mr, tp, tn, worst, floor

for lam in [0.0, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0]:
    Mr, tp, tn, worst, floor = evaluate(lam)
    print(f"lam={lam:5.1f}  worst residual coupling: {worst:6.1f}  "
          f"noise floors: " + " ".join(f"{a}:{f:.0f}" for a, f in zip(AXES, floor)))

# ---- Final emit at chosen lambda ----
print("\n==== FINAL (lam=10) ====")
Mr, tp, tn, worst, floor = evaluate(10.0)
print("const float DECOUPLE[6][6] = {")
for i in range(6):
    print("    " + carr(Mr[i]) + ("," if i < 5 else ""))
print("};")
print(f"const float TRIM_POS[6] = {carr(tp)};")
print(f"const float TRIM_NEG[6] = {carr(tn)};")
print("floors: " + " ".join(f"{a}:{f:.0f}" for a, f in zip(AXES, floor)))
