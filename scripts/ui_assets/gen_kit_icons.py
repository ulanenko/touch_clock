"""Generate the kit icon sources (SVG) and small decorations (PNG) for the three UI styles.

Writes main/ui/assets_src/kit/<style>/<icon>.svg for every kit_icon_t (ui_kit.h) in each
style's icon language, and a few decorative PNGs rendered with headless Chrome:

  instrument  28 px grid, 2 px stroke, round caps (paths from instrument.html)
  orbit       24-unit grid drawn at 28 px, 2.5 px optical stroke (paths from orbit.html)
  atelier     24-unit grid drawn at 28 px, 1.75 px stroke (paths from atelier.html)

Then convert with: python3 scripts/ui_assets/build_ui_assets.py kit

Usage: python3 scripts/ui_assets/gen_kit_icons.py
Requires: playwright with Chrome (for the PNG decorations).
"""
import asyncio
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "main" / "ui" / "assets_src" / "kit"
SIZE = 28
# Small icons: chevrons and locks in rows, icons inside buttons and chips.
SMALL = {"instrument": 24, "orbit": 24, "atelier": 22}
# Large icons: Orbit scales the glyph with its round keys (80-96 px keys).
LARGE = {"orbit": 36}

# Shared 24-unit drawings for icons a style's presentation does not define. Each style draws
# them with its own stroke, so they still read as part of that set.
COMMON24 = {
    "themes": '<path d="M12 3.5a8.5 8.5 0 1 0 0 17c1.2 0 1.8-.8 1.8-1.7 0-1.3-1.1-1.6-1.1-2.7 0-1 .8-1.6 1.8-1.6h2.2a3.8 3.8 0 0 0 3.8-3.8C20.5 6.6 16.7 3.5 12 3.5z"/>'
              '<circle cx="7.6" cy="11.6" r="1.25" fill="currentColor" stroke="none"/>'
              '<circle cx="10.2" cy="7.6" r="1.25" fill="currentColor" stroke="none"/>'
              '<circle cx="14.8" cy="7.6" r="1.25" fill="currentColor" stroke="none"/>',
    "repeat": '<path d="M4.5 11.5V10a3 3 0 0 1 3-3h12"/><path d="M16.5 4l3 3-3 3"/>'
              '<path d="M19.5 12.5V14a3 3 0 0 1-3 3h-12"/><path d="M7.5 20l-3-3 3-3"/>',
    "snooze": '<path d="M3.5 12.5h5.5l-5.5 7h5.5"/><path d="M12 4.5h8.5L12 16.5h8.5"/>',
    "restart": '<path d="M12 3.5v7.5"/><path d="M7.2 6.2a7.8 7.8 0 1 0 9.6 0"/>',
    "style": '<path d="M12 3.5l8.5 4.5L12 12.5 3.5 8z"/><path d="M3.5 12L12 16.5l8.5-4.5"/>'
             '<path d="M3.5 16L12 20.5l8.5-4.5"/>',
    "info": '<circle cx="12" cy="12" r="8.5"/><path d="M12 11v5.5M12 7.6v.1"/>',
    "minus": '<path d="M5 12h14"/>',
    "stop": '<rect x="6.5" y="6.5" width="11" height="11" rx="2"/>',
}


def orbit_gear():
    import math
    d, n, ro, ri, to, tr = "", 8, 9.6, 7.2, .2, .34
    for i in range(n):
        a = i / n * 2 * math.pi - math.pi / 2
        for j, (r, t) in enumerate([(ri, a - tr), (ro, a - to), (ro, a + to), (ri, a + tr)]):
            d += ("M" if i == 0 and j == 0 else "L") + f"{12 + r * math.cos(t):.2f} {12 + r * math.sin(t):.2f}"
    return f'<path d="{d}Z"/><circle cx="12" cy="12" r="3"/>'


def atelier_gear():
    import math
    rad = math.pi / 180
    d = ""
    for k in range(8):
        c = k * 45
        p = lambda r, a: f"{12 + r * math.sin(a * rad):.2f} {12 - r * math.cos(a * rad):.2f}"
        pts = [p(7.3, c - 15), p(9.7, c - 8.5), p(9.7, c + 8.5), p(7.3, c + 15)]
        d += ("L" if k else "M") + "L".join(pts)
    return f'<path d="{d}Z"/><circle cx="12" cy="12" r="3"/>'


DIM = 'stroke-opacity=".28" fill-opacity=".28"'

