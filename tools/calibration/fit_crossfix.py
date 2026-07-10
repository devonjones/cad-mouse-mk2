import re
import sys
import numpy as np

AXES = ["X", "Y", "Z", "Rx", "Ry", "Rz"]

# verify4 telemetry is gained() = post-matrix, post-OLD-trim values.
# Rescale to v2's NEW trims so coefficients apply to what v2 computes.
OLD_POS = np.array([1.1403, 1.1873, 1.3074, 0.9970, 0.8971, 0.8984])
OLD_NEG = np.array([1.5141, 1.4741, 1.8186, 1.1210, 1.6126, 1.1909])
NEW_POS = np.array([0.984, 1.187, 1.023, 0.870, 0.897, 0.992])
NEW_NEG = np.array([1.453, 1.384, 2.000, 1.121, 1.450, 1.336])

def parse(path):
    samples, t, cur = [], 0.0, {}
    for line in open(path):
        line = line.strip()
        m = re.match(r"#t=([\d.]+)", line)
        if m:
            t = float(m.group(1)); continue
        m = re.match(r">(X|Y|Z|Rx|Ry|Rz):(-?[\d.]+)", line)
        if m:
            cur[m.group(1)] = float(m.group(2))
            if m.group(1) == "Rz":
                if len(cur) == 6:
                    samples.append((t, np.array([cur[a] for a in AXES])))
                cur = {}
    return samples

samples = parse(sys.argv[1])

PHASES = [  # axis, sign, t0, t1
    (0, +1, 0.5, 3.0), (0, -1, 4.5, 7.0),
    (1, -1, 8.5, 10.5), (1, +1, 10.8, 12.8),
    (2, -1, 13.0, 15.5), (2, +1, 15.8, 17.5),
    (3, +1, 17.8, 19.5), (3, -1, 19.8, 22.5),
    (4, -1, 22.8, 24.5), (4, +1, 24.8, 27.0),
    (5, +1, 27.5, 29.5), (5, -1, 29.8, 31.5),
]

def retrim(v):
    out = np.empty(6)
    for i in range(6):
        old = OLD_POS[i] if v[i] >= 0 else OLD_NEG[i]
        new = NEW_POS[i] if v[i] >= 0 else NEW_NEG[i]
        out[i] = v[i] / old * new
    return out

CROSS_POS = np.zeros((6, 6))  # [source][target]
CROSS_NEG = np.zeros((6, 6))

print("== verify4 phase means (rescaled to v2 trims) and coupling ratios ==")
for axis, sign, t0, t1 in PHASES:
    win = np.array([v for (t, v) in samples if t0 <= t < t1])
    dom = win[:, axis] * sign
    plateau = win[dom > 0.6 * dom.max()]
    mean = retrim(plateau.mean(axis=0))
    src = mean[axis]
    lbl = f"{'+' if sign > 0 else '-'}{AXES[axis]}"
    ratios = []
    for tgt in range(6):
        if tgt == axis:
            continue
        r = mean[tgt] / src
        # Only correct meaningful coupling, and only rotation->translation
        # and translation->Z (the measured offenders); leave the rest to
        # the dead zones.
        significant = abs(r) > 0.15 and (
            (axis >= 3 and tgt < 3) or (axis < 2 and tgt == 2))
        if significant:
            if sign > 0:
                CROSS_POS[axis][tgt] = r
            else:
                CROSS_NEG[axis][tgt] = r
            ratios.append(f"{AXES[tgt]}:{r:+.3f}")
    print(f"{lbl:>4}: src {src:7.1f}   corrections: {' '.join(ratios) or '-'}")

def carr2(M):
    rows = [",\n    ".join(
        "{" + ", ".join(f"{x:.4f}" for x in M[i]) + "}" for i in range(6))]
    return "{\n    " + rows[0] + "\n};"

print("\nconst float CROSS_POS[6][6] = " + carr2(CROSS_POS))
print("const float CROSS_NEG[6][6] = " + carr2(CROSS_NEG))
