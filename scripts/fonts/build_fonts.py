"""Build the LVGL bitmap fonts for the three UI styles.

Downloads the open-licence source fonts from the google/fonts repository (cached in
tmp/fonts_src), instantiates variable fonts at the needed weight and optical size,
freezes tabular/lining figures into the cmap, and converts every face/size in FONTS
with lv_font_conv into main/assets/skin_fonts/.

Usage: python3 scripts/fonts/build_fonts.py [name ...]
Requires: fonttools, node/npx (lv_font_conv 1.5.3 is fetched by npx).
"""
import subprocess
import sys
import urllib.parse
import urllib.request
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "tmp" / "fonts_src"
OUT = ROOT / "main" / "assets" / "skin_fonts"
RAW = "https://raw.githubusercontent.com/google/fonts/main/ofl/"

SOURCES = {
    "inter": "inter/Inter[opsz,wght].ttf",
    "intertight": "intertight/InterTight[wght].ttf",
    "plexmono-regular": "ibmplexmono/IBMPlexMono-Regular.ttf",
    "plexmono-medium": "ibmplexmono/IBMPlexMono-Medium.ttf",
    "cormorant": "cormorantgaramond/CormorantGaramond[wght].ttf",
    "jost": "jost/Jost[wght].ttf",
}

# Latin, Latin-1, Latin Extended-A (Polish, Dutch), Cyrillic (Russian, Ukrainian) and the
# punctuation the UI uses: dashes, quotes, bullet, ellipsis, arrows, minus, thin spaces.
TEXT = ("0x20-0x7E,0xA0-0x17F,0x400-0x45F,0x490-0x491,0x2009,0x2013-0x2014,0x2018-0x2019,"
        "0x201C-0x201D,0x2022,0x2026,0x202F,0x2190-0x2193,0x2212")
DIGITS = "0x20,0x25,0x2B-0x3A,0x3D,0x3F,0xD7,0x2013,0x2212"

# Glyphs missing from a face (Jost has no Ukrainian Є І Ї Ґ or arrows) come from a small
# fallback font built from FALLBACK and chained with lv_font_conv --lv-fallback.
FALLBACK = ("inter", {"opsz": 22})

# name: (source, axes, features to freeze, size, ranges)
FONTS = {
    # Instrument: Inter Tight for values and titles, IBM Plex Mono for labels and readouts.
    "it_light_144": ("intertight", {"wght": 300}, ["tnum"], 144, DIGITS),
    "it_light_88": ("intertight", {"wght": 300}, ["tnum"], 88, DIGITS),
    "it_light_56": ("intertight", {"wght": 300}, ["tnum"], 56, DIGITS),
    "it_medium_34": ("intertight", {"wght": 500}, [], 34, TEXT),
    "it_medium_28": ("intertight", {"wght": 500}, [], 28, TEXT),
    "it_medium_24": ("intertight", {"wght": 500}, [], 24, TEXT),
    "it_regular_24": ("intertight", {"wght": 400}, [], 24, TEXT),
    "it_regular_22": ("intertight", {"wght": 400}, [], 22, TEXT),
    "it_mono_22": ("plexmono-regular", {}, [], 22, TEXT),
    "it_mono_18": ("plexmono-regular", {}, [], 18, TEXT),
    "it_mono_medium_18": ("plexmono-medium", {}, [], 18, TEXT),
    # Kit: selected segment label, roller rows next to the selected time.
    "it_medium_22": ("intertight", {"wght": 500}, [], 22, TEXT),
    "it_light_34": ("intertight", {"wght": 300}, ["tnum"], 34, DIGITS),
    # Orbit: Inter with tabular figures for times.
    "or_semibold_168": ("inter", {"wght": 600, "opsz": 32}, ["tnum"], 168, DIGITS),
    "or_semibold_112": ("inter", {"wght": 600, "opsz": 32}, ["tnum"], 112, DIGITS),
    "or_semibold_56": ("inter", {"wght": 600, "opsz": 32}, ["tnum"], 56, DIGITS),
    "or_bold_40": ("inter", {"wght": 700, "opsz": 32}, [], 40, TEXT),
    "or_semibold_26": ("inter", {"wght": 600, "opsz": 26}, [], 26, TEXT),
    "or_medium_22": ("inter", {"wght": 500, "opsz": 22}, [], 22, TEXT),
    "or_regular_22": ("inter", {"wght": 400, "opsz": 22}, [], 22, TEXT),
    "or_semibold_18": ("inter", {"wght": 600, "opsz": 18}, [], 18, TEXT),
    # Atelier: Cormorant Garamond with lining figures, Jost for labels.
    "at_serif_168": ("cormorant", {"wght": 500}, ["lnum", "tnum"], 168, DIGITS),
    "at_serif_104": ("cormorant", {"wght": 500}, ["lnum", "tnum"], 104, DIGITS),
    "at_serif_56": ("cormorant", {"wght": 500}, ["lnum", "tnum"], 56, DIGITS),
    "at_serif_40": ("cormorant", {"wght": 600}, ["lnum"], 40, TEXT),
    "at_serif_30": ("cormorant", {"wght": 600}, ["lnum"], 30, TEXT),
    "at_sans_26": ("jost", {"wght": 400}, [], 26, TEXT),
    "at_sans_22": ("jost", {"wght": 400}, [], 22, TEXT),
    "at_sans_medium_20": ("jost", {"wght": 500}, [], 20, TEXT),
    "at_sans_medium_18": ("jost", {"wght": 500}, [], 18, TEXT),
}


