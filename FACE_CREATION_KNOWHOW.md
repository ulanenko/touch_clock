# Face Creation Know-How

Project-specific notes for adding new clock faces to this firmware without repeating the same mistakes.

This is based on the work done for:

- Digital (the "Digital Vintage" face of earlier notes)
- Sternglas
- the analog faces and the evening faces (Ember, Nocturne, Filament)

The goal is high visual fidelity first, then performance and interaction quality.

## Where Face Code Lives

| What | Where |
|---|---|
| face id and count (`clock_face_id_t`, `CLOCK_FACE_COUNT`) | [main/domain/clock_types.h](main/domain/clock_types.h) |
| name, enabled flag, carousel order | [main/domain/face_catalog.c](main/domain/face_catalog.c) (`s_face_manifest`) |
| face UI state (`clock_ui_face_state_t`) | [main/ui/clock_ui_internal.h](main/ui/clock_ui_internal.h) |
| create and update (`create_*_face`, `update_face`) | [main/ui/ui_faces.c](main/ui/ui_faces.c); the evening faces in [main/ui/ui_faces_evening.c](main/ui/ui_faces_evening.c) |
| declarations | [main/ui/clock_ui_private.h](main/ui/clock_ui_private.h) |
| tile and `create_*_face` call | `build_root_ui` in [main/ui/ui_shell.c](main/ui/ui_shell.c) |
| palettes per theme, the inks the chrome borrows (`ui_face_ink`), the sheet's swatches (`face_theme_preview_colors`) | [main/ui/ui_faces.c](main/ui/ui_faces.c) |
| theme names and preview thumbnails | `theme_name`, `theme_thumb` in [main/ui/ui_face_themes.c](main/ui/ui_face_themes.c); PNGs in `main/ui/assets_src/face/thumbs/f<face>_t<theme>.png` |
| where the night overlay and the alarm banner sit on the face | `night_layout` in [main/ui/ui_face_night.c](main/ui/ui_face_night.c) |
| the face's image for the night face picker | `ui_face_preview_source` in [main/ui/ui_faces.c](main/ui/ui_faces.c) |
| whether its slide snapshot goes stale with the seconds | `face_shows_seconds` in [main/ui/ui_shell.c](main/ui/ui_shell.c) |
| font assets | [main/assets](main/assets), declared in [main/assets/seven_segment_font.h](main/assets/seven_segment_font.h) |
| build wiring for assets | [main/CMakeLists.txt](main/CMakeLists.txt) (`main/assets/*.c` are listed one by one) |

To add a face, you usually need to touch all of those. The full list is in the checklist at the end.

## Recommended Workflow

1. Start from a real reference.
2. Identify what must be exact:
   - font
   - hand geometry
   - tick layout
   - colors
   - texture / glow / mesh treatment
3. Separate static vs dynamic elements before writing code.
4. Get the correct font assets in first.
5. Build the static dial/background.
6. Add only the live parts afterward.
7. If swiping becomes bad, optimize rendering before doing more visual tweaks.

The key design mistake is trying to approximate everything with stock LVGL widgets too early. For high-fidelity faces, the geometry and typography matter more than convenience.

## Fonts

### Use the real font when possible

For high-fidelity faces, do not settle for a close-enough system face if the reference depends on a specific typeface.

What worked:

- DSEG7 Classic Italic for Digital Vintage digits
- DSEG14 Classic Italic for Digital Vintage text
- Jost for Sternglas

### Font conversion

Use `lv_font_conv` to generate LVGL font assets.

Typical pattern:

```bash
npx --yes lv_font_conv \
  --font /path/to/font.ttf \
  --size 34 \
  --bpp 4 \
  --format lvgl \
  --symbols 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ \
  --no-kerning \
  --no-compress \
  --lv-font-name my_font_name \
  -o main/assets/my_font_name.c
```

Important notes:

- generate only the glyphs you need
- `--no-compress` avoided compatibility trouble here
- the generated files may include `#include "lvgl/lvgl.h"`; this project expects `#include "lvgl.h"`, so patch that if needed
- add the new asset to [main/CMakeLists.txt](main/CMakeLists.txt); the simulator picks up every file in `main/assets/` by itself
- export it from [main/assets/seven_segment_font.h](main/assets/seven_segment_font.h) or another shared asset header
- the chrome's fonts are a different set: they are built by `scripts/fonts/build_fonts.py` into `main/assets/skin_fonts/` (see [docs/ui_styles.md](docs/ui_styles.md))

## Static vs Dynamic Split

This is the most important engineering decision for new faces.