# --- Orbit (orbit.html, 24-unit grid) ---------------------------------------------------
O = {
    "back": '<path d="M15 5l-7 7 7 7"/>',
    "close": '<path d="M6 6l12 12M18 6L6 18"/>',
    "chevron_right": '<path d="M9 5l7 7-7 7"/>',
    "chevron_down": '<path d="M5 9l7 7 7-7"/>',
    "chevron_up": '<path d="M5 15l7-7 7 7"/>',
    "check": '<path d="M4.5 12.5l5 5L19.5 7"/>',
    "plus": '<path d="M12 4.5v15M4.5 12h15"/>',
    "minus": '<path d="M4.5 12h15"/>',
    "wifi": '<path d="M2.6 9.3a13.6 13.6 0 0 1 18.8 0"/><path d="M5.8 12.8a9 9 0 0 1 12.4 0"/><path d="M9 16.2a4.4 4.4 0 0 1 6 0"/><circle cx="12" cy="19.6" r="1.25" fill="currentColor" stroke="none"/>',
    "wifi_2": f'<path d="M2.6 9.3a13.6 13.6 0 0 1 18.8 0" {DIM}/><path d="M5.8 12.8a9 9 0 0 1 12.4 0"/><path d="M9 16.2a4.4 4.4 0 0 1 6 0"/><circle cx="12" cy="19.6" r="1.25" fill="currentColor" stroke="none"/>',
    "wifi_1": f'<path d="M2.6 9.3a13.6 13.6 0 0 1 18.8 0" {DIM}/><path d="M5.8 12.8a9 9 0 0 1 12.4 0" {DIM}/><path d="M9 16.2a4.4 4.4 0 0 1 6 0"/><circle cx="12" cy="19.6" r="1.25" fill="currentColor" stroke="none"/>',
    "lock": '<rect x="5" y="10.5" width="14" height="10" rx="2.5"/><path d="M8.5 10.5V7.5a3.5 3.5 0 0 1 7 0v3"/>',
    "moon": '<path d="M19.5 14.6A8 8 0 0 1 9.4 4.5a8 8 0 1 0 10.1 10.1z"/>',
    "sun": '<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/>',
    "sunrise": '<path d="M3 19h18M6.5 19a5.5 5.5 0 0 1 11 0"/><path d="M12 3.5v6M9.5 6l2.5-2.5L14.5 6"/><path d="M4.4 11.4l1.4 1.4M19.6 11.4l-1.4 1.4"/>',
    "alarm": '<circle cx="12" cy="13" r="7.5"/><path d="M12 9.5V13l2.5 2M3.8 6.6L6.6 4M20.2 6.6L17.4 4"/>',
    "bell": '<path d="M6 16.5V11a6 6 0 0 1 12 0v5.5l1.5 2h-15z"/><path d="M10 21.2h4"/>',
    "settings": '<path d="M4 7.5h8M17 7.5h3M4 16.5h3M12 16.5h8"/><circle cx="14.5" cy="7.5" r="2.5"/><circle cx="9.5" cy="16.5" r="2.5"/>',
    "gear": orbit_gear(),
    "sound": '<path d="M3.5 9.5H7L11.5 5.5v13L7 14.5H3.5z"/><path d="M15 9.2a4 4 0 0 1 0 5.6M17.9 6.4a8 8 0 0 1 0 11.2"/>',
    "sound_off": '<path d="M3.5 9.5H7L11.5 5.5v13L7 14.5H3.5z"/><path d="M15.5 9.5l5 5M20.5 9.5l-5 5"/>',
    "themes": COMMON24["themes"],
    "brightness": '<circle cx="12" cy="12" r="3.2"/><path d="M12 4.5v1.5M12 18v1.5M4.5 12H6M18 12h1.5M6.7 6.7l1 1M16.3 16.3l1 1M6.7 17.3l1-1M16.3 7.7l1-1"/>',
    "globe": '<circle cx="12" cy="12" r="9"/><path d="M3 12h18"/><path d="M12 3c2.4 2.6 3.6 5.6 3.6 9s-1.2 6.4-3.6 9c-2.4-2.6-3.6-5.6-3.6-9S9.6 5.6 12 3z"/>',
    "download": '<circle cx="12" cy="12" r="9"/><path d="M12 7.5v8.5M8.2 12.5l3.8 3.8 3.8-3.8"/>',
    "info": '<circle cx="12" cy="12" r="9"/><path d="M12 11v5.5M12 7.6v.1"/>',
    "clock": '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3.5 2"/>',
    "sync": '<path d="M19.5 12a7.5 7.5 0 1 1-2.6-5.7"/><path d="M19.5 4v4.5H15"/>',
    "trash": '<path d="M4.5 7h15M9.5 7V4.5h5V7M6.5 7l1 13h9l1-13"/>',
    "play": '<path d="M8 5.5v13l10.5-6.5z"/>',
    "stop": '<rect x="6" y="6" width="12" height="12" rx="2.5" fill="currentColor"/>',
    "eye": '<path d="M2 12s3.6-6.5 10-6.5S22 12 22 12s-3.6 6.5-10 6.5S2 12 2 12z"/><circle cx="12" cy="12" r="3"/>',
    "eye_off": '<path d="M2 12s3.6-6.5 10-6.5S22 12 22 12s-3.6 6.5-10 6.5S2 12 2 12z"/><circle cx="12" cy="12" r="3"/><path d="M4 3.5l16 17"/>',
    "backspace": '<path d="M9 5h10.5A1.5 1.5 0 0 1 21 6.5v11a1.5 1.5 0 0 1-1.5 1.5H9l-6-7z"/><path d="M12.5 9.5l5 5M17.5 9.5l-5 5"/>',
    "repeat": COMMON24["repeat"],
    "snooze": COMMON24["snooze"],
    "math": '<path d="M7 4v6M4 7h6M14 7h6M5 15l4 4M9 15l-4 4M14 15h6M14 19h6"/>',
    "restart": COMMON24["restart"],
    "style": COMMON24["style"],
}

