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

## Fixed 2026-10-27/28 (measured on the clock)

- **Analog faces (Sternglas, Avenir, Modern Silver)** cleared a 640 px ARGB hands canvas and snapshotted the whole tile every second: about 110 ms of UI thread a tick. The face is now one RGB565 canvas, the dial snapshot with the hands drawn in. A tick restores the dial only where the hands were and are now, and redraws them there (`face_hands_begin`/`face_hands_finish` in `ui_faces.c`). A tick now costs 10-24 ms, and that worst case is the first full draw after a swipe in. It also frees three 1.6 MB hands canvases.
- **Matrix and Wharton** redraw and invalidate only the dots that changed (`face_dot`, `dot_canvas_finish`).
- **Digital** re-applies its styles only when the palette changes.
- **Alarm sounds** are synthesised (`alarm_tones.c`) block by block in an 8 KB internal-RAM mixing buffer, and pre-rendered by a lowest-priority task 20 s after boot (about 3.3 s of work, 3 MB of PSRAM). A preview or an alarm starts at once.
- **Settings pages at 80-140 ms a frame, on whichever screen happened to be open (LP RAM heap).** `CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP` added the P4's LP RAM to the general heap. When L2 memory ran low, LVGL's small per-draw buffers came from it: the row masks of rounded rectangles, and glyph and mask buffers. Every pixel through them was about 20x slower. A 500x160 card with radius 44 took 17.4 ms when a 500-byte `malloc` landed in LP RAM, and 0.9 ms otherwise (measured side by side on the clock). Which pages were slow depended on the heap state at the time, so it looked random across runs and styles. LP RAM is no longer heap. Small allocations fall back to PSRAM, which is cached and fast, and the 32 KB internal DMA reserve is unchanged.
- **LVGL clip_radius mask leak.** LVGL 9.3.0 never frees the radius mask of an image drawn with `clip_radius` (`lv_draw_sw_img.c`). The face reveal draws one a frame with a growing radius, so each frame pins a circle-cache entry for good. Once all 64 are pinned, every reveal frame also leaks a temporary entry. `cmake/lvgl_patches.cmake` applies the one-line fix at configure time. It writes a fixed copy of the file into the build directory and compiles it instead of the managed original, for the firmware and the simulator alike, so a fresh LVGL download keeps the fix. It warns if the source no longer matches, and stands down once LVGL ships the fix.
- **Atelier settings (30 -> 15-18 ms).** A one-frame timeline of draw tasks on the clock showed why Atelier was slow. LVGL draws each frame as two horizontal halves, one per draw unit, and our decoration callbacks queued every ring tile in both halves: 155 tasks. LVGL's main thread spent its time building and dispatching them, while one draw thread did nearly all the drawing. The changes:
  - Ring tiles outside the half being drawn are not queued.
  - Kit tiles register their rows' runs of visible pixels (`ui_sparse_image_register`). The RGB565 blend handler touches only those pixels, and fully transparent ARGB pixels otherwise. The Atelier rim's tiles cover about 125k pixels for about 4k visible ones.
  - Since area is now nearly free and each image task costs about 70 us of LVGL setup, small rings are one tile and large ones 8 (the rim takes about 1 MB).
  - Atelier's round borders are cached rings instead of LVGL's per-row masked borders (`kit_ring_border`): keys (border and outline in one dial) and the row-icon sub-dials, now one object.
  - The theme sheet's rim is the kit's cached minute rim instead of a live 712 px arc and 60 line ticks.
  - The blend handler runs the same CPU code in the simulator, where every screen of the three styles stays pixel-identical (ring edges differ by at most 41/255 where LVGL borders became kit rings).
