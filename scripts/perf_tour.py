#!/usr/bin/env python3
"""Build, flash and run the on-device UI performance tour, then write a report.

Run with the ESP-IDF Python environment (it has pyserial), from the repo root:

    ~/.espressif/python_env/idf5.5_py3.12_env/bin/python scripts/perf_tour.py --restore

The perf firmware is built in build_perf/ from a copy of the local sdkconfig with
CONFIG_TOUCH_CLOCK_PERF_TOUR enabled and the console moved to USB-Serial-JTAG
(the UART console is too slow for thumbnails). The normal build/ directory is not
touched; --restore flashes the existing build/ artifacts back without rebuilding.
"""

import argparse
import glob
import json
import os
import re
import struct
import subprocess
import sys
import time
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PERF_BUILD = ROOT / "build_perf"
PERF_IMAGES = ROOT / "perf_images"
NORMAL_BUILD = ROOT / "build"
DEFAULT_IDF_EXPORT = Path.home() / "esp/v5.5.3/esp-idf/export.sh"
DEFAULT_IDF_PY_ENV = Path.home() / ".espressif/python_env/idf5.5_py3.12_env"

PERF_OVERRIDES = {
    "CONFIG_TOUCH_CLOCK_PERF_TOUR": "y",
    "CONFIG_TOUCH_CLOCK_PERF_TOUR_THUMBNAILS": "y",
    "CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG": "y",
    "CONFIG_ESP_CONSOLE_SECONDARY_NONE": "y",
    "CONFIG_FREERTOS_USE_TRACE_FACILITY": "y",
    "CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS": "y",
}
PERF_DROP_PREFIXES = ("CONFIG_ESP_CONSOLE_UART", "CONFIG_ESP_CONSOLE_SECONDARY_", "CONFIG_CONSOLE_UART")

SEG_RE = re.compile(r"PERF seg (.*)")
SEGX_RE = re.compile(r"PERF segx (.*)")
KV_RE = re.compile(r"(\w+)=(\S+)")


def idf_shell(cmd, cwd=ROOT):
    export = Path(os.environ.get("IDF_EXPORT", DEFAULT_IDF_EXPORT))
    env = dict(os.environ)
    if "IDF_PYTHON_ENV_PATH" not in env and DEFAULT_IDF_PY_ENV.exists():
        env["IDF_PYTHON_ENV_PATH"] = str(DEFAULT_IDF_PY_ENV)
    full = f'source "{export}" >/dev/null 2>&1 && {cmd}'
    print(f"$ {cmd}", flush=True)
    result = subprocess.run(["bash", "-c", full], cwd=cwd, env=env)
    if result.returncode != 0:
        sys.exit(f"command failed ({result.returncode}): {cmd}")


def write_perf_sdkconfig(extra=None):
    src = ROOT / "sdkconfig"
    if not src.exists():
        sys.exit("local sdkconfig not found; build the normal firmware once first")
    PERF_BUILD.mkdir(exist_ok=True)
    lines = []
    for line in src.read_text().splitlines():
        key = line.split("=", 1)[0].replace("# ", "").replace(" is not set", "").strip()
        if key in PERF_OVERRIDES or key.startswith(PERF_DROP_PREFIXES):
            continue
        lines.append(line)
    overrides = dict(PERF_OVERRIDES, **(extra or {}))
    lines = [l for l in lines if l.split("=", 1)[0].replace("# ", "").replace(" is not set", "").strip() not in overrides]
    lines += [f"{k}={v}" for k, v in overrides.items()]
    (PERF_BUILD / "sdkconfig").write_text("\n".join(lines) + "\n")


def build_perf(extra=None):
    write_perf_sdkconfig(extra)
    idf_shell('idf.py -B build_perf -D SDKCONFIG="$PWD/build_perf/sdkconfig" reconfigure build')


def flash(build_dir, port):
    flash_args = build_dir / "flash_args"
    if not flash_args.exists():
        sys.exit(f"{flash_args} missing; build first")
    idf_shell(f'python -m esptool --chip esp32p4 -p {port} -b 460800 '
              f'--before default_reset --after hard_reset write_flash "@flash_args"', cwd=build_dir)


def save_image(name):
    dest = PERF_IMAGES / name
    for rel in ["flash_args"] + re.findall(r"\S+\.bin", (PERF_BUILD / "flash_args").read_text()):
        (dest / rel).parent.mkdir(parents=True, exist_ok=True)
        (dest / rel).write_bytes((PERF_BUILD / rel).read_bytes())
    print(f"saved perf image: {dest}")


