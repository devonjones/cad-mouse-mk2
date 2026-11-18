"""Segment a raw-HID capture into single-axis ramps and summarise each.

Input lines: t_ms device report_id v0 v1 v2 (report 1 = TX TY TZ, 2 = RX RY RZ).
Usage: analyze_hid.py <capture.txt> <device tag: real|diy> [--cadence]
--cadence prints how often each report's values actually change while
deflected. A host driver that throttles reports shows up there, not in
the per-axis peaks.
"""
import sys

AX = ["TX", "TY", "TZ", "RX", "RY", "RZ"]


def load(path, dev):
    state = [0] * 6
    frames = []  # (t, [6])
    for line in open(path):
        p = line.split()
        if len(p) != 6 or p[1] != dev:
            continue
        t, rid, v = int(p[0]), int(p[2]), list(map(int, p[3:]))
        if rid == 1:
            state[0:3] = v
        elif rid == 2:
            state[3:6] = v
        else:
            continue
        frames.append((t, state[:]))
    return frames


def segments(frames, gap_ms=300, quiet=8):
    segs, cur, last_active = [], [], None
    for t, v in frames:
        active = any(abs(x) > quiet for x in v)
        if active:
            if last_active is not None and t - last_active > gap_ms and cur:
                segs.append(cur)
                cur = []
            cur.append((t, v))
            last_active = t
    if cur:
        segs.append(cur)
    return [s for s in segs if len(s) > 10]


def summarise(seg):
    energy = [sum(abs(v[i]) for _, v in seg) for i in range(6)]
    a = max(range(6), key=lambda i: energy[i])
    sign = 1 if sum(v[a] for _, v in seg) > 0 else -1
    vals = [sign * v[a] for _, v in seg]
    peak = max(vals)
    t0 = seg[0][0]
    first = next((v for v in vals if v > 0), 0)

    def t_at(frac):
        for (t, _), x in zip(seg, vals):
            if x >= frac * peak:
                return t - t0
        return None

    t95 = t_at(0.95) or 1
    shape = [round((t_at(f) or 0) / t95, 2) for f in (0.1, 0.25, 0.5, 0.75)]
    # Bleed: other axes relative to the dominant axis, over frames where the
    # dominant axis is above 30% of its peak (the deliberate part of the push).
    bleed = {}
    for i in range(6):
        if i == a:
            continue
        r = [abs(v[i]) / abs(v[a]) for _, v in seg if abs(v[a]) > 0.3 * peak]
        if r:
            bleed[AX[i]] = round(sum(r) / len(r), 2)
    worst = max(bleed.items(), key=lambda kv: kv[1]) if bleed else ("-", 0)
    sat = sum(1 for x in vals if x >= 345) / len(vals)
    dt = [b[0] - a_[0] for a_, b in zip(seg, seg[1:])]
    rate = 1000 / (sorted(dt)[len(dt) // 2] or 1)
    return {
        "axis": ("+" if sign > 0 else "-") + AX[a], "n": len(seg),
        "dur_ms": seg[-1][0] - t0, "first": first, "peak": peak,
        "sat%": round(100 * sat), "t_to95_ms": t95,
        "shape(t10,t25,t50,t75)/t95": shape,
        "bleed_mean": round(sum(bleed.values()) / max(len(bleed), 1), 2),
        "worst_bleed": f"{worst[0]}={worst[1]}", "rate_hz": round(rate),
    }


def cadence(path, dev):
    last, changes = {}, {1: [], 2: []}
    for line in open(path):
        p = line.split()
        if len(p) != 6 or p[1] != dev or int(p[2]) not in changes:
            continue
        t, rid, v = int(p[0]), int(p[2]), tuple(map(int, p[3:]))
        if any(v) and v != last.get(rid):
            changes[rid].append(t)
        last[rid] = v
    for rid, ts in changes.items():
        # Gaps over 300 ms are pauses between moves, not update gaps.
        g = sorted(b - a for a, b in zip(ts, ts[1:]) if b - a < 300)
        if not g:
            print(f"report {rid}: no changing frames")
            continue
        q = lambda f: g[int(f * (len(g) - 1))]
        print(f"report {rid} ({'translation' if rid == 1 else 'rotation'}): "
              f"value-change gap p50 {q(.5)} ms, p90 {q(.9)} ms, "
              f"p99 {q(.99)} ms (n={len(g)})")


if __name__ == "__main__":
    path, dev = sys.argv[1], sys.argv[2]
    if "--cadence" in sys.argv[3:]:
        cadence(path, dev)
        sys.exit(0)
    segs = segments(load(path, dev))
    print(f"{dev}: {len(segs)} segments")
    for s in segs:
        print(summarise(s))
