# UI styles: how the chrome is built

The clock has three interface styles, chosen in Settings › Other › Interface style:

- **Instrument:** Braun / Teenage Engineering, graphite with one orange accent.
- **Orbit:** round-native, watchOS-like.
- **Atelier:** luxury horology, ink and gilt.

The clock faces are the same in every style. Everything around them (the "chrome") is drawn in the current style:
- controls over the face
- sheets
- settings
- alarms
- dialogs

The design reference for every screen is in `design/ui_concepts/`:
- `instrument.html`, `orbit.html`, `atelier.html`: system specimen and screens
- `compare/<style>/NN.png`: 720×720 renders of the 12 shared screens
- `BRIEF.md`: the screen list and constraints

## Structure

| Part | Files | What it does |
|---|---|---|
| Core | `clock_ui.c`, `ui/ui_chrome.c`, `ui/ui_l10n.c`, `ui/ui_format.c` | boot, 1 s tick, chrome lifecycle, strings, shared formatting |
| Faces | `ui/ui_faces.c`, face parts of `ui/ui_shell.c` | the faces, swipe and reveal |
| Kit | `ui/kit/ui_skin.*`, `ui/kit/ui_kit.*` | design tokens and shared components per style |
| Face chrome | `ui/ui_shell.c`, `ui/ui_brightness.c`, state in `ui/ui_face_chrome_state.h` | controls over the face, theme sheet, quick actions, night overlay |
| Settings | `ui/ui_settings.c`, state in `ui/ui_settings_state.h` | settings home, Wi-Fi, join, night mode, other |
| Alarms | `ui/ui_alarms.c`, state in `ui/ui_alarms_state.h` | banner, list, alarm settings, editor, ringing |

The core and the areas talk only through `ui/ui_areas.h`. The chrome rules:

- **Under the chrome root.** Every chrome object is created under `ui_chrome_root(ctx)`.
- **Timers.** Every chrome timer is created with `ui_chrome_timer_create()`.
- **Animations.** Use a chrome object as the anim var.
- **Buffers.** Free draw buffers and allocations in the area's `*_destroy()` hook.

A style change then deletes the root, zeroes the area state structs and builds everything again in the new style (`ui_chrome_rebuild_async`). If settings was open, it returns to the same settings page.

## Building a screen

1. Use kit parts: pages, groups, rows, switches, sliders, buttons, chips, segmented controls, rollers, the keyboard and dialogs. Take colours and fonts from `ui_skin()`. Never hard-code a colour or font in an area.
2. **Where styles differ in layout,** branch on `ui_skin_style()` inside the area. Examples:
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
- **Ticks:** only touch a label or style when its value changed. `kit_label_set_text_if_changed()` and `kit_switch_set()` help. Setting a style invalidates the object even when the value is the same.
- **Arcs and rings:** never `lv_draw_arc` or an `lv_arc` widget's own drawing. A software arc costs the device 4–14 ms however small, and its task covers the whole circle. Use cached dials (`kit_dial_draw`, `kit_draw_ring`, `kit_arc_bake`) or, for a one-off, `kit_draw_arc`.
- **Touch targets:** at least 72 px (`ext_click_area` when the visual is smaller).

## Simulator

`sim/` runs the real UI on the desktop with LVGL 9.3 and the project's sdkconfig, and writes 720×720 PNGs:

```bash
cmake -S sim -B sim/build && cmake --build sim/build -j8
sim/build/sim sim/scenarios/smoke.txt sim/out
```

The scenario commands are documented at the top of `sim/sim_main.c`:
- `wait`, `tap`, `drag`, `press`/`move`/`release`
- `call <debug call>`
- `set <key> <value>`, for example `set style 2`
- `clock HH:MM:SS`
- `shot <name>`

Configure a build with `-DSIM_ASAN=ON` to run under AddressSanitizer.

## Assets

- **Fonts:** `scripts/fonts/build_fonts.py` builds the bitmap fonts in `main/assets/skin_fonts/` (Latin, Latin Extended, Cyrillic). Add a size there; don't hand-edit the generated files.
- **Icons and images:** `scripts/ui_assets/build_ui_assets.py <area>` converts `main/ui/assets_src/<area>/**.svg` into tintable A8 icons and `.png` into RGB565(A8) images (`main/assets/ui_assets/<area>_assets.*`).