# --- Atelier (atelier.html, 24-unit grid) -----------------------------------------------
A = {
    "back": '<path d="M14.5 5.5 8 12l6.5 6.5"/>',
    "close": '<path d="M6.5 6.5l11 11M17.5 6.5l-11 11"/>',
    "chevron_right": '<path d="M9.5 5.5 16 12l-6.5 6.5"/>',
    "chevron_down": '<path d="M5.5 9 12 15.5 18.5 9"/>',
    "chevron_up": '<path d="M5.5 15 12 8.5l6.5 6.5"/>',
    "check": '<path d="M5 12.5l4.5 4.5L19 7.5"/>',
    "plus": '<path d="M12 5v14M5 12h14"/>',
    "minus": '<path d="M5 12h14"/>',
    "wifi": '<path d="M2.8 9.2a13.5 13.5 0 0 1 18.4 0"/><path d="M5.9 12.5a9 9 0 0 1 12.2 0"/><path d="M9 15.8a4.6 4.6 0 0 1 6 0"/><circle cx="12" cy="19.2" r="1.1" fill="currentColor" stroke="none"/>',
    "wifi_2": f'<path d="M2.8 9.2a13.5 13.5 0 0 1 18.4 0" {DIM}/><path d="M5.9 12.5a9 9 0 0 1 12.2 0"/><path d="M9 15.8a4.6 4.6 0 0 1 6 0"/><circle cx="12" cy="19.2" r="1.1" fill="currentColor" stroke="none"/>',
    "wifi_1": f'<path d="M2.8 9.2a13.5 13.5 0 0 1 18.4 0" {DIM}/><path d="M5.9 12.5a9 9 0 0 1 12.2 0" {DIM}/><path d="M9 15.8a4.6 4.6 0 0 1 6 0"/><circle cx="12" cy="19.2" r="1.1" fill="currentColor" stroke="none"/>',
    "lock": '<rect x="5.5" y="10.5" width="13" height="9.5" rx="2"/><path d="M8.5 10.5V8a3.5 3.5 0 0 1 7 0v2.5"/>',
    "moon": '<path d="M19.5 14.6A8 8 0 1 1 9.4 4.5a6.4 6.4 0 0 0 10.1 10.1z"/>',
    "sun": '<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/>',
    "sunrise": '<path d="M3 19h18M6.5 19a5.5 5.5 0 0 1 11 0"/><path d="M12 3.5v6M9.5 6l2.5-2.5L14.5 6"/><path d="M4.4 11.4l1.4 1.4M19.6 11.4l-1.4 1.4"/>',
    "alarm": '<circle cx="12" cy="13" r="7.5"/><path d="M12 9.5V13l2.5 2M3.8 6.6L6.6 4M20.2 6.6L17.4 4"/>',
    "bell": '<path d="M6.5 16.5V11a5.5 5.5 0 0 1 11 0v5.5l1.5 1.5h-14z"/><path d="M10.2 20.5a1.9 1.9 0 0 0 3.6 0"/>',
    "settings": '<path d="M4 7.5h9M17 7.5h3M4 16.5h3M11 16.5h9"/><circle cx="15" cy="7.5" r="2"/><circle cx="9" cy="16.5" r="2"/>',
    "gear": atelier_gear(),
    "sound": '<path d="M4 9.5h3.5L12 6v12l-4.5-3.5H4z"/><path d="M15.5 9.2a4 4 0 0 1 0 5.6M18.2 6.6a7.6 7.6 0 0 1 0 10.8"/>',
    "sound_off": '<path d="M4 9.5h3.5L12 6v12l-4.5-3.5H4z"/><path d="M16 9.5l5 5M21 9.5l-5 5"/>',
    "themes": COMMON24["themes"],
    "brightness": '<circle cx="12" cy="12" r="3"/><path d="M12 5.5V7M12 17v1.5M5.5 12H7M17 12h1.5M7.4 7.4l1 1M15.6 15.6l1 1M7.4 16.6l1-1M15.6 8.4l1-1"/>',
    "globe": '<circle cx="12" cy="12" r="8.5"/><path d="M3.5 12h17M12 3.5c2.6 2.4 3.8 5.2 3.8 8.5s-1.2 6.1-3.8 8.5c-2.6-2.4-3.8-5.2-3.8-8.5s1.2-6.1 3.8-8.5z"/>',
    "download": '<path d="M12 4v11M7.5 10.5 12 15l4.5-4.5M5 19.5h14"/>',
    "info": COMMON24["info"],
    "clock": '<circle cx="12" cy="12" r="8.5"/><path d="M12 7.5V12l3 2"/>',
    "sync": '<path d="M19.2 9A7.6 7.6 0 1 0 19.6 13"/><path d="M19.6 4.4V9h-4.6"/>',
    "trash": '<path d="M4.5 7h15M9.5 7V4.5h5V7M6.5 7l1 13h9l1-13"/>',
    "play": '<path d="M8.5 6v12l9.5-6z"/>',
    "stop": COMMON24["stop"],
    "eye": '<path d="M2.5 12s3.5-6.5 9.5-6.5 9.5 6.5 9.5 6.5-3.5 6.5-9.5 6.5S2.5 12 2.5 12z"/><circle cx="12" cy="12" r="3"/>',
    "eye_off": '<path d="M2.5 12s3.5-6.5 9.5-6.5 9.5 6.5 9.5 6.5-3.5 6.5-9.5 6.5S2.5 12 2.5 12z"/><circle cx="12" cy="12" r="3"/><path d="M4.5 4l15 16"/>',
    "backspace": '<path d="M9 6h11v12H9l-5.5-6z"/><path d="M12 9.5l5 5M17 9.5l-5 5"/>',
    "repeat": COMMON24["repeat"],
    "snooze": COMMON24["snooze"],
    "math": '<path d="M5 7.5h5M7.5 5v5M14 7.5h5M5.5 15l4 4M9.5 15l-4 4M14 15.5h5M14 18.5h5"/>',
    "restart": COMMON24["restart"],
    "style": COMMON24["style"],
}

