#!/usr/bin/env python3
"""Fit the simulator's device cost model from microbenchmarks.

1. On the clock (one style; Orbit by default):
     scripts/perf_tour.py --set CONFIG_TOUCH_CLOCK_PERF_TOUR_MICROBENCH=y \
         --set CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLES=y --set CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLE_FIRST=1 \
         --set CONFIG_TOUCH_CLOCK_PERF_TOUR_LOOPS=1 --set CONFIG_TOUCH_CLOCK_PERF_TOUR_THUMBNAILS=n
2. In the simulator, the same scenes' features (same style):
     printf 'set style 1\\nwait 500\\nmicrobench mb.tsv\\n' > mb.txt && sim/build/sim mb.txt out
3. python3 sim/cost_fit.py mb.tsv perf_reports/<run>/serial.log
   writes sim/cost_model.txt, which the simulator's "estimate" command reads.

The model: frame render time = c0 + task x tasks + sum over classes of c_class x pixels_class
(microseconds), fitted with non-negative least squares. "cpu_scale" is the clock's time over the
host's for the same per-pixel CPU loop: multiply host timings of CPU-bound code (bakes, face
updates) by it.
"""
import re
import sys
from pathlib import Path

import numpy as np
from scipy.optimize import nnls

HERE = Path(__file__).resolve().parent


def read_features(path):
    feats = {}
    host_cpu = None
    for line in Path(path).read_text().splitlines():
        parts = line.split("\t")
        name = parts[0]
        kv = dict(p.split("=", 1) for p in parts[1:])
        if name == "cpu_loop":
            host_cpu = float(kv["host_us"])
            continue
        feats[name] = {k: float(v) for k, v in kv.items()}
    return feats, host_cpu


def read_device(path):
    times = {}
    cpu = None
    for line in Path(path).read_text(errors="replace").splitlines():
        m = re.search(r"PERF seg loop=\d+ name=(?:[a-z]+\.)?mb_([a-z0-9_]+) frames=(\d+) render_avg=(\d+)", line)
        if m and int(m.group(2)) >= 5:
            times[m.group(1)] = float(m.group(3))
        m = re.search(r"PERF cpu_loop us=(\d+)", line)
        if m:
            cpu = float(m.group(1))
    return times, cpu


def main():
    feats, host_cpu = read_features(sys.argv[1])
    dev, dev_cpu = read_device(sys.argv[2])
    names = [n for n in feats if n in dev]
    classes = sorted({k for n in names for k in feats[n] if k != "tasks"})
    cols = ["c0", "task"] + classes
    a = np.array([[1.0, feats[n].get("tasks", 0)] + [feats[n].get(c, 0) for c in classes] for n in names])
    b = np.array([dev[n] for n in names])
    # Scale columns so the solver treats them evenly, then undo.
    scale = np.maximum(a.max(axis=0), 1e-9)
    x, _ = nnls(a / scale, b)
    x = x / scale
    pred = a @ x
    print(f"{'scene':16s} {'device ms':>9s} {'model ms':>9s} {'err %':>6s}")
    for n, d, p in zip(names, b, pred):
        print(f"{n:16s} {d / 1000:9.2f} {p / 1000:9.2f} {100 * (p - d) / d:6.1f}")
    rms = np.sqrt(np.mean(((pred - b) / b) ** 2)) * 100
    print(f"rms error {rms:.1f} %")
    lines = [f"{c} {v:.6g}" for c, v in zip(cols, x)]
    if host_cpu and dev_cpu:
        lines.append(f"cpu_scale {dev_cpu / host_cpu:.3f}")
        print(f"cpu_scale {dev_cpu / host_cpu:.2f} (device {dev_cpu:.0f} us, host {host_cpu:.0f} us)")
    out = HERE / "cost_model.txt"
    out.write_text("\n".join(lines) + "\n")
    print(f"-> {out}")
    for c, v in zip(cols, x):
        unit = "us" if c in ("c0", "task") else "ns/px"
        print(f"  {c:12s} {v * (1 if unit == 'us' else 1000):8.3f} {unit}")


if __name__ == "__main__":
    main()
