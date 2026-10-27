"""Summarise LVGL builtin-profiler traces captured by the perf tour.

The perf tour prints "PROF_BEGIN <segment>", then "PROF <trace line>" for every event and
"PROF_END". For each profiled segment this prints the functions with the most inclusive
and self time, per thread.

Usage: python3 scripts/prof_summary.py <serial.log> [top_n]
"""
import re
import sys
from collections import defaultdict

LINE = re.compile(r"LVGL-(\d+) \[(\d+)\] (\d+)\.(\d+): tracing_mark_write: ([BE])\|1\|(.+)$")


def summarise(name, events, top):
    stacks = defaultdict(list)
    inclusive = defaultdict(float)
    self_time = defaultdict(float)
    calls = defaultdict(int)
    span = [None, None]
    for tid, t, tag, func in events:
        span[0] = t if span[0] is None else min(span[0], t)
        span[1] = t if span[1] is None else max(span[1], t)
        stack = stacks[tid]
        if tag == "B":
            stack.append([func, t, 0.0])
            continue
        while stack and stack[-1][0] != func:
            stack.pop()
        if not stack:
            continue
        f, start, child = stack.pop()
        dur = t - start
        if dur > 1.0:
            # Printing the previous trace stalls every thread; that is not UI work.
            if stack:
                stack[-1][2] += dur
            continue
        inclusive[(tid, f)] += dur
        self_time[(tid, f)] += dur - child
        calls[(tid, f)] += 1
        if stack:
            stack[-1][2] += dur
    frames = max(1, calls.get(next((k for k in calls if k[1] == "refr_invalid_areas"), None), 0)
                 or sum(v for k, v in calls.items() if k[1] == "lv_display_refr_timer") or 1)
    print(f"\n=== {name}: {len(events)} events, {frames} frames (ms per frame below)")
    rows = sorted(self_time.items(), key=lambda kv: -kv[1])[:top]
    print(f"  {'thread':>6} {'self':>8} {'incl':>8} {'calls':>7}  function")
    for key, s in rows:
        tid, f = key
        print(f"  {tid:>6} {s * 1000 / frames:8.2f} {inclusive[key] * 1000 / frames:8.2f} {calls[key] / frames:7.1f}  {f}")


def main(path, top):
    current = None
    events = []
    for raw in open(path, "rb").read().decode("utf-8", "replace").splitlines():
        if "PROF_BEGIN" in raw:
            if current:
                summarise(current, events, top)
            current = raw.split("PROF_BEGIN", 1)[1].strip()
            events = []
            continue
        if "PROF_END" in raw:
            continue  # the trace is flushed after PROF_END and still belongs to `current`
        if current and "PROF " in raw:
            m = LINE.search(raw)
            if m:
                tid, _cpu, sec, frac, tag, func = m.groups()
                events.append((int(tid), int(sec) + int(frac) / 10 ** len(frac), tag, func.strip()))
    if current:
        summarise(current, events, top)


if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 15)
