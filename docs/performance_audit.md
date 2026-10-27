# UI Performance Audit (2026-10-26)

Status of the clock, what was measured, what is fixed, what is still open, and the
on-device test plan. Findings marked *unverified* come from code review only and
still need a measurement.

## Current state (2026-10-27)

On the clock (and in `build/`, ELF `dc9c0668e`):
- `d7e42dc` plus the MAC fix, the Other-menu and theme-sheet fixes, and the low-risk per-tick fixes
- `TRIPLE_FULL` display mode
- LVGL style cache and shadow cache
- panel pixel clock 48 MHz, via the vendored BSP in `components/`
- the 2026-10-27 fixes below (face reveal, button pressed style, Night preview, settings save bug, smooth theme sheet dimming, PPA blending, snapshot backdrop for the pull sheets)

The perf tour is not in this firmware.

Dependencies:
- `main/idf_component.yml` now pins `esp_hosted` to 2.12.0 and `esp_wifi_remote` to 1.4.0.
- With `"*"` and a git-ignored lock file, a reconfigure had silently pulled `esp_hosted` 3.0.10. That no longer matches the 2.12.0 firmware on the C6 Wi-Fi coprocessor (`major version mismatch` at boot).

## Root cause of the slow "Other" scrolling (measured, fixed)

- In `TRIPLE_PARTIAL` mode LVGL drew each frame as 15 strips of 50 rows and walked the
  whole object tree for every strip. On the UI core, about 30% of time went to style lookups,
  20% to per-object draw events, and under 1% to actual pixel blending.
- Other has about twice the objects of Night (57 vs 28, 33 vs 11 labels), so it cost about 2x per frame.
- Fix: `TRIPLE_FULL` (one pass per frame) plus `CONFIG_LV_OBJ_STYLE_CACHE`. Animated
  scrolling of Other went from 45 ms to 20 ms per frame (about 16 to 32 fps); Night unchanged at about 20 ms.
- Ruled out: memory placement of objects, fonts, cache line size, CPU stolen by other tasks.
- Side effect: every refresh now redraws the whole screen, so anything that
  invalidates often (animations, per-second style sets) costs a full frame.

## White flash while scrolling repeatedly (fixed)

Reproduced by hand on a monitor build:
- At 80 MHz, several white flashes happened on the Night and Other pages.
- In the same recording, no LVGL-rendered frame was white and no DSI underrun was logged. So the frames were correct, and the panel side was failing.

The BSP drives the panel at `dpi_clock_freq_mhz = 80` (about 131 Hz) straight from PSRAM. With `TRIPLE_FULL` rendering into those same PSRAM frame buffers, that is too much.

At 48 MHz (about 79 Hz):
- There were no flashes in a 4-minute manual stress test.
- Every frame renders 13-15% faster (perf tour: Other scroll 20.4 -> 17.5 ms, idle 28.6 -> 24.5 ms, theme pull 43 -> 37 ms).

The BSP is vendored in `components/esp32_p4_wifi6_touch_lcd_xc` (see `VENDORED.md`) and used through `override_path`.

Not usable on this board: the adapter's `DOUBLE_FULL` and `DOUBLE_DIRECT` modes hang (`DOUBLE_DIRECT` stops refreshing about 8 s after boot, `DOUBLE_FULL` stalls during boot).

## Perf tour results (48 MHz, after fixes)

| step | render avg ms | p95 ms | notes |
|---|---|---|---|
| idle face (Matrix) | 24.5 | 25 | full redraw every second |
| face tap (affordance fade) | 27.7 | 39 | 14 slow frames per tap |
| face swipe (reveal) | 35 | 123 | circular `clip_corner` reveal, worst step |
| theme pull / close | 37 / 34 | 50 | |
| brightness pull | 38 | 40 | 70% scrim over the live face |
| settings Wi-Fi / Other scroll | 17 / 17.5 | 20 | smooth |
| settings Night scroll | 24.6 | 76 | face-preview re-render at every scroll end |
| alarms scroll | 16 | 22 | smooth |
| alarm editor open | 32.5 | 48 | |

## Fixed 2026-10-27 (on the clock)

| Step (perf tour) | Before | After |
|---|---|---|
| Face swipe (avg / p95) | 35 / 123 ms | 27 / 39 ms |
| Night page scroll (avg / p95) | 24.6 / 75.6 ms | 17.3 / 18.4 ms |
| Night page slow drag (avg / p95) | 21.7 / 73.5 ms | 17.3 / 18.4 ms |
| Night page open | 20.5 ms | 18 ms |
| Other / Wi-Fi / alarms scroll | 17-18 / 17 / 15-17 ms | unchanged |