# --- Instrument (instrument.html, 28 px grid). Icons it does not define come from the
# shared 24-unit drawings, scaled to 28 px with the same 2 px stroke. -----------------------
I28 = {
    "back": '<path d="M17 6.5L9.5 14l7.5 7.5"/>',
    "close": '<path d="M8 8l12 12M20 8L8 20"/>',
    "chevron_right": '<path d="M11 6.5l7.5 7.5L11 21.5"/>',
    "chevron_down": '<path d="M6.5 11l7.5 7.5L21.5 11"/>',
    "chevron_up": '<path d="M6.5 17.5L14 10l7.5 7.5"/>',
    "check": '<path d="M6 14.5l5 5L22 8.5"/>',
    "plus": '<path d="M14 6v16M6 14h16"/>',
    "minus": '<path d="M6 14h16"/>',
    "wifi": '<path d="M4.1 11.1a14 14 0 0 1 19.8 0M7.3 14.3a9.5 9.5 0 0 1 13.4 0M10.5 17.5a5 5 0 0 1 7 0"/><path d="M14 21.2v.1"/>',
    "wifi_2": f'<path d="M4.1 11.1a14 14 0 0 1 19.8 0" {DIM}/><path d="M7.3 14.3a9.5 9.5 0 0 1 13.4 0M10.5 17.5a5 5 0 0 1 7 0"/><path d="M14 21.2v.1"/>',
    "wifi_1": f'<path d="M4.1 11.1a14 14 0 0 1 19.8 0M7.3 14.3a9.5 9.5 0 0 1 13.4 0" {DIM}/><path d="M10.5 17.5a5 5 0 0 1 7 0"/><path d="M14 21.2v.1"/>',
    "lock": '<rect x="7" y="12.5" width="14" height="11" rx="2"/><path d="M10 12.5V9.5a4 4 0 0 1 8 0v3"/>',
    "moon": '<path d="M22.5 16.8A9 9 0 1 1 11.2 5.5a7 7 0 0 0 11.3 11.3z"/>',
    "sun": '<circle cx="14" cy="14" r="4.5"/><path d="M14 3v3M14 22v3M3 14h3M22 14h3M6.2 6.2l2.1 2.1M19.7 19.7l2.1 2.1M21.8 6.2l-2.1 2.1M6.2 21.8l2.1-2.1"/>',
    "sunrise": '<path d="M4 21h20M8 21a6 6 0 0 1 12 0M14 5v6M11 8l3-3 3 3M6 14.5l1.6 1.2M22 14.5l-1.6 1.2"/>',
    "alarm": '<circle cx="14" cy="15" r="8.5"/><path d="M14 10.5V15l3 2M4.5 7l3.5-3M23.5 7L20 4"/>',
    "settings": '<path d="M4 9h5M14 9h10M4 19h10M19 19h5"/><circle cx="11.5" cy="9" r="2.5"/><circle cx="16.5" cy="19" r="2.5"/>',
    "sound": '<path d="M5 11h4l5.5-4.5v15L9 17H5z"/><path d="M18.5 10.5a5 5 0 0 1 0 7M21.5 7.5a9.5 9.5 0 0 1 0 13"/>',
    "sound_off": '<path d="M5 11h4l5.5-4.5v15L9 17H5z"/><path d="M18.5 11l5 6M23.5 11l-5 6"/>',
    "brightness": '<circle cx="14" cy="14" r="3.5"/><path d="M14 6.5v1M14 20.5v1M6.5 14h1M20.5 14h1M8.7 8.7l.7.7M18.6 18.6l.7.7M19.3 8.7l-.7.7M8.7 19.3l.7-.7"/>',
    "globe": '<circle cx="14" cy="14" r="10"/><path d="M4 14h20M14 4c-3 3-4.5 6.5-4.5 10s1.5 7 4.5 10c3-3 4.5-6.5 4.5-10S17 7 14 4z"/>',
    "download": '<path d="M14 4v13M8.5 11.5L14 17l5.5-5.5M5 23h18"/>',
    "info": '<circle cx="14" cy="14" r="10"/><path d="M14 13v7M14 9v.1"/>',
    "clock": '<circle cx="14" cy="14" r="10"/><path d="M14 8.5V14l4 2.5"/>',
    "sync": '<path d="M22.5 14a8.5 8.5 0 1 1-2.5-6"/><path d="M21 3.5V8.5h-5"/>',
    "trash": '<path d="M5.5 8h17M11 8V5h6v3M8 8l1 15h10l1-15M12 12v7M16 12v7"/>',
    "play": '<path d="M10 7.5v13l10.5-6.5z"/>',
    "stop": '<rect x="8" y="8" width="12" height="12" rx="2"/>',
    "eye": '<path d="M3 14s4-7.5 11-7.5S25 14 25 14s-4 7.5-11 7.5S3 14 3 14z"/><circle cx="14" cy="14" r="3.5"/>',
    "eye_off": '<path d="M3 14s4-7.5 11-7.5S25 14 25 14s-4 7.5-11 7.5S3 14 3 14z"/><circle cx="14" cy="14" r="3.5"/><path d="M5 4.5l18 19"/>',
    "backspace": '<path d="M10 7h12a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H10l-6.5-7z"/><path d="M13.5 11l5.5 6M19 11l-5.5 6"/>',
}
I24 = {  # borrowed 24-unit drawings
    "bell": O["bell"],
    "gear": orbit_gear(),
    "themes": COMMON24["themes"],
    "repeat": COMMON24["repeat"],
    "snooze": COMMON24["snooze"],
    "math": O["math"],
    "restart": COMMON24["restart"],
    "style": COMMON24["style"],
}

