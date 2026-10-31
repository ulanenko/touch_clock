# UI styles: how the chrome is built

The clock has four interface styles, chosen in Settings › Other › Interface style
(`clock_ui_style_t` in [main/domain/clock_types.h](../main/domain/clock_types.h); Orbit is the default):

- **Instrument:** Braun / Teenage Engineering, graphite with one orange accent.
- **Orbit:** round-native, watchOS-like.
- **Atelier:** luxury horology, ink and gilt.
- **Liquid Glass:** Orbit's layouts, with every surface drawn as glass over the face (see [Liquid Glass](#liquid-glass) below).

The clock faces are the same in every style. Everything around them (the "chrome") is drawn in the current style:
- controls over the face
- sheets
- settings
- alarms
- dialogs

The design reference for every screen is in `design/ui_concepts/`:
- `instrument.html`, `orbit.html`, `atelier.html`: system specimen and screens
- `compare/<style>/NN.png`: 720×720 renders of the 12 shared screens (Instrument, Orbit, Atelier)
- `BRIEF.md`: the screen list and constraints

Liquid Glass has no design file of its own: it takes Orbit's screens. The brief's "avoid frosted glass of
live content" still holds in spirit: the glass is baked once, not blurred per frame.

## Structure

| Part | Files | What it does |
|---|---|---|
| Core | `clock_ui.c`, `ui/ui_chrome.c`, `ui/ui_l10n.c`, `ui/ui_format.c` | boot, 1 s tick, chrome lifecycle, debug calls, strings, shared formatting |
| Faces | `ui/ui_faces.c`, `ui/ui_faces_evening.c`, `ui/ui_sunrise.c`, face parts of `ui/ui_shell.c` | the faces, the face slide, the sunrise layers |
| Kit | `ui/kit/ui_skin.*`, `ui/kit/ui_kit.*`, `ui/kit/ui_kit_controls.c`, `ui/kit/ui_kit_text.c`, `ui/kit/ui_kit_dial.c` | design tokens per style, shared components, controls, text, cached dials |
| Glass | `ui/kit/ui_glass.*` | Liquid Glass: backdrop, bakes, the hook that draws chrome objects as glass |
| Face chrome | `ui/ui_shell.c`, `ui/ui_face_controls.c`, `ui/ui_face_sheet.c`, `ui/ui_face_themes.c`, `ui/ui_brightness.c`, `ui/ui_face_night.c`; state in `ui/ui_face_chrome_state.h` | controls over the face, theme sheet, quick actions, night overlay |
| Settings | `ui/ui_settings.c`, `ui/ui_settings_wifi.c`, `ui/ui_settings_night.c`, `ui/ui_settings_other.c`; state in `ui/ui_settings_state.h` | settings home, Wi-Fi, join, night mode, other |
| Alarms | `ui/ui_alarms.c`, `ui/ui_alarms_list.c`, `ui/ui_alarms_editor.c`, `ui/ui_alarms_timeset.c`, `ui/ui_alarms_ring.c`; state in `ui/ui_alarms_state.h` | banner, list, alarm settings, editor, ringing |

The core and the areas talk only through `ui/ui_areas.h`. The chrome rules:

- **Under the chrome root.** Every chrome object is created under `ui_chrome_root(ctx)`.
- **Timers.** Every chrome timer is created with `ui_chrome_timer_create()`.
- **Animations.** Use a chrome object as the anim var.
- **Buffers.** Free draw buffers and allocations in the area's `*_destroy()` hook.

A style change then deletes the root, zeroes the area state structs and builds everything again in the new style (`ui_chrome_rebuild_async`). If settings was open, it returns to the same settings page. The faces are not rebuilt.

## Building a screen

1. Use kit parts: pages, groups, rows, switches, sliders, buttons, chips, segmented controls, rollers, the keyboard and dialogs. Take colours and fonts from `ui_skin()`. Never hard-code a colour or font in an area. For bare containers use `kit_plain()` (no theme styles, not clickable, not scrollable), and show or hide with `kit_show()`, which only touches the object when the state changes; don't add area-local copies of these.
2. **Where styles differ in layout,** branch on `ui_skin_style()` (or `ui_skin_is_instrument()`, `ui_skin_is_orbit()`, `ui_skin_is_atelier()`) inside the area. Liquid Glass reports `CLOCK_UI_STYLE_ORBIT` here, so it gets Orbit's layout for free; check `ui_skin_is_glass()` only where glass needs something Orbit does not. Examples:
   - the alarm editor's time setter: drums, ring plus rollers, or watch rings
   - the ringing screen's stop control
   - the brightness arc

   Keep the logic shared: one callback that changes the value, several ways to draw it.
   Details drawn on the face itself (the night alarm row, the snooze banner) are the
   exception to the skin rule: they take the face's inks from `ui_face_ink()` and sit in
   the face's clear spot from `ui_face_chrome_alarm_slot_y()`, so they read as part of the
   face in every style.