def fetch(key):
    path = SRC / Path(SOURCES[key]).name
    if not path.exists():
        SRC.mkdir(parents=True, exist_ok=True)
        url = RAW + urllib.parse.quote(SOURCES[key])
        print(f"download {url}")
        urllib.request.urlretrieve(url, path)
    return path


def freeze_features(font, tags):
    """Point the cmap at the single-substitution targets of the given GSUB features,
    applied one feature after another in the given order (lnum, then tnum)."""
    if not tags or "GSUB" not in font:
        return
    gsub = font["GSUB"].table
    for tag in tags:
        lookup_ids = set()
        for record in gsub.FeatureList.FeatureRecord:
            if record.FeatureTag == tag:
                lookup_ids.update(record.Feature.LookupListIndex)
        mapping = {}
        for index in sorted(lookup_ids):
            lookup = gsub.LookupList.Lookup[index]
            for sub in lookup.SubTable:
                if lookup.LookupType == 7:
                    sub = sub.ExtSubTable
                if hasattr(sub, "mapping"):
                    mapping.update(sub.mapping)
        for table in font["cmap"].tables:
            for code, glyph in list(table.cmap.items()):
                if glyph in mapping:
                    table.cmap[code] = mapping[glyph]


def prepare(name, source, axes, features):
    out = SRC / "instances" / f"{name}.ttf"
    out.parent.mkdir(parents=True, exist_ok=True)
    font = TTFont(fetch(source))
    if "fvar" in font:
        available = {a.axisTag: (a.minValue, a.maxValue) for a in font["fvar"].axes}
        location = {tag: max(lo, min(hi, axes.get(tag, (lo + hi) / 2 if tag != "wght" else 400)))
                    for tag, (lo, hi) in available.items()}
        font = instancer.instantiateVariableFont(font, location)
    freeze_features(font, features)
    font.save(out)
    return out


def parse_ranges(ranges):
    codes = set()
    for part in ranges.split(","):
        lo, _, hi = part.partition("-")
        codes.update(range(int(lo, 16), int(hi or lo, 16) + 1))
    return codes


def format_ranges(codes):
    """Compact sorted code points back into an lv_font_conv range list."""
    out = []
    codes = sorted(codes)
    start = prev = codes[0]
    for code in codes[1:] + [None]:
        if code is not None and code == prev + 1:
            prev = code
            continue
        out.append(f"0x{start:X}" if start == prev else f"0x{start:X}-0x{prev:X}")
        if code is not None:
            start = prev = code
    return ",".join(out)


def build(name):
    source, axes, features, size, ranges = FONTS[name]
    ttf = prepare(name, source, axes, features)
    available = set(TTFont(ttf).getBestCmap())
    wanted = parse_ranges(ranges)
    missing = sorted(wanted - available)
    if missing:
        print(f"{name}: not in font: {' '.join(f'U+{c:04X}' for c in missing)}")
    ranges = format_ranges(wanted & available)
    fallback = None
    if missing and ranges != DIGITS:
        fallback = build_fallback(name, axes, size, missing)
    OUT.mkdir(parents=True, exist_ok=True)
    cmd = ["npx", "-y", "lv_font_conv@1.5.3", "--font", str(ttf), "-r", ranges,
           "--size", str(size), "--bpp", "4", "--format", "lvgl", "--no-compress",
           "--lv-include", "lvgl.h", "--lv-font-name", name, "-o", str(OUT / f"{name}.c")]
    if fallback:
        cmd += ["--lv-fallback", fallback]
    subprocess.run(cmd, check=True, cwd=ROOT)
    # Keep the generated header comment free of local paths.
    path = OUT / f"{name}.c"
    text = path.read_text().replace(str(ttf), f"{Path(SOURCES[source]).name} ({axes or 'static'}, {features or 'default figures'})")
    text = text.replace(str(ROOT) + "/", "")
    path.write_text(text)
    print(f"{name}: {path.stat().st_size // 1024} KB source")


def build_fallback(name, axes, size, missing):
    source, fb_axes = FALLBACK
    fb_name = f"{name}_fb"
    ttf = prepare(fb_name, source, {**fb_axes, "wght": axes.get("wght", 400)}, [])
    codes = sorted(set(missing) & set(TTFont(ttf).getBestCmap()))
    if not codes:
        return None
    cmd = ["npx", "-y", "lv_font_conv@1.5.3", "--font", str(ttf), "-r", format_ranges(codes),
           "--size", str(size), "--bpp", "4", "--format", "lvgl", "--no-compress",
           "--lv-include", "lvgl.h", "--lv-font-name", fb_name, "-o", str(OUT / f"{fb_name}.c")]
    subprocess.run(cmd, check=True, cwd=ROOT)
    path = OUT / f"{fb_name}.c"
    path.write_text(path.read_text().replace(str(ttf), f"{Path(SOURCES[source]).name} (fallback glyphs)")
                    .replace(str(ROOT) + "/", ""))
    FALLBACKS.add(fb_name)
    return fb_name


FALLBACKS = set()


def write_header():
    lines = ["#pragma once", "", "/* Generated by scripts/fonts/build_fonts.py. */", "",
             '#include "lvgl.h"', ""]
    lines += [f"LV_FONT_DECLARE({name})" for name in FONTS]
    fallbacks = sorted(p.stem for p in OUT.glob("*_fb.c"))
    if fallbacks:
        lines += ["", "/* Fallback glyphs, chained from the fonts above. */"]
        lines += [f"LV_FONT_DECLARE({name})" for name in fallbacks]
    (OUT / "skin_fonts.h").write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    names = sys.argv[1:] or list(FONTS)
    for n in names:
        build(n)
    write_header()