### Static elements

These should usually be pre-rendered or cached:

- dial background
- ticks
- numerals
- branding text
- decorative icons
- overlays / mesh / scanlines

### Dynamic elements

These should stay live:

- hands
- seconds
- center pivot if it sits above moving hands
- any alarm/status badge injected on top

If a face has many static objects and only a few moving parts, do not keep the full object tree live.

## Snapshot Caching

### When to use it

Use snapshot caching when:

- the face contains many labels/lines/overlays
- swiping to that face feels slow
- the first render freezes or stalls

This was required for:

- Digital Vintage
- Sternglas dial

### Where it is already used

- Digital: `refresh_digital_face_snapshot` in [main/ui/ui_faces.c](main/ui/ui_faces.c). The live face shows while it
  is idle; the snapshot shows while anything moves over it or it moves (`sync_face_animation_state`).
- Sternglas, Avenir, Modern Silver: the dial is a snapshot, and the hands are drawn into it (below).
- The evening faces: a static base drawn once per theme and day, restored only where something moves.
- LVGL snapshot support is on in [sdkconfig.defaults](sdkconfig.defaults) (`CONFIG_LV_USE_SNAPSHOT`).

### Important caveat

RGB565 snapshots can show ugly gradient banding. If that happens:

- flatten the background color
- reduce soft gradients
- avoid very subtle large-area fades in snapshot-backed faces

That exact issue happened on Digital Vintage.

## Canvas vs Live LVGL Objects

### Prefer live objects when

- the face is simple
- the moving parts are only lines and circles
- the face already performs well

### Prefer canvas/layer drawing when

- the hand shape must match a reference exactly
- you need polygons or composite shapes
- a shadow should look painted rather than widget-like

Sternglas hands are a good example:

- generic `lv_line` hands looked wrong
- the proper fix was to use the exact reference polygon geometry
- those polygons are drawn straight into the face's RGB565 canvas, over the dial snapshot

The analog faces share this machinery (`face_hands_*` in [main/ui/ui_faces.c](main/ui/ui_faces.c)): the face
is one RGB565 canvas holding the dial snapshot with the hands drawn in. A tick restores the dial only where
the hands were and are now (`face_hands_begin`), then draws the hands there (`face_hands_finish`). An
earlier version cleared a full-screen ARGB hands canvas and snapshotted the face every second: about
110 ms a tick on the device. Only one face shows at a time, so all these faces share one 1 MB view
buffer; a face that finds another owner in it repaints in full.

## Reusing Reference Geometry

If the reference gives you exact coordinates, use them.

For Sternglas, the original HTML polygons were:

- minute hand: `198,225 202,225 202,55 200,45 198,55`
- hour hand: `197,220 203,220 203,115 200,105 197,115`

The right approach was:

1. keep those points in reference-space
2. rotate them around the original center
3. map them into screen-space with one consistent scale function

This is much better than trying to infer the shape from screenshots.

## Scaling Strategy

For HTML/SVG-style references, define one mapping function instead of scattering magic numbers everywhere.

Sternglas uses:

- `STERNGLAS_SCALE`
- `STERNGLAS_OFFSET`
- `sternglas_map(...)`
- `sternglas_rotate_point(...)`

That makes it much easier to:

- fit the dial to the round panel
- remove unnecessary frame/background area
- retune overall scale later

If the face does not fully fill the screen cleanly, fix the mapping function first, not fifty individual coordinates.

## Gesture / Swipe Performance

New faces must be judged not only by looks but also by how they behave during face-to-face swipes.

### Known pitfalls

- many live labels on a face can make swiping jerky
- rotated labels are especially expensive
- live decorative layers can make a face feel frozen during transitions

### How face changes work now

- A sideways drag does not scroll the tileview. The face slide (`face_shell_slide_*` in
  [main/ui/ui_shell.c](main/ui/ui_shell.c)) moves snapshots of the two faces with the finger, masked to
  discs once when taken, so a frame is two image blits whatever the face draws.
- The neighbours' snapshots are taken ahead while the face is idle: again each minute, and every 5 s for
  faces in `face_shows_seconds`. A drag starts at once.
- The tileview has momentum off and one face at a time; it only jumps to the new face when the slide ends.
- So a face's cost during a change is the cost of its snapshot, taken at idle. What still matters is its
  tick (every second) and its first full draw after the slide.