ICONS = ["back", "close", "chevron_right", "chevron_down", "chevron_up", "check", "plus", "minus",
         "wifi", "wifi_2", "wifi_1", "lock", "moon", "sun", "sunrise", "alarm", "bell", "settings",
         "gear", "sound", "sound_off", "themes", "brightness", "globe", "download", "info", "clock",
         "sync", "trash", "play", "stop", "eye", "eye_off", "backspace", "repeat", "snooze", "math",
         "restart", "style"]


def svg(body, view, stroke, size=SIZE):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 {view} {view}" '
            f'fill="none" stroke="currentColor" stroke-width="{stroke:.3f}" stroke-linecap="round" '
            f'stroke-linejoin="round">{body}</svg>\n')


def write_icons():
    for style in ("instrument", "orbit", "atelier"):
        sets = [(SIZE, OUT / style), (SMALL[style], OUT / style / "sm")]
        if style in LARGE:
            sets.append((LARGE[style], OUT / style / "lg"))
        for size, d in sets:
            d.mkdir(parents=True, exist_ok=True)
            for old in d.glob("*.svg"):
                old.unlink()
            for name in ICONS:
                # Stroke weight stays the same in device pixels at every size.
                if style == "instrument":
                    text = (svg(I28[name], 28, 2 * 28 / size, size) if name in I28
                            else svg(I24[name], 24, 2 * 24 / size, size))
                elif style == "orbit":
                    text = svg(O[name], 24, 2.5 * 24 / size, size)
                else:
                    text = svg(A[name], 24, 1.75 * 24 / size, size)
                (d / f"{name}.svg").write_text(text)
    print(f"icons: {len(ICONS)} per style")