def compare(report_a, report_b):
    a = json.loads((Path(report_a) / "summary.json").read_text())
    b = json.loads((Path(report_b) / "summary.json").read_text())
    lines = ["| step | render avg ms A -> B | p95 A -> B | max A -> B | longest gap A -> B | >33ms A -> B | white A -> B |",
             "|---|---|---|---|---|---|---|"]
    for name in list(a) + [n for n in b if n not in a]:
        x, y = a.get(name), b.get(name)
        if not x or not y:
            lines.append(f"| {name} | only in {'A' if x else 'B'} | | | | | |")
            continue
        lines.append(f"| {name} | {x['render_avg_ms']:.1f} -> {y['render_avg_ms']:.1f} | "
                     f"{x['render_p95_ms']:.1f} -> {y['render_p95_ms']:.1f} | "
                     f"{x['render_max_ms']:.1f} -> {y['render_max_ms']:.1f} | "
                     f"{x['interval_max_ms']:.0f} -> {y['interval_max_ms']:.0f} | "
                     f"{x['slow_frames']} -> {y['slow_frames']} | {x['white_frames']} -> {y['white_frames']} |")
    print("\n".join(lines))
    return lines


def find_port(explicit):
    if explicit:
        return explicit
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("no /dev/cu.usbmodem* port found; is the clock connected?")
    return ports[0]


def capture(port, log_path, timeout_s):
    import serial

    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.2
    ser.dtr = False
    ser.rts = False
    ser.open()
    ser.rts = True
    time.sleep(0.2)
    ser.rts = False

    deadline = time.time() + timeout_s
    buf = b""
    done = False
    with open(log_path, "wb") as out:
        while time.time() < deadline:
            chunk = ser.read(65536)
            if not chunk:
                if done:
                    break
                continue
            out.write(chunk)
            buf = (buf + chunk)[-4096:]
            if b"PERF tour_done" in buf:
                done = True
                deadline = min(deadline, time.time() + 2)
    ser.close()
    return done