- **Face swipe reveal.** It no longer uses a `clip_corner` parent, which allocated two ARGB layers up to 1 MB each per frame. A `DRAW_MAIN` callback now draws a centred crop of the target face's snapshot with `clip_radius`. The crop is a sub-buffer view with no copy and is 4-byte aligned. The panel is round, so above 720 px it draws unmasked.
- **Pressed buttons.**
  - The default theme's pressed style uses `recolor`, which LVGL applies to the button and its whole subtree on every frame the finger is down.
  - Dragging a page from a large button (Night page cards) made every frame about 74 ms.
  - A small theme extension (`ui_install_theme_overrides`) gives every `lv_button` a pressed style with recolor off and `opa` 70% instead. It has no layer and the same dim look.
  - A `color_filter` shade was tried and rejected: overlay pages slowed down over time with it.
- **Night preview.**
  - The live `clip_corner` preview circle is never drawn on screen (`opa_layered` 0, still in the layout). The pre-masked snapshot is always shown.
  - The snapshot is re-taken when the face or enabled state changes, or at the end of a scroll if the minute changed. It used to be re-taken at every scroll end.
- **Settings save storm (pre-existing bug).**
  - `app/app_settings_storage.c` never included `nvs.h`, so on the device its "not found" constant was `ESP_ERR_NOT_FOUND`. NVS returns `ESP_ERR_NVS_NOT_FOUND`.
  - Erasing the absent legacy key therefore "failed" every save, and the core re-ran the whole save every 2 s forever on the UI task.
  - Now each change saves once. A save takes about 2.6 ms on the UI task (measured), so saves stay synchronous.
  - Moving them to a background task was tried. It made overlay rendering about 2x slower and getting worse over time (cause not found), so it was rejected.
  - Failed NVS writes are now logged with the key name.
- **Theme sheet dimming.** The face dims smoothly with the sheet again. An earlier fix had limited the dimming to 8 levels, which showed as visible steps. That saved almost nothing, because every frame is a full redraw anyway. Measured on the same face: theme pull average 37.6 -> 39.3 ms, p95 unchanged at about 50 ms. The bottom quick-actions sheet was already continuous; it now only sets the dimming when the value changes.
- **Hardware blending (PPA).** `main/ui/ui_ppa_blend.c` registers an LVGL RGB565 blend handler. Large full-width fills and RGB565 image blends (face images, screen backgrounds, scrims, sheet snapshots) run on the P4's PPA; everything else stays on the CPU.
  - Measured per full 720x720 frame (CPU -> PPA): fill 7.0 -> 3.0 ms, 70% fill 13.4 -> 10.6 ms, image copy 14.3 -> 11.8 ms, 50% image 34.5 -> 11.7 ms. PPA's SRM copy is slower (22 ms), so copies use blend.
  - Only cache-line-aligned whole rows go to the PPA, so both software draw units stay on. The adapter's own PPA option needs a single draw unit; that made the face swipe 2.4x slower. Its cache sync is also wrong for partial-width blocks.
- **Pull sheets draw a dimmed snapshot.** While the theme sheet or the quick-actions sheet moves, the face is a snapshot drawn with image opacity over the overlay's opaque black background. Before, it was the live face under a translucent scrim. LVGL then starts each frame at the overlay and skips the face and the scrim blend. The face holds still during the movement. The snapshot is taken when the movement starts, and costs one render.

| Step (perf tour, same face) | Before | PPA + snapshot backdrop |
|---|---|---|
| Theme pull (avg / p95) | 39.3 / 50.0 ms | 20.5 / 26.7 ms |
| Theme close | 36.7 / 50.2 ms | 18.4 / 26.4 ms |
| Brightness pull | 37.2 / 38.9 ms | 19.2 / 29.1 ms |
| Idle face | 24.4 ms | 15.2 ms |
| Face swipe | 36.6 / 64.6 ms | 27.4 / 46.7 ms |
| Settings scroll (Wi-Fi / Night / Other) | 17.2 / 17.4 / 17.4 ms | 12.3 / 12.5 / 12.7 ms |
| Alarms scroll | 16.8 ms | 13.0 ms |

- **Dependencies.** `esp_hosted` is pinned to 2.12.0 and `esp_wifi_remote` to 1.4.0 (see Current state).

## Fixed overnight (now on the clock)

