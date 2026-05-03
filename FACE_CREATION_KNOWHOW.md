# Face Creation Know-How

Project-specific notes for adding new clock faces to this firmware without repeating the same mistakes.

This is based on the work done for:

- Digital Vintage
- Sternglas

The goal is high visual fidelity first, then performance and interaction quality.

## Where Face Code Lives

- face enum and face count: [main/clock_model.h](main/clock_model.h)
- face display names: [main/clock_model.c](main/clock_model.c)
- face UI state: [main/clock_ui.c](main/clock_ui.c)
- face creation/update logic: [main/ui/ui_faces.c](main/ui/ui_faces.c)
- face registration in carousel: [main/ui/ui_shell.c](main/ui/ui_shell.c)
- font/image assets: [main/assets](main/assets)
- build wiring for assets: [main/CMakeLists.txt](main/CMakeLists.txt)

To add a face, you usually need to touch all of those.

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
- add the new asset to [main/CMakeLists.txt](main/CMakeLists.txt)
- export it from [main/assets/seven_segment_font.h](main/assets/seven_segment_font.h) or another shared asset header

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

- Digital Vintage snapshot path in [main/ui/ui_faces.c](main/ui/ui_faces.c)
- LVGL snapshot support enabled in:
  - local `sdkconfig`
  - [sdkconfig.defaults](sdkconfig.defaults)

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
- those polygons are now drawn on a transparent ARGB canvas

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

### What already helped in this project

- tileview configured for one-face-at-a-time movement
- momentum disabled for face swipes
- Digital Vintage has a dedicated transparent swipe layer because normal tileview handling was not good enough
- snapshotting the heavy face content

If a new face is visually correct but makes swipes bad, treat that as a rendering architecture problem, not a UX problem.

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

- add enum in [main/clock_model.h](main/clock_model.h)
- add display name in [main/clock_model.c](main/clock_model.c)
- add state fields in [main/clock_ui.c](main/clock_ui.c) if needed
- add create/update functions in [main/ui/ui_faces.c](main/ui/ui_faces.c)
- register the face in [main/ui/ui_shell.c](main/ui/ui_shell.c)
- add new asset sources to [main/CMakeLists.txt](main/CMakeLists.txt)
- export font symbols from the shared asset header
- build the `clock_ui.c` object first for quick iteration
- then do a full build
- then flash and test on hardware

## Hardware Testing Checklist

For each new face, test:

- first render latency
- left/right swipe responsiveness
- swipe-up quick actions still work
- no weird background/frame is visible
- typography matches the reference
- hands are proportionally correct
- shadows/glow feel natural rather than obvious
- no clipping near the round edge

## Good Defaults For Future Faces

- build the face in [main/ui/ui_faces.c](main/ui/ui_faces.c)
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
