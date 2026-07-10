import re, sys

if len(sys.argv) < 2:
    print(f"Usage: python3 {sys.argv[0]} <capture_file>")
    sys.exit(1)

AXES = ["X", "Y", "Z", "Rx", "Ry", "Rz"]

# Parse: #t= markers give flush timestamps; samples between markers share the
# last seen timestamp (50ms flush interval, good enough for phase binning).
samples = []  # (t, dict axis->float)
t = 0.0
cur = {}
for line in open(sys.argv[1]):
    line = line.strip()
    m = re.match(r"#t=([\d.]+)", line)
    if m:
        t = float(m.group(1))
        continue
    m = re.match(r">(X|Y|Z|Rx|Ry|Rz):(-?[\d.]+)", line)
    if m:
        cur[m.group(1)] = float(m.group(2))
        if m.group(1) == "Rz":  # last axis of each frame
            if len(cur) == 6:
                samples.append((t, dict(cur)))
            cur = {}

print(f"frames: {len(samples)}, span: {samples[0][0]:.1f}s .. {samples[-1][0]:.1f}s")

# Global extremes per axis
print("\n== global extremes (pre-deadzone, pre-clamp, current gains) ==")
for a in AXES:
    vals = [s[a] for _, s in samples]
    print(f"{a:>3}: min {min(vals):8.1f}  max {max(vals):8.1f}")

# 1-second bins: dominant axis + its peak value in the bin, to identify phases
print("\n== timeline (1s bins: dominant axis, peak signed value; '.' = quiet <30) ==")
end = samples[-1][0]
b = 0.0
while b < end:
    frame = [s for tt, s in samples if b <= tt < b + 1.0]
    if frame:
        peaks = {a: max((f[a] for f in frame), key=abs) for a in AXES}
        dom = max(AXES, key=lambda a: abs(peaks[a]))
        if abs(peaks[dom]) < 30:
            print(f"{b:5.0f}s  .")
        else:
            others = " ".join(f"{a}:{peaks[a]:.0f}" for a in AXES
                              if a != dom and abs(peaks[a]) > 0.4 * abs(peaks[dom]))
            print(f"{b:5.0f}s  {dom:>3} {peaks[dom]:8.1f}   {others}")
    b += 1.0

# Rest-phase noise floor: first 4 seconds
print("\n== rest phase (first 4s) per-axis abs-max (noise+drift floor) ==")
for a in AXES:
    vals = [abs(s[a]) for tt, s in samples if tt < 4.0]
    if vals:
        print(f"{a:>3}: {max(vals):7.1f}")