- `sync_update_button` and `set_action_button_text` only call `lv_label_set_text` when the text changed. Before, this ran every second, plus the alarm overlay buttons.
- `update_dots` only sets a colour when it changed.
- `sync_alarm_banner_style` only re-applies its 12 styles when light/dark changes. It runs on every face change and every settings action.
- `sync_night_controls` re-applies the card and button visuals only when Night mode is toggled. Before, every Night slider step re-styled four cards (with `border_width`, so the whole page re-laid-out).
- `refresh_settings_controls` (run on every slider `VALUE_CHANGED`) only syncs Night when `night_controls_need_sync()` says so.
- `CONFIG_LV_DRAW_SW_SHADOW_CACHE_SIZE=64`: the banner and button shadows were re-blurred every frame.

## Open findings, by priority (*unverified* unless measured)

### High
- **Analog faces render twice per second** (canvas redraw + `lv_snapshot_take_to_draw_buf`, `ui_faces.c` ~376-412). Sternglas redraws every second for about 0.5 px of movement.
- **Matrix and Wharton redraw the whole canvas every second** (1089 and about 202 rect draws) when only 2-3 dots change.
- **Digital face sets about 22 styles per second unconditionally** (`ui_faces.c` ~925-1008). It also has two invisible glow labels.
- **Alarm list re-styles every card at scroll end** (`ui_alarms.c` `refresh_alarm_management_layout`, `update_alarm_list_focus_treatment`).

- **Night face picker re-renders all 8 previews on every open.**

### Medium
- Extra snapshots during `set_active_face` and reveal end: `lv_tileview_set_tile` fires `SCROLL_BEGIN`/`END` synchronously, which triggers `refresh_digital_face_snapshot`.
- The theme sheet's first pull after a face change still takes the panel snapshot plus a digital snapshot in one frame. Pre-take them on an idle timer.
- Affordance fade in/out is about 15 full frames per tap. Show/hide instantly or in 2 steps.
- The alarm volume slider starts swipe-close animations for every empty slot on each drag step. The alarm settings slider and switch have no drag passthrough.
- The swipe-to-delete animation changes width and `translate_x`, which re-lays out the list every frame.
- The brightness panel and Wi-Fi dialog use a 70% scrim, so the live page underneath is redrawn every frame (keyboard cursor blink included). The keyboard keys use the light theme.
- `CONFIG_LV_DRAW_THREAD_PRIO=3` is below `ota_task`, `wifi_scan` and `alarm_audio` (4).
- Synchronous `clock_ui_refresh` (refresh_settings_controls + update_brightness_ui + `set_active_face`) runs on every settings action, including slider steps.

### Low
- The Wi-Fi list is rebuilt with `lv_obj_clean` on every scan.
- Dropdown lists are re-styled on every open (12 styles, including fonts).
- Theme transitions (80 ms) add 4-5 full frames per press.
- Every surface has 4 transparent edge sensors.
- Wi-Fi RPCs run on the LVGL task.
- Every click writes the codec volume over the I2C bus shared with touch.

## On-device test plan

Run with the IDF Python (it has pyserial), clock connected over USB, nobody touching it.
Each run takes about 4 minutes:

```bash
PY=~/.espressif/python_env/idf5.5_py3.12_env/bin/python
$PY scripts/perf_tour.py --image baseline
$PY scripts/perf_tour.py --image after_fixes
$PY scripts/perf_tour.py --image after_fixes_dpi48 --restore
$PY scripts/perf_tour.py --compare perf_reports/<baseline-dir> perf_reports/<after-dir>
```

Then:

1. Check the thumbnails (`thumb_*.png`) to confirm the tour reached each screen.
2. Read the underrun count and white frames in each `report.md`.
3. With `after_fixes_dpi48` flashed, scroll the settings pages repeatedly by hand while
   capturing the serial log, to see whether the flash still happens and whether underruns are logged.
4. Turn the measured wins into the normal firmware, rebuild `build/`, and flash.

## Perf tour tooling

- `CONFIG_TOUCH_CLOCK_PERF_TOUR` (default off; never ship it) enables `main/ui/ui_perf_tour.c`.
- 25 s after boot, a virtual touch input plays face taps and swipes, theme pulls, the
  brightness pull, settings flick bursts and slow drags, and the alarm list and editor.
- It prints one `PERF seg` line per step: render avg/p95/max, layout time, longest frame interval, slow frames and white frames.
- `scripts/perf_tour.py` builds it in `build_perf/` from the local `sdkconfig`, with the
  console on USB-Serial-JTAG (the UART console is too slow for thumbnails).
- It flashes, captures, and writes `perf_reports/<time>-<label>/` (`report.md`,
  `summary.json`, thumbnails). `--restore` flashes the untouched `build/` artifacts back.