If a new face is visually correct but makes the clock stutter, treat that as a rendering architecture
problem, not a UX problem. The perf tour's `CONFIG_TOUCH_CLOCK_PERF_TOUR_FACE_COSTS` times every face's
first frames and ticks on the device ([docs/development.md](docs/development.md#perf-tour-on-the-clock)).

## Shadows and Glow

### Avoid literal duplicate-shape shadows

A single hard offset copy often looks fake, especially for hands.

What worked better:

- a few low-opacity layers
- smaller offsets
- softer stacking instead of one big dark copy

### Avoid expensive blur/filter ideas first

True blur effects are tempting, but on-device performance matters more than browser-like fidelity. Start with layered low-opacity shapes. Only go heavier if the face can afford it.

## Round-Screen Fit

For circular displays, a reference that looks good in a square browser mockup may need one extra pass to:

- fill the visible circle better
- avoid showing dead background corners
- keep bottom or side content out of the clipped edge

Do not assume the first scale that looks right in code will feel right on the device.

## Asset / Build Checklist

When adding a face, check all of these:

- add the id in [main/domain/clock_types.h](main/domain/clock_types.h) before `CLOCK_FACE_COUNT`. Ids are saved
  in NVS: add new ones at the end and never renumber. `face_themes[]` in the settings grows with the count;
  the loader copes with settings saved with fewer faces
- add its entry in [main/domain/face_catalog.c](main/domain/face_catalog.c), enabled; the position in the
  enum is its place in the carousel
- add its state to `clock_ui_face_state_t` in [main/ui/clock_ui_internal.h](main/ui/clock_ui_internal.h)
- write `create_<face>_face` and its update in [main/ui/ui_faces.c](main/ui/ui_faces.c) (or
  [main/ui/ui_faces_evening.c](main/ui/ui_faces_evening.c)), declare it in
  [main/ui/clock_ui_private.h](main/ui/clock_ui_private.h), and add it to the `update_face` switch
- call it in `build_root_ui` in [main/ui/ui_shell.c](main/ui/ui_shell.c)
- three themes: its palettes, its `ui_face_ink` and `face_theme_preview_colors` cases in
  [main/ui/ui_faces.c](main/ui/ui_faces.c), its names and thumbnails in
  [main/ui/ui_face_themes.c](main/ui/ui_face_themes.c). Render the thumbnails in the simulator at 10:09:32, put
  them in `main/ui/assets_src/face/thumbs/f<id>_t<theme>.png`, and run
  `python3 scripts/ui_assets/build_ui_assets.py face`
- a `ui_face_preview_source` case, so the night face picker can show it
- a `night_layout` case in [main/ui/ui_face_night.c](main/ui/ui_face_night.c) if the night overlay or the alarm
  banner would cover its numerals, date or logo
- `face_shows_seconds` in [main/ui/ui_shell.c](main/ui/ui_shell.c) if it shows seconds
- add new font sources to [main/CMakeLists.txt](main/CMakeLists.txt) and export them from the shared asset header
- update the face list in [docs/architecture_invariants.md](docs/architecture_invariants.md) and the
  [README](README.md), and the `TOUCH_CLOCK_PERF_TOUR_START_FACE` range in
  [main/Kconfig.projbuild](main/Kconfig.projbuild)
- iterate in the simulator (`set face <id>`, `set face_theme <n>`, `shot`; see
  [docs/development.md](docs/development.md#simulator)), check every style and language there
- then do a full build, flash and test on hardware

## Hardware Testing Checklist

For each new face, test:

- first render latency
- left/right slide responsiveness, and the first frame after sliding onto it
- the theme sheet (pull down) and quick actions (pull up from the bottom) still work over it
- the night overlay and the alarm banner sit clear of its details
- no weird background/frame is visible
- typography matches the reference
- hands are proportionally correct
- shadows/glow feel natural rather than obvious
- no clipping near the round edge

## Good Defaults For Future Faces

- build the face in [main/ui/ui_faces.c](main/ui/ui_faces.c), or [main/ui/ui_faces_evening.c](main/ui/ui_faces_evening.c) for a dim one
- keep static art cached when possible
- keep dynamic parts minimal
- use the real font
- use exact reference geometry if available
- optimize only after the look is correct
- if performance breaks, snapshot the static layer instead of simplifying the design too early

## Lessons Learned

### Digital Vintage

- exact font choice mattered a lot
- a custom-looking face with many layered labels became too heavy live
- snapshot caching was the right fix
- gradient banding in RGB565 snapshots is real

### Sternglas

- proper typography mattered
- exact hand geometry mattered more than expected
- live dial composition was too expensive for swipe performance
- cached dial + live hands is the right split
- shadows need restraint or they read like duplicated hands
