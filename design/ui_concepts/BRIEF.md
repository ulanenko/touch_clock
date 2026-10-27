# Touch Clock UI redesign brief

The clock faces stay exactly as they are. Everything around them is redesigned:
- the sheets
- settings
- alarms
- dialogs
- the small controls shown on the face

## The device

- **Display:** a 720x720 px **round** IPS panel. Only the inscribed circle (diameter 720, centre 360,360) is visible; the corners do not exist for the user. The largest square fully inside the circle is about 509 px (from 105 to 615).
- **Placement:** a bedside clock about 50–80 cm from the eyes, with a capacitive touch screen. It is used mostly in the dark, with brightness turned low, and by sleepy fingers.
  - Tap targets: at least 72 px.
  - Body text: at least 22 px, labels at least 18 px.
  - Contrast must also hold at 10% backlight.
- **Platform:** ESP32-P4, LVGL 9.3, rendered to RGB565 (16-bit colour). Gradients band slightly; keep them subtle and short.

## What LVGL can and cannot do well here

These constraints keep the designs buildable and smooth on the device. Treat them as rules.

**Do:**
- solid fills
- rounded rectangles of any radius
- 1–2 px borders and hairlines
- arcs and rings (`lv_arc`)
- lines
- simple linear and radial gradients on a few elements
- text in any TTF/OTF font, converted to bitmaps at fixed sizes. Pick sizes deliberately; there is no free scaling.
- static images: icons, textures, pre-rendered decorations as PNG
- one soft shadow on a few elements (cached)
- opacity on single objects

**Avoid:**
- backdrop blur or frosted glass of live content
- `backdrop-filter`
- masks or clipping of whole containers
- opacity on whole groups
- per-frame transforms such as scaling list items by position
- mix-blend modes
- large live shadows on many items
- text on a curved path, unless it is a pre-rendered static image (fine for fixed titles)

A frosted look is allowed only as a static pre-rendered image, or as a flat translucent colour over a dimmed snapshot.

When a sheet slides over the face, the face below is dimmed by drawing it at reduced opacity over black. Any dim level from 0 to about 80% is fine; you may pick a tint colour instead of black.

## Fonts

Any open-licence font (Google Fonts) is fine, but it **must cover Latin Extended and Cyrillic**: the UI ships in English, Polish, Russian and Ukrainian. Good options:
- sans: Inter, Inter Tight, Manrope, Onest, Golos Text, IBM Plex Sans, Jost, Montserrat, Rubik, Unbounded
- mono: IBM Plex Mono, JetBrains Mono
- serif: Cormorant Garamond, Playfair Display, Lora, PT Serif, Literata

The faces already use DSEG, Jost, Avenir and Helvetica Neue.

Icons: draw simple SVG line icons yourself, consistent stroke and size per direction. Do not use emoji.

## Screens to design, in this order

Each one is a full 720x720 round screen. Use plausible, real content (English).

1. **Face, controls shown.** After a tap on the face, the controls appear for a few seconds:
   - page dots for 8 faces, the current one marked
   - a hint for pulling down (themes) and pulling up (quick actions)
   - the entry to settings, and to alarms if you choose to place it there
   - a small "Update available" badge
   - the alarm "next up" chip

   Use face screenshot `faces/face_5.png` (Sternglas, white) or `faces/face_0.png` (digital, dark). Show both if useful.
2. **Theme sheet** (pulled down from the top). Title "Face themes", for the current face. Three themes (Theme 1–3), each with a name and 3 colour swatches; one is selected. A preview of each theme is a nice touch: small round thumbnails using the face PNG with a tint.
3. **Quick actions** (pulled up from the bottom over the dimmed face):
   - brightness control (current 40%)
   - buttons: Alarms, Night mode on/off, Settings
   - optionally a Do-not-disturb or sound toggle
4. **Settings home.** Three sections, each with a subtitle:
   - Wi-Fi: "Networks, clock, and time zone"
   - Night mode: "Schedule, face, and brightness"
   - Other: "Language, sound, and device info"

   Show a status line such as "Wi-Fi: Home-5G • synced 14:13".