- **Atelier time-set dial.** The dial's guilloche grooves, rim, minute track and bezel hairline were 55 live LVGL arcs, built when the editor first opened (about 30 ms for the 712 px circle alone). They are now drawn per pixel straight into the dial canvas, weighted to match LVGL's lighter hairlines. The dial is also built 3 s after the chrome, so the editor never waits for it. The build still takes about 100 ms on the clock (numerals and ticks), but while the face is idle.
- **Kit arcs and dials** are cached tiles instead of live `lv_draw_arc` (4-14 ms of mask setup each on the device).

## Fixed 2026-10-28 (measured on the clock)

- **Atelier hung the clock once PSRAM ran out (task watchdog in both draw tasks).** Atelier started with about 1.3 MB of PSRAM free, and the settings scroll took the rest. Three causes, all fixed:
  - The settings scroll register drew its moving thumb with the kit ring cache, which built and kept a ~115 KB ring image for every scroll position (24 cache entries, about 3 MB, plus a ring build on scroll frames). The thumb is now an LVGL arc; the track stays cached. Settings scrolling also got faster: 11-15 ms a frame in Instrument and Orbit.
  - Each hand-drawn face (Sternglas, Avenir, Modern Silver and the three evening faces) kept its own 1 MB view canvas. Only one face shows at a time, so they now share one (`face_hands_*` in `ui_faces.c` tracks the owner; a face that lost it repaints in full). The evening faces also share one base buffer. Together about 7 MB.
  - The theme and quick sheets share one panel snapshot buffer, and the slide's snapshot of the face it starts on lives in the sheet backdrop buffer (they never run at the same time).
- **Face slide.** A sideways drag now moves the faces with the finger as two discs (`face_shell_slide_*` in `ui_shell.c`), replacing the circular reveal on release. The snapshots are masked to the disc once when taken, so a frame is two plain image blits and a black gap (18-19 ms a frame; `clip_radius` on every frame cost 26-27 ms). The neighbours are snapshotted ahead while the face is idle (again each minute, every 5 s for faces with seconds), so a drag starts at once. The settle is a critically damped spring that starts at the finger's speed. A release counts as a flick by the drag's average speed when the drag was short, and the finger counts as resting only when touch reads saw it still: a busy UI thread no longer turns a flick into a snap-back.
- **Evening faces.** `ev_fill` filled the 720x720 base through the per-pixel blend (three divides a pixel): Ember's base took 150-180 ms, now about 45 ms. Nocturne's moon (a shaded sphere with maria and craters, about 30k samples) is cached per theme and day, so retaking the shared base copies it.
- **Face controls** come in as a sprung sweep round the dial (about 580 ms, only the moving controls and the rim redraw) and fade out in 230 ms.

## Liquid Glass (2026-10-28/29)