# --- Decorations -------------------------------------------------------------------------
GILT = "radial-gradient(circle at 36% 30%,#FFF6E0 0,#ECD29C 20%,#C89C60 58%,#6E5028 100%)"
RUBY = "radial-gradient(circle at 35% 30%,#FFB0BC 0,#D84860 34%,#8A1E32 100%)"
STEEL = "radial-gradient(circle at 36% 30%,#D8D0C0 0,#9A9080 50%,#4C4640 100%)"


def jewel(d, ruby=True, body=GILT):
    cab = (f"<i style='position:absolute;left:32%;top:32%;width:36%;height:36%;border-radius:50%;"
           f"background:{RUBY};box-shadow:0 0 0 1px rgba(80,40,16,.55)'></i>") if ruby else ""
    return (f"<div style='position:relative;width:{d}px;height:{d}px;border-radius:50%;background:{body}'>"
            f"{cab}</div>")


DECOR = {
    # Atelier: gilt bead with a ruby cabochon (switch on, slider, arcs) and the steel bead (off).
    "atelier/jewel_on_30.png": jewel(30),
    "atelier/jewel_off_30.png": jewel(30, ruby=False, body=STEEL),
    "atelier/jewel_28.png": jewel(28),
    "atelier/jewel_40.png": jewel(40),
    # Atelier: lozenge ornament on the engraved rules (11 px rotated square, 5 px ink moat).
    "atelier/lozenge.png": ("<div style='width:23px;height:23px;display:flex;align-items:center;justify-content:center'>"
                            "<div style='width:11px;height:11px;transform:rotate(45deg);border:1px solid #B89868;"
                            "background:#100C18;box-shadow:0 0 0 4px #100C18'></div></div>"),
    # Instrument: grip ridges on the slide-switch thumb (three 1 px lines at 35 % black).
    "instrument/grip.png": ("<div style='position:relative;width:11px;height:14px'>"
                            + "".join(f"<i style='position:absolute;left:{x}px;top:0;width:1px;height:14px;"
                                      "background:rgba(0,0,0,.35)'></i>" for x in (0, 5, 10)) + "</div>"),
}


async def write_decor():
    from playwright.async_api import async_playwright
    async with async_playwright() as p:
        browser = await p.chromium.launch(channel="chrome")
        page = await browser.new_page(device_scale_factor=1)
        for rel, html in DECOR.items():
            await page.set_content("<html><body style='margin:0;background:transparent'>"
                                   f"<div id='d' style='display:inline-block;line-height:0'>{html}</div></body></html>")
            path = OUT / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            await page.locator("#d > *").first.screenshot(path=str(path), omit_background=True)
        await browser.close()
    print(f"decorations: {len(DECOR)}")


if __name__ == "__main__":
    write_icons()
    asyncio.run(write_decor())