def write_png(path, width, height, rgb_rows):
    raw = b"".join(b"\x00" + bytes(row) for row in rgb_rows)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def parse(log_path, out_dir):
    text = log_path.read_bytes().decode("utf-8", "replace").splitlines()
    segments = {}
    extras = {}
    order = []
    white = []
    thumbs = {}
    current_thumb = None
    rows = []
    for line in text:
        line = line.strip()
        m = SEGX_RE.search(line)
        if m:
            kv = dict(KV_RE.findall(m.group(1)))
            extras.setdefault(kv["name"], []).append({k: int(v) for k, v in kv.items() if v.isdigit()})
            continue
        m = SEG_RE.search(line)
        if m:
            kv = dict(KV_RE.findall(m.group(1)))
            if "name" not in kv or "t_ms" not in kv:
                continue  # cut short by another task's output on the same line
            name = kv["name"]
            if name not in segments:
                segments[name] = []
                order.append(name)
            segments[name].append({k: int(v) if v.lstrip("-").isdigit() else v for k, v in kv.items()})
            continue
        if "PERF white_frame" in line:
            white.append(line[line.index("PERF"):])
            continue
        if line.startswith("PERF thumb ") or " PERF thumb " in line:
            parts = line[line.index("PERF thumb "):].split()
            current_thumb = (parts[2], int(parts[3]), int(parts[4]))
            rows = []
            continue
        if current_thumb and "PERF px " in line:
            hexs = line[line.index("PERF px ") + 8:].strip()
            row = []
            try:
                for i in range(0, len(hexs) - 3, 4):
                    v = int(hexs[i:i + 4], 16)
                    row += [((v >> 11) & 0x1F) * 255 // 31, ((v >> 5) & 0x3F) * 255 // 63, (v & 0x1F) * 255 // 31]
            except ValueError:
                row = []
            rows.append(row)
            continue
        if current_thumb and "PERF thumb_end" in line:
            name, w, h = current_thumb
            if len(rows) == h and all(len(r) == w * 3 for r in rows):
                thumbs.setdefault(name, rows)
            current_thumb = None

    for name, rows in thumbs.items():
        write_png(out_dir / f"thumb_{name}.png", len(rows[0]) // 3, len(rows), rows)

    def agg(runs):
        frames = sum(r["frames"] for r in runs)
        weighted = lambda k: (sum(r[k] * r["frames"] for r in runs) / frames) if frames else 0
        return {
            "loops": len(runs),
            "frames": frames,
            "render_avg_ms": weighted("render_avg") / 1000,
            "render_p95_ms": max(r["render_p95"] for r in runs) / 1000,
            "render_max_ms": max(r["render_max"] for r in runs) / 1000,
            "pre_avg_ms": weighted("pre_avg") / 1000,
            "pre_max_ms": max(r["pre_max"] for r in runs) / 1000,
            "interval_max_ms": max(r["interval_max"] for r in runs) / 1000,
            "slow_frames": sum(r["slow"] for r in runs),
            "white_frames": sum(r["white"] for r in runs),
            "internal_free_min": min(r["internal_free_min"] for r in runs),
        }

    def agg_extra(name):
        runs = extras.get(name, [])
        frames = sum(r.get("frames", 0) for r in runs)
        if frames == 0:
            return {"flush_wait_ms": 0.0, "ppa_ms": 0.0}
        return {
            "flush_wait_ms": sum(r.get("flush_wait_avg", 0) * r.get("frames", 0) for r in runs) / frames / 1000,
            "ppa_ms": sum(r.get("ppa_us_avg", 0) * r.get("frames", 0) for r in runs) / frames / 1000,
        }

    summary = {name: {**agg(segments[name]), **agg_extra(name)} for name in order}
    md = ["| step | frames | render avg ms | p95 | max | flush wait ms | PPA ms | layout avg/max ms | longest gap ms "
          "| >33ms | white |",
          "|---|---|---|---|---|---|---|---|---|---|---|"]
    for name in order:
        s = summary[name]
        md.append(f"| {name} | {s['frames']} | {s['render_avg_ms']:.1f} | {s['render_p95_ms']:.1f} | "
                  f"{s['render_max_ms']:.1f} | {s['flush_wait_ms']:.1f} | {s['ppa_ms']:.1f} | "
                  f"{s['pre_avg_ms']:.1f}/{s['pre_max_ms']:.1f} | "
                  f"{s['interval_max_ms']:.0f} | {s['slow_frames']} | {s['white_frames']} |")
    underruns = [l for l in text if "underrun" in l]
    crashes = [l for l in text if "Guru Meditation" in l or "abort()" in l or "assert failed" in l]
    resets = [l for l in text if "rst:" in l]
    md += ["", f"DSI underrun messages: {len(underruns)}", f"Crashes/asserts: {len(crashes)}",
           f"Boots seen: {len(resets)}"]
    if underruns:
        md += [""] + [f"- `{u.strip()}`" for u in underruns[:10]]
    if white:
        md += ["", "White frames rendered by LVGL:", ""] + [f"- `{w}`" for w in white]
    md += ["", f"Thumbnails: {', '.join(sorted(thumbs)) or 'none'}"]
    (out_dir / "report.md").write_text("\n".join(md) + "\n")
    (out_dir / "summary.json").write_text(json.dumps(summary, indent=2))
    print("\n".join(md))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port")
    ap.add_argument("--no-build", action="store_true", help="flash the existing build_perf artifacts")
    ap.add_argument("--no-flash", action="store_true", help="only capture and parse (perf firmware already running)")
    ap.add_argument("--restore", action="store_true", help="flash the existing normal build/ artifacts afterwards")
    ap.add_argument("--timeout", type=int, default=420, help="capture timeout in seconds")
    ap.add_argument("--parse", help="parse an existing log file instead of running")
    ap.add_argument("--image", help="flash a saved image from perf_images/<name> instead of building")
    ap.add_argument("--save-image", help="after building, save the perf image as perf_images/<name>")
    ap.add_argument("--build-only", action="store_true", help="build (and optionally --save-image), then stop")
    ap.add_argument("--manual", action="store_true",
                    help="build a monitor-only image (no scripted tour) for reproducing issues by hand")
    ap.add_argument("--compare", nargs=2, metavar=("REPORT_A", "REPORT_B"), help="compare two report dirs")
    ap.add_argument("--set", action="append", default=[], metavar="CONFIG_X=VALUE",
                    help="extra sdkconfig option for the perf build (repeatable)")
    args = ap.parse_args()

    if args.compare:
        compare(*args.compare)
        return

    extra = {"CONFIG_TOUCH_CLOCK_PERF_TOUR_MANUAL_ONLY": "y",
             "CONFIG_TOUCH_CLOCK_PERF_TOUR_START_DELAY_S": "15"} if args.manual else {}
    extra.update(item.split("=", 1) for item in args.set)

    if args.build_only:
        build_perf(extra)
        if args.save_image:
            save_image(args.save_image)
        return

    label = args.image or "build_perf"
    out_dir = ROOT / "perf_reports" / f"{time.strftime('%Y%m%d-%H%M%S')}-{label}"
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.parse:
        parse(Path(args.parse), out_dir)
        print(f"\nreport: {out_dir / 'report.md'}")
        return

    port = find_port(args.port)
    if not args.no_flash:
        if args.image:
            flash(PERF_IMAGES / args.image, port)
        else:
            if not args.no_build:
                build_perf(extra)
                if args.save_image:
                    save_image(args.save_image)
            flash(PERF_BUILD, port)
        time.sleep(1)

    log_path = out_dir / "serial.log"
    done = capture(port, log_path, args.timeout)
    print(f"capture {'complete' if done else 'TIMED OUT'}: {log_path}")
    parse(log_path, out_dir)

    if args.restore:
        flash(NORMAL_BUILD, port)
    print(f"\nreport: {out_dir / 'report.md'}")


if __name__ == "__main__":
    main()