3. **Strings:**
   - Existing ones: `ui_l10n_text(ctx, L10N_...)`.
   - New ones are declared inline next to their use: `static const ui_l10n_str_t s_x = UI_L10N_STR("en", "pl", "uk", "ru", "nl");` and `ui_l10n_str(ctx, &s_x)`.
   - Bind kit labels that must follow a language change with `ui_l10n_bind_kit()` / `ui_l10n_bind_kit_str()`.
4. **Simulator entry points:** register a named debug call for each screen and state (`ui_debug_register("open_wifi", fn)`), so the simulator and the perf tour can open it.

## Performance rules (measured on the device)

- **Redraws:** every frame redraws the full screen, so keep per-frame work small.
- **No layers:**
  - no style `opa` on containers
  - no `clip_corner`
  - no transforms, filters or `recolor` on pressed states (buttons dim to 70% opacity)
- **Shadows:** at most one per screen; shadows are cached.
- **Images:** RGB565 or A8. Full-width opaque images and fills go to the PPA, so they are cheap.
- **Sheets over the face:** draw a dimmed snapshot over an opaque overlay while they move (`ui_backdrop_set`). They must not redraw the live face.
- **Ticks:** only touch a label or style when its value changed. `kit_label_set_text_if_changed()`, `kit_switch_set()` and `kit_show()` help. Setting a style invalidates the object even when the value is the same.
- **Arcs and rings:** never `lv_draw_arc` or an `lv_arc` widget's own drawing. A software arc costs the device 4–14 ms however small, and its task covers the whole circle. Use cached dials (`kit_dial_draw`, `kit_draw_ring`, `kit_arc_bake`) or, for a one-off, `kit_draw_arc`.
- **Touch targets:** at least 72 px (`ext_click_area` when the visual is smaller).

## Liquid Glass

Liquid Glass is a skin (`s_glass` in [main/ui/kit/ui_skin.c](../main/ui/kit/ui_skin.c)) plus a drawing pass
([main/ui/kit/ui_glass.c](../main/ui/kit/ui_glass.c)).

- **Marker colours.** The glass skin's surface tokens (`bg`, `well`, `surface`, `sheet`, `raised`) are not
  real colours. They are near-black values nothing else uses. `ui_glass_attach()` on the chrome root hooks
  every object under it: one painted with a marker colour, and every coloured button or bead, draws as glass
  of that kind instead (clear, raised, well, sheet or tinted). So an area written for Orbit with skin tokens
  becomes glass with no code of its own. Never use these tokens as colours for anything else.
- **Backdrop.** When glass comes up, the face is snapshotted and turned into the frost (blurred, lifted, a
  little more saturated) and, for pages, the dim (the face darkened: a page's ground). The frost is rebuilt
  only when the face, its minute or its theme changes.
- **Bakes.** Each shape is baked once at its place into an RGB565 image: the backdrop refracted through a
  convex squircle rim (Snell's law, with a little dispersion), frosted, tinted, lit at the rim. A frame then
  draws one image per shape. Bakes are cached (6 MB, 96 shapes, 2 MB of PSRAM left free) and split across
  both cores.
- **Prewarm.** The face's idle tick bakes the controls, the quick sheet and the pages ahead, under a 20 ms
  budget a tick, so opening them does not wait.
- **Cost.** A Glass page scrolls at about 20-28 ms a frame against Orbit's 11-13 ms (memory bandwidth for
  the images). The numbers and their history are in [performance_audit.md](performance_audit.md).
- **New screens:** use skin tokens for surfaces as in Orbit, register a debug call, and check the screen in
  the simulator with `set style 3` (bakes, the page entrance, scrolling).

## Simulator

`sim/` runs the real UI on the desktop with the project's LVGL and sdkconfig, and writes 720×720 PNGs:

```bash
cmake -S sim -B sim/build && cmake --build sim/build -j8
mkdir -p sim/out && sim/build/sim sim/scenarios/smoke.txt sim/out
```

`set style <n>` switches style (0 Instrument, 1 Orbit, 2 Atelier, 3 Liquid Glass). Every scenario command,
`set` key and debug call is listed in [development.md](development.md#simulator). Configure a build with
`-DSIM_ASAN=ON` to run under AddressSanitizer and UBSan.

## Assets

- **Fonts:** `scripts/fonts/build_fonts.py` builds the bitmap fonts in `main/assets/skin_fonts/` (Latin, Latin Extended, Cyrillic). Add a size there; don't hand-edit the generated files.
- **Icons and images:** `scripts/ui_assets/build_ui_assets.py <area>` converts `main/ui/assets_src/<area>/**.svg` into tintable A8 icons and `.png` into RGB565(A8) images (`main/assets/ui_assets/<area>_assets.*`).