5. **Wi-Fi.**
   - connected network "Home-5G" with signal strength
   - list of networks: Home-5G, Home-2G, Neighbours, Café Lumière, iPhone (Anna)
   - Scan button
   - Time zone "Warsaw, Berlin (UTC+1)"
   - Clock: "Auto sync" on, "Sync time" action
6. **Join network dialog.** Network "Café Lumière", password field with a hidden password and a show/hide toggle, Cancel and Connect. The on-screen keyboard is optional; if you show one, fit it to the circle.
7. **Night mode.**
   - Enabled switch (on)
   - Schedule 22:30 → 07:00
   - Night face: preview thumbnail of a dark face (`faces/face_1.png`), "Tap to choose"
   - Brightness 12%
   - Sunrise ramp, on: "Brighten during the pre-alarm sunrise"
8. **Other.**
   - Language: English
   - Touch sound: on, with a volume
   - Software update: current 1.8.2, available 1.9.0, Install
   - About this device:
     - Device ID TC-4F2A91
     - Version 1.8.2
     - Build Oct 27 2026
     - Target ESP32-P4
     - ESP-IDF v5.5.3
9. **Alarms list.**
   - "Next up: Tomorrow 06:30, in 8 h 12 min"
   - alarms:
     - 06:30, Weekdays, on
     - 07:45, Weekends, on
     - 09:00, Once (Sat 1 Nov), off
   - Add alarm
   - an entry to alarm settings (snooze 9 min, volume, sound "Morning Light", ascending)
10. **Alarm editor.**
    - Time 06:30, as a large scroll wheel or digits plus steppers
    - Repeat: Once / Every day / Weekdays / Weekends, plus individual days M T W T F S S
    - Sound "Morning Light" with a Test button
    - Math to turn off, switch
    - Delete / Save
11. **Alarm ringing.**
    - full screen, the time 06:30
    - "Alarm"
    - big Snooze (9 min) and a slide or press-and-hold to stop
    - optional math challenge variant: "7 + 15 = ?" with a number pad, fitting the circle
12. **Night mode active.** The night face (`faces/face_1.png`) shown at a very dim level, with a minimal night overlay: for example, the next alarm and the time until it. This shows how the design behaves in the dark.

## Deliverable

One standalone HTML file: `design/ui_concepts/<slug>.html`. Images load by relative path (`faces/face_N.png`); fonts from Google Fonts. It contains:
- **A header:** the direction's name, a one-paragraph rationale (what it is inspired by and why it suits a bedside round clock), then a compact system specimen:
  - colour tokens with hex values
  - type scale with font names and px sizes
  - radii
  - a control set: button, switch, slider, list row, chip, segmented control
- **The 12 screens** in a grid, each a real 720x720 element showing:
  - a circular mask (`border-radius: 50%; overflow: hidden`)
  - a thin dark bezel ring around it to look like the device
  - a caption below

  Scale the grid view with a CSS `transform` (for example 0.55), and let a click on a screen open it at 1:1 in an overlay.
- **Light and dark:** dark device UI. The page around the screens can follow `prefers-color-scheme`, but the screens themselves are the device.
- **Sizes:** everything at true device pixel sizes inside the 720 frame, so sizes can be carried over to LVGL as they are.

Quality bar: an Apple or top industrial design studio presentation. That means:
- strict grid and spacing scale
- optical alignment to the circle
- consistent iconography
- real hierarchy
- no generic web-dashboard look
- nothing touching the bezel edge awkwardly

Content near the top and bottom of the circle must curve with it: rows get narrower towards the edges. Long scrolling pages may show the cut-off at the bottom edge, as on a watch.

Check your work visually. Render screenshots with headless Chrome:

    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --hide-scrollbars --force-device-scale-factor=1 --window-size=1600,5200 --screenshot=/path/out.png file:///abs/path/to/file.html

Then look at them with the Read tool and fix what looks off: misalignment, clipping by the circle, cramped spacing, inconsistent icons. Iterate until it is genuinely excellent.