The fourth style draws chrome surfaces as glass (`main/ui/kit/ui_glass.c`). Each shape is baked once at its place into an RGB565 image: the face behind it refracted through a convex squircle bezel (Snell's law, n = 1.5, the three channels bent slightly differently), mixed with a half-resolution frost, tinted and lit at the rim. A frame then draws one image per shape; rows not inside other glass bake their corners against the page ground and skip LVGL's per-pixel corner mask. Sheets and large panels (over 100 k px), and glass sitting on them, use a deep frost instead (blurred again at 90x90, about 30 px at full size) with no sharp face mixed in: they read as material.

Measured on the clock (glass tour 20261028-213305, before the changes below):

- Frames: face 15 ms, face slide 20-22 ms, settings scrolling 23-29 ms (Orbit 11-15 ms).
- Backdrop: about 138 ms (downsample 13, blur 70, upscale 40, back 15). The frost is rebuilt only when the face, its minute or its theme changes (idle time); going from the face to a page only dims the back image in place.
- Bakes: about 0.4-0.9 us a pixel: a page's rows about 200 ms the first time a page opens for a face, the theme sheet panel about 270 ms. Bakes on pages and large panels stay valid while the face and theme are the same.

Since then (simulator cost model only, to confirm on the clock):

- Blur: interleaved RGBA, rows with four sums, columns a row at a time with a sum per byte, edges from index tables: bit-identical, 2.4x (-O1) to 4.75x (-O2) faster on the host. Upscale: the fixed (3 near + far) / 4 weights as shifts and adds in 16-bit lanes. The backdrop debug call (snapshot and frost) is estimated at ~100 ms, down from ~168 with the deep frost; the frost build alone at ~37 ms, the rest being the face snapshot render.
- Bakes carry their own context (page or face, from their ancestry), so page glass is baked while the face is showing: the face's idle timer (every 250 ms when nothing is touched) prewarms the chrome's glass for up to 20 ms a tick, into free cache room only (a prewarm never evicts). Opening a page should no longer wait for its rows.
- Boot intro (`main/ui/ui_boot.c`): estimated 32-39 ms a frame during the iris (one large arc over the face), about 26-30 fps for 1.1 s, once per boot.

Deferred: the frost build on a worker task (it is mostly idle-time work now, and the snapshot it needs renders on the UI thread anyway) and the settings scroll's full-screen ground blit (about 12 ms of a 23-29 ms frame; cutting it needs occlusion bookkeeping against the opaque rows). Revisit with the numbers from the device.

### Liquid Glass against Orbit (2026-10-29, tour 20261029-133342, avg / max ms)

| Step | Glass before | Glass now | Orbit |
|---|---|---|---|
| Face swipe | 21.9 / 26.9 | 22.1 / 27.9 | 22.7 / 28.1 |
| Theme sheet pull | 24.5 / 33.2 | 15.9 / 17.8 | 18.5 / 35.2 |
| Quick sheet pull | 32.8 / 38.1 | 28.4 / 36.6 | 22.7 / 39.3 |
| Settings open (rows not baked ahead) | 36.2 / 190 | 33.7 / 146 | 10.4 / 10.4 |
| Night page open | 60.7 / 255 | 30.1 / 170 | 13.5 / 13.5 |
| Page scroll | 21-27 / 260 | 20-25 / 45-90 | 11-13 / 17 |

What made the difference: the page ground drawn only where no opaque glass covers it; bakes over
a page's smooth ground at a quarter of the colour work (row bakes 38 -> 23 ms); rows baked
ahead seeing the frosted ground they stand on (they were baked again on every open); settled
shapes baked again one a frame; the face behind a moving sheet not drawn under its body.

Then (tour 20261029-145512): bakes split between the two cores (the UI thread the top half,
a worker task the bottom; the other core is mostly idle while a page opens), and corner pixels
past the rim on the flat-top path. A 500 x 88 row bakes in 11 ms (was 20); a page opened right
after a face change 105-115 ms (was 146-178), settings reopened 81 ms, alarms 97 ms.

Scrolling rows (2026-10-29, tour 20261029-164612): a page's glass row kept its baked image while
it scrolled, so the blurred face inside it moved with the row and jumped back into place when the
row was baked again where it stopped. A scrolling row now draws its flat top from a screen-fixed
page ground (the same colours as its baked top, built into the sheet ground's buffer as the page
opens: about 125 ms of work in slices, 4 ms while the entrance plays, 16 ms after) and only its rim
and corners from its bake. Night page scrolling 24.0 -> 27.8 ms a frame (p95 31.7 -> 34.3): the
row is eight pieces of its bake and one of the ground instead of one image.

Still open: a page opened right after the face changed still bakes its rows then; a scrolling
page draws its ground and rows as images (about 20-25 ms a frame against Orbit's fills, about
13 ms), which is memory bandwidth, not work that can be skipped.

Pressing and stopping (29 Oct, evening). A pressed Glass row baked a raised version of itself on
the frame the finger landed (11-22 ms on the clock, a frame of 50-70 ms), and every row a scroll
moved was baked again 120 ms after it stopped, one a tick, on the frames of the next flick. A
pressed row now draws its clear bake under the raised tint (what the raised body is; the tint a
half again, as RGB565 rounds a light blend down), and a moved page row, already drawn right where
it is, is baked again only once nothing has been touched for 600 ms. The tour's alarm "scroll"
was also opening the first alarm: a list too short to scroll took a swipe across a row as a tap
on release. Alarm flicks on the clock: 13 frames over 33 ms (max 68) -> none (max 21.7).

Sliders (29 Oct). The whole UI refresh each slider step asks for costs under 0.1 ms (it sets only
what changed), and the display redraws the whole frame anyway (`TRIPLE_FULL`: 518 k pixels a
frame, now `inv_kpx_avg` in the tour), so a drag costs what its screen costs: the Night
brightness slider 12.6 ms a frame, the quick panel's brightness arc 36.9. What each step did
waste: every settings change applied the time zone again and restarted SNTP (a time request to
the network per slider step); both now go only when they change. The glass sheets' rim no
longer draws its sides and the middle of its bands through the radius mask: quick panel drag
36.9 -> 34.1 ms. Still open: under the quick panel the face is drawn whole each frame, though
the panel's ground covers most of it.

Night face picker (29 Oct). Opening it drew all eight thumbnails at once: 665 ms on the clock
the first time (call time, now `PERF call` in the tour), and again on every open. A thumbnail is
now keyed by what it shows (the minute, the face's theme, the language) and drawn again only
when that changes: the chosen face's at once, the rest one a tick while the picker shows
(`PERF face_thumb`: 10-70 ms each). First open 665 -> 43 ms, reopen 2 ms. Filament's thumbnail
was 324 ms: the three evening faces share one base buffer, so its preview drew its base again,
four identical glass tubes per pixel. It now draws one and copies it (and takes the mesh from
an 8 x 7 table): 68 ms, the same pixels.

### Checking Liquid Glass on the clock

1. Glass tour against the 20261028-213305 baseline:
   `$PY scripts/perf_tour.py --port /dev/cu.usbmodem1301 --restore --set CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLE_FIRST=3`,
   then `--compare` with the baseline. Look at `PERF glass_step` (down / blur / up: blur was 70 ms, up 40)
   and `PERF glass_bake` lines, and whether bakes now land while the face is idle rather than when a page opens.
2. Boot (normal firmware): the ring fills as the faces build, the iris opens smoothly, the chime plays once
   at the UI click volume, and stays silent with UI sounds off or in night mode (`night_now`).
3. Pull the theme and quick sheets slowly: frosted, no face detail through them or their keys, no noise
   at the screen's edge.
4. After a minute idle, tap the face and open settings: no stall before the controls or the page.

## Stability soak (2026-10-29, tour 20261029-151343)

16 tour loops over all four styles (Glass, Instrument, Orbit, Atelier, four times each),
about 30 minutes of continuous taps, swipes, sheets, pages and the alarm editor:

- No resets, panics or asserts.
- PSRAM: no leak. Free PSRAM at the same step of each loop holds steady (Glass idle face
  1.3-1.8 MB, Instrument 7.5 MB); Glass at its lowest (the night face picker) keeps
  435-526 KB.
- **Atelier lost about 750 KB more in loops 11 and 15** (free at its lowest: 1137 KB in loops 3
  and 7, then 386, then 276). The cause was `kit_arc_bake`, which cached a lv_arc's indicator in the
  kit dial cache like its track. The indicator's end follows the value, so every minute shown on the
  alarm time dial kept a ring of its own, 0.3-1 MB of ARGB tiles at 344 px, up to 24 of them.
  Dragging the minute wheel in Atelier (about 1.1 MB free) could have run PSRAM out, as in the
  hang fixed on 10-28. Indicators are now drawn live (`kit_draw_arc`, bounded to the arc), and the
  dial cache keeps to 6 MB, dropping its least recently drawn dials (never one drawn in the last
  100 ms). Atelier's dials come to about 5.1 MB in the simulator and no longer grow with the time.
- Internal RAM: at least 24.9 KB free throughout.
- Stacks (`PERF stack`, the least free each task ever had): the UI thread (`lvgl`, the
  adapter's default 8 KB) came down to **504 bytes**, `alarm_audio` to 964, `sys_evt` to 956;
  the others kept 1.2 KB or more (`glass_bake` 5.4 KB of 6 KB).

Fixed from it: the UI thread's stack is now 10 KB and `alarm_audio`'s 5 KB. The simulator's
ASAN run found a use-after-free in Liquid Glass: a page's entrance animation kept moving its
controls after a style change deleted the page (`enter_root_deleted_cb` now stops it).
Corner optics tables are capped at 480 KB, and a face control whose smoked shell can't be
allocated is drawn as a plain rounded rectangle instead of nothing.

Idle prewarm order (Glass): the settings home and the alarm list are baked first, then the
rest of the chrome, under one 20 ms budget a tick.

## Atelier frame time (2026-10-30, tours 20261029-221406 -> 20261030-100118, avg ms)

| step | before | after |
|---|---|---|
| Other page scroll | 19.7 | 15.2 |
| face swipe with the controls up | 36.1 (27 frames over 33 ms) | 25.7 (none) |
| tap to the controls | 35.9 (12 over) | 26.6 (4 over) |
| settings close | 35.7 (24 over) | 26.1 (4 over) |
| alarms close | 36.2 (8 over) | 26.5 (3 over) |

A tour that scrolled the Other page with one part off at a time (20261030-093259) put the minute
rim at 3.4 ms a frame and the scroll register at 2.7 ms (the band painting out the ticks under
it 1.0, the track 0.6, the thumb about 1.1), although they blend only a few thousand pixels.
What they pay for is PSRAM misses: each run of visible pixels is a miss in the tile and another
in the frame buffer.

- **The controls' bezel** (a 72 px band at 90 %, about 146k pixels blended over the face on
  every frame the controls show) is opaque. The sparse blender keeps opaque runs of one colour
  of 8 px or more as runs of their own and fills them without reading the tile or the screen.
  The bezel no longer shows the dial's rim faintly through it.
- **Sparse tiles keep their blended pixels packed** one run after another, so blending reads
  them in order instead of missing on a cache line per run (1 MB of rim tiles for about 4k
  visible pixels).
- **The register's thumb** was a plain `lv_draw_arc`, whose task covers the whole 688 px circle
  and holds up the rest of the frame; it is `kit_draw_arc` now.
- **Atelier's minute rim leaves a gap under the register** instead of the register painting the
  ticks out. A rim with a gap is the plain rim's tiles less those round the gap, plus those tiles
  baked again without the gap's ticks (`kit_dial_t.tiles`, `kit_dial_draw_tiles`): 258 KB for
  the settings gap and 517 KB for the theme sheet's, where each was a whole 1 MB rim. A first try
  with whole gapped rims took the dial cache over 6 MB, and closing settings evicted the bezel and
  rebuilt it (163 ms); Atelier's dials now come to 4.9 MB. The alarm screens drew a second whole
  rim to mean "no gap" (skip 999); they draw the plain one.

## Liquid Glass quick sheet and pages (2026-10-30, tours 20261030-100118 -> 20261030-111421)

Measured in Glass with the short tour, now 15 s of idle face first and 5 s before the quick
sheet (as at the bedside: the idle prewarm needs that long after a boot or a style change).
"Again" is the tour's second loop, the pages built and prewarmed.

| step | before | after |
|---|---|---|
| quick sheet open (first frame) | 288 ms | 31 ms |
| brightness drag | 35.8 ms, every frame over 33 | 30.9 ms, none |
| Night page open, again | 139 ms | 24 ms |
| Other page open, again | 124 ms | 24 ms |
| Other scroll, again: worst frame while touching | 64 ms | 33 ms (the rebake at rest still costs one 60 ms frame) |

- **The quick sheet's glass was never prewarmed**: closed, it sits below the screen, and the idle
  prewarm skips what is off it. Its first frame baked the panel (64 ms), the sheet ground (92 ms)
  and the four keys (104 ms). `face_sheet_prewarm` moves the panel to where it opens and back
  inside its hidden overlay (nothing is redrawn) and bakes it there, first in the idle prewarm and
  making room for itself (`ui_glass_prewarm_first`); the pages take what is left.
- **At rest the quick sheet keeps the face as the backdrop snapshot** (`freeze_face`), drawn only
  around the panel, instead of the live face drawn whole under it on every frame of a drag. The
  snapshot is taken again when the face changes (a minute, or a second on faces that show them;
  `ui_backdrop_refresh`).
- **Settings prewarms while a page sits still**: the other pages already built, and the rows of
  this page below the fold, baked where they come into view at the end of its scroll
  (`ui_glass_prewarm_at`). Those are marked as baked ahead and give way first.
- **The bake cache thrashed**: more glass shapes than its 48 slots, and the prewarm only checked
  bytes, so prewarms evicted each other round and round. It has 96 slots and 4 MB; a prewarm
  needs a free slot as well, and leaves current bakes alone (touching them made them look
  recently drawn, so the cache dropped what was actually in use). 4.6 MB took internal RAM's
  low point from 25 KB to 13 KB (medium buffers spill into it when PSRAM runs short), so it stays
  at 4 MB; PSRAM's low point in the tour is 1.5 MB.
- **Page glass that moved bakes again only once nothing is touched or gliding**, cards too (rows
  already did): a card baked ahead that stops part-way through a slow drag no longer bakes inside
  the drag.
- The first visit to each page after a boot still bakes in its first frame (about 110 ms):
  pages are built on first open.

## Memory and page opens (2026-10-30, full Glass tours 20261030-112240 -> 20261030-115445)

- **PSRAM ran down to 119 KB over one full tour** (theme sheet, settings, Wi-Fi, the alarm
  editor): the bake cache grew to its fixed 4 MB whatever else needed, and shapes whose bake
  could not be allocated fell back to translucent boxes (slow, and wrong). The cache now also
  leaves 1 MB of PSRAM free (`bake_budget`, dropped bakes awaiting their frame counted as free),
  and a closed pull sheet gives its 1 MB snapshot buffer back. Low point over two loops: about
  0.8-1 MB. Other page scrolling 32.4 -> 27.6 ms.
- **The settings pages opened most are built ahead** (Home, Night, Other; one per idle tick from
  20 s after a boot), so the idle prewarm bakes them: the first settings open 111 -> 36 ms,
  Other 116 -> 25 ms.
- **Every page opened from the face waited 85 ms** for the page backdrop (the frost dimmed, a
  full-screen image): it is now worked out once per 2 x 2 block and dithered per pixel (identical
  output), 41 ms; the open waits 56 ms.
- **The face controls are prewarmed first**, before the quick sheet and the pages.
- **LVGL's draw buffers come from PSRAM** (under 16 KB they came from internal RAM).
- Still open: Night page scrolling about 29-32 ms (the moving rows' nine draw tasks each, for the
  ground that stays put); alarms open (97 ms) and the Night page and style picker on a second
  visit (about 110-125 ms) bake rows evicted under the PSRAM floor; internal RAM dips to about
  7 KB during the first network burst after a boot (update check), with the settings pages built.
  The 3 MB of pre-rendered alarm sounds is the largest PSRAM user left.
- **Alarm sounds (same day): only the sounds the enabled alarms use stay rendered**
  (`alarm_audio_keep_sounds`, told by the core when the alarms change). Others are rendered when
  previewed (Wind chimes 2.2 s, Marimba 0.4 s, Singing bowl 0.8 s on the clock) and freed when
  the preview stops. With the alarms on the recorded tone, PSRAM free after boot 4.3 -> 7.4 MB;
  the full tour's low point 0.8 -> 2.4 MB.

## Open findings, by priority (*unverified* unless measured)

### High
- **Atelier settings take about 15 ms a frame** (Instrument and Orbit: 11-15 ms). What remains is about 75 draw tasks (each image task costs about 70 us of LVGL setup), the 164 px gradient scrim (about 2.5 ms), pill borders (about 0.7 ms each) and the rim and register (about 4 ms, mostly PSRAM misses along the ring).
- **Atelier screens with the face controls up take about 26 ms a frame** (Glass about 22 ms).


### Medium
- Extra snapshots during `set_active_face` and reveal end: `lv_tileview_set_tile` fires `SCROLL_BEGIN`/`END` synchronously, which triggers `refresh_digital_face_snapshot`.
- The theme sheet's first pull after a face change still takes the panel snapshot plus a digital snapshot in one frame. Pre-take them on an idle timer.
- Affordance fade in/out is about 15 full frames per tap. Show/hide instantly or in 2 steps.
- The alarm volume slider starts swipe-close animations for every empty slot on each drag step. The alarm settings slider and switch have no drag passthrough.
- The swipe-to-delete animation changes width and `translate_x`, which re-lays out the list every frame.
- The brightness panel and Wi-Fi dialog use a 70% scrim, so the live page underneath is redrawn every frame (keyboard cursor blink included). The keyboard keys use the light theme.
- `CONFIG_LV_DRAW_THREAD_PRIO=3` is below `ota_task`, `wifi_scan` and `alarm_audio` (4).

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
- It prints one `PERF seg` line per step: render avg/p95/max, layout time, longest frame interval, slow frames and white frames, and a `PERF segx` line with the flush wait, PPA time and free PSRAM.
- `CONFIG_TOUCH_CLOCK_PERF_TOUR_FACE_COSTS=y` instead shows every face in turn and times its first frames and its ticks; `CONFIG_TOUCH_CLOCK_PERF_TOUR_START_FACE` picks the face each loop starts on.
- `scripts/perf_tour.py` builds it in `build_perf/` from the local `sdkconfig`, with the
  console on USB-Serial-JTAG (the UART console is too slow for thumbnails).
- It flashes, captures, and writes `perf_reports/<time>-<label>/` (`report.md`,
  `summary.json`, thumbnails). `--restore` flashes the untouched `build/` artifacts back.

## Simulator cost model (estimates without the clock)

The simulator draws what the clock draws, but its timings say nothing about the clock's. The cost
model estimates them:

- `estimate [label]` in a sim scenario counts one full redraw of the current screen by draw-task
  class (opaque, full-width and clipped RGB565 images, dense and sparse ARGB, plain, rounded and
  translucent fills, text, borders, shadows, arcs, lines, layers, transforms) and prints the
  estimated clock render time with the biggest contributors, e.g.
  `estimate atelier_settings: border 1.4 label 3.6 argb_sparse 8.5 tasks 48 2.1 | 19.0 ms`.
- `timecall <debug call>` runs a debug call (opening a page, building something) and scales its
  host CPU time to the clock (`cpu_scale`, about 20x): the UI-thread stall it would cause.
- The coefficients (`sim/cost_model.txt`) come from `ui_microbench.c`'s scenes timed on the clock
  (`CONFIG_TOUCH_CLOCK_PERF_TOUR_MICROBENCH=y`) against the simulator's counts of the same scenes
  (`microbench <file>`), fitted by `sim/cost_fit.py` (instructions in its header). Refit after
  changing the blend paths, LVGL or clock speeds.

Accuracy (2026-10-28, Orbit, not fitted on these): image faces 15.0 ms estimated vs 14.3-16.3 ms
measured; settings and alarm pages within 2-28 %; the Digital face (live widgets) +30 %. Good for
"is this 15 ms or 40 ms or a 300 ms stall", not for single-millisecond tuning: the perf tour on the
clock stays the final check.

