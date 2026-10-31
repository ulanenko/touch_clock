# Development

How to build the firmware, run the tests and the simulator, measure on the clock, and where the code runs.
For what the clock does and the layer map, see the [README](../README.md).

## Toolchain

- **ESP-IDF v5.5.3.** The scripts expect it in `~/esp/v5.5.3/esp-idf`, with its Python environment in
  `~/.espressif/python_env/idf5.5_py3.12_env` ([scripts/perf_tour.py](../scripts/perf_tour.py)). Install it the
  usual way (`git clone -b v5.5.3 --recursive https://github.com/espressif/esp-idf.git`, then
  `install.sh esp32p4`).
- **Host tests:** CMake 3.20 or newer and a C17 compiler.
- **Simulator:** CMake, a C compiler, Python 3 and zlib (`zlib1g-dev` on Debian and Ubuntu). It builds on
  macOS and on Linux with glibc (the `mem` command reads each one's malloc statistics).
- **Assets** (only to regenerate them): `scripts/fonts/build_fonts.py` needs fonttools and node (`npx`);
  `scripts/ui_assets/build_ui_assets.py` needs Pillow and Playwright with Chrome; `sim/cost_fit.py` needs
  numpy and scipy.

## Firmware

```bash
. ~/esp/v5.5.3/esp-idf/export.sh
idf.py build
idf.py -p /dev/cu.usbmodem2101 flash monitor   # macOS; the port name varies
idf.py size
```

- The target, `esp32p4`, comes from [sdkconfig.defaults](../sdkconfig.defaults). A fresh checkout needs no
  `set-target`. Note that `idf.py set-target` regenerates `sdkconfig` from scratch, losing local values.
- The first build downloads the managed components (LVGL 9.3, ESP-Hosted, the Wi-Fi remote, the codec
  and touch drivers) into `managed_components/`. Versions are pinned in
  [main/idf_component.yml](../main/idf_component.yml) and [dependencies.lock](../dependencies.lock). The board
  support package is the vendored copy in [components/](../components/esp32_p4_wifi6_touch_lcd_xc).
- `sdkconfig` is generated and git-ignored. The OTA manifest URL, the telemetry URL and token live there,
  set through `idf.py menuconfig` › Touch Clock ([main/Kconfig.projbuild](../main/Kconfig.projbuild)). Both
  URLs must be https or the feature turns itself off. Never commit `sdkconfig`, and never paste its
  values into docs or logs: see [operations.md](operations.md).
- LVGL fixes not yet upstream are applied at configure time by
  [cmake/lvgl_patches.cmake](../cmake/lvgl_patches.cmake), for the firmware and the simulator alike.
- CI ([.github/workflows/ci.yml](../.github/workflows/ci.yml)) runs on every push and pull request: the host
  tests, then `idf.py build` in the `espressif/idf:v5.5.3` container followed by a simulator smoke test on
  what that build made (see [Simulator](#simulator)).

## Host tests

[tests/host](../tests/host) builds the domain layer, the controller core and the string tables
(`main/domain/*.c`, `main/app/*.c`, `main/localization.c`) with the host compiler; [tests/host/fakes/esp_err.h](../tests/host/fakes/esp_err.h) stands in
for ESP-IDF. Run them as CI does, under AddressSanitizer and UBSan:

```bash
cmake -S tests/host -B /tmp/touch_clock_host_tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all"
cmake --build /tmp/touch_clock_host_tests
ctest --test-dir /tmp/touch_clock_host_tests --output-on-failure
```

Keep the sanitizers on locally too: a `bool` read from a `0xFF` byte passed with macOS's clang and failed
on CI's Linux.

| Test | Covers |
|---|---|
| `face_catalog_test` | the carousel, enabled faces, stepping |
| `settings_policy_test` | defaults and sanitising of stored settings |
| `brightness_policy_test` | day, night, sunrise and ringing brightness; UI and hardware percent |
| `alarm_scheduler_test` | weekly and one-time alarms, snooze, cancel-next, the sunrise, DST changes, late ticks |
| `timezone_rules_test` | fixed offsets, summer time, picker order, POSIX rules against IANA zones |
| `app_actions_test` | user actions and the effects they return |
| `app_controller_core_test` | boot, tick, save debounce, auto-stop, sunrise, services that fail to start |
| `app_settings_storage_test` | settings to and from a fake key-value store, older versions' migrations |
| `ota_manifest_test` | manifest parsing, https and SHA-256 checks, version order |
| `localization_test` | runtime statuses (Wi-Fi, updates) in the clock's language |

## Simulator

[sim/](../sim) runs the real UI code (`main/clock_ui.c`, `main/ui/`, `main/domain/`, `main/localization.c`,
the assets) on the desktop with the same LVGL and the project's `sdkconfig`. It renders to a 720x720 RGB565
frame buffer and plays a scenario script.

```bash
idf.py reconfigure          # once: fetches managed_components/ and writes sdkconfig
cmake -S sim -B sim/build \
  -DLVGL_DIR=$PWD/managed_components/lvgl__lvgl \
  -DSDKCONFIG=$PWD/sdkconfig
cmake --build sim/build -j8
mkdir -p sim/out
sim/build/sim sim/scenarios/smoke.txt sim/out
```

- `LVGL_DIR` and `SDKCONFIG` default to those same paths ([sim/CMakeLists.txt](../sim/CMakeLists.txt)); pass
  them to point at another LVGL or config.
- `-DSIM_ASAN=ON` builds with AddressSanitizer and UBSan. It found a use-after-free in Liquid Glass that
  the device never showed. Use a separate build directory (`sim/build_asan`).
- [sim/gen_sdkconfig.py](../sim/gen_sdkconfig.py) turns `sdkconfig` into `sim_sdkconfig.h`. It drops what
  a single-threaded host build cannot use (`CONFIG_LV_OS_*`, the PPA, assembly, FreeRTOS and SoC keys,
  the perf tour) and every `CONFIG_TOUCH_CLOCK_TELEMETRY_*` and `CONFIG_TOUCH_CLOCK_OTA_*` key, so the
  simulator holds no copy of the telemetry token or the update URL. It sets one draw unit and LVGL's
  profiler hooks, which count draw tasks per frame.
- The UI sources are compiled with [sim/stubs/sim_time.h](../sim/stubs/sim_time.h) force-included, so
  `time()` reads the simulated clock. Other ESP headers are stubbed in [sim/stubs](../sim/stubs).
- The simulator's controller is a stand-in: callbacks write the settings and runtime state directly;
  Wi-Fi, OTA and cancel-next requests do nothing.
- Starting state: Tuesday 27 October 2026, 14:13:00 UTC, the Sternglas face in the default style (Orbit),
  three alarms, night mode 22:30-07:00 on Matrix, five scanned networks, an update 1.9.0 available.
- Timings are the host's. To estimate the clock's, use `estimate` and `timecall` (below, and
  [performance_audit.md](performance_audit.md#simulator-cost-model-estimates-without-the-clock)).
- CI builds it with `-DSIM_ASAN=ON` and runs `smoke`, `settings_screens` and `alarms_orbit`. Each must run
  to the end and write every `shot`, and no sanitizer report may point outside `managed_components`.

### Scenario commands

One command per line; `#` starts a comment. An unknown command, setting or debug call stops the run with
an error naming the line. Simulated time advances in 5 ms steps; each simulated second calls the UI tick.

| Command | Does |
|---|---|
| `wait <ms>` | advance simulated time |
| `tap <x> <y>` | press and release in place (60 ms) |
| `drag <x0> <y0> <x1> <y1> <ms>` | press, move in a straight line over `ms`, release |
| `press <x> <y>` / `move <x> <y>` / `release` | a gesture by hand |
| `call <name>` | run a debug call (below) |
| `set <key> <value>` | change a setting or runtime state, then refresh the UI (keys below) |
| `date <YYYY>-<MM>-<DD>` | the day the next `clock` sets the time on |
| `clock <HH:MM:SS>` | set the wall clock and tick the UI |
| `shot <name>` | write `<output-dir>/<name>.png` |
| `probe <x> <y>` | print the clickable object under a point and its flags |
| `sliders` | drag every visible slider past both ends and print the range reached |
| `alarms` | print the saved alarms |
| `bench <name> [n]` | time `n` full-screen renders (default 20) and the draw tasks per frame |
| `coldbench <name>` | flush the dial cache and time the next frame (dial builds) |
| `benchdrag <name> <x0> <y0> <x1> <y1> <ms>` | drag and time the UI work (events, layout, render) per step |
| `costs [n]` | hide each visible object in turn; print the `n` (default 12) that cost the most render time |
| `drawcost [n]` | count each object's draw tasks and pixels by kind for one frame; print the top `n` (default 15) |
| `estimate [label]` | estimated clock render time of the current screen, from [sim/cost_model.txt](../sim/cost_model.txt) |
| `timecall <name>` | run a debug call and scale its host CPU time to the clock (`cpu_scale`) |
| `microbench <file>` | write the cost-model scenes' features for [sim/cost_fit.py](../sim/cost_fit.py) |
| `mem [note]` | print the process heap in use |
| `hold <s>` | sleep in real seconds (to inspect the process from outside) |

`set` keys ([sim/sim_main.c](../sim/sim_main.c), `apply_set`):

| Key | Value |
|---|---|
| `style` | 0 Instrument, 1 Orbit, 2 Atelier, 3 Liquid Glass |
| `face`, `night_face` | a `clock_face_id_t` (0 Digital ... 10 Filament; 3 and 4 are disabled) |
| `face_theme` | 0-2, for the current face |
| `language` | 0 en, 1 pl, 2 uk, 3 ru, 4 nl |
| `brightness` | base brightness, hardware percent |
| `night_enabled`, `night_active` | night mode setting; in night mode now |
| `alarm_ringing`, `alarm_math`, `alarm_test` | alarm 0 ringing; its maths challenge; the test sound playing |
| `alarm_delete` | slot to clear, as a delete leaves it |
| `next_alarm_minutes`, `snooze_minutes_left` | next alarm or snooze end, minutes from now (0: none) |
| `sunrise_active`, `sunrise_from_night` | the sunrise running; it started in the night |
| `wifi_connected`, `wifi_scanning`, `wifi_ssid_empty` | Wi-Fi state; 1 clears the saved network |
| `time_synced`, `time_sync_manual` | sync state; manual time mode |
| `ota_available`, `ota_configured`, `ota_progress` | update state; install progress 0-100 |
| `click_sound` | touch sounds on or off |

### Debug calls

Named entry points that open a screen or set a state, registered with `ui_debug_register(name, fn)`
([main/ui/ui_areas.h](../main/ui/ui_areas.h), table in [main/ui/ui_chrome.c](../main/ui/ui_chrome.c)). The
simulator runs them with `call`; the perf tour calls them through `DEBUG_CALL` wrappers in
[main/ui/ui_perf_tour.c](../main/ui/ui_perf_tour.c). Register one for every new screen and state.

| Area | Calls |
|---|---|
| Face ([ui_shell.c](../main/ui/ui_shell.c)) | `show_controls`, `hide_controls`, `open_theme`, `close_theme`, `next_theme`, `open_quick`, `close_quick`, `boot_intro` |
| Liquid Glass timing ([ui_shell.c](../main/ui/ui_shell.c)) | `glass_backdrop` (snapshot and frost), `glass_to_page`, `glass_bake_row` (500x88), `glass_bake_panel` (600x572) |
| Night and sunrise | `night_overlay` (toggle), `sunrise_preview`, `daybreak_render` |
| Settings ([ui_settings.c](../main/ui/ui_settings.c)) | `open_settings`, `open_wifi`, `open_night`, `open_other`, `settings_back`, `close_settings`, `settings_scroll` (300 px), `settings_scroll_end`, `join_dialog`, `join_type`, `timezone_picker`, `manual_time`, `night_schedule`, `night_face_picker`, `night_brightness`, `sunrise_options`, `language_picker`, `style_picker`, `about` |
| Alarms ([ui_alarms.c](../main/ui/ui_alarms.c)) | `open_alarms`, `close_alarms`, `open_alarm_editor`, `alarm_editor_details`, `new_alarm`, `new_alarm_details`, `close_alarm_editor`, `alarm_settings`, `alarms_scroll_end`, `alarm_swipe_delete`, `ringing`, `ringing_math`, `ringing_off`, `banner_feedback`, `alarm_sound_next`, `alarm_test_stop` |
| Kit ([ui_kit_specimen.c](../main/ui/kit/ui_kit_specimen.c)) | `kit_specimen`, then `kit_specimen_2` ... `kit_specimen_7`, `kit_specimen_scroll`, `kit_specimen_close` |
| Core | `ui_refresh` |

A short scenario:

```text
set style 3          # Liquid Glass
clock 06:58:00
wait 500
call open_night
wait 600
shot glass_night
estimate glass_night
```

## Perf tour (on the clock)

`CONFIG_TOUCH_CLOCK_PERF_TOUR` builds [main/ui/ui_perf_tour.c](../main/ui/ui_perf_tour.c) into the firmware.
After a delay it drives the UI with a virtual touch input (face taps and swipes, theme and quick sheet
pulls, settings flicks and slow drags, the alarm list and editor) and prints `PERF` lines on the console:
`PERF seg` per step (render average, p95 and max, layout time, longest gap between frames, frames over
33 ms, white frames) and `PERF segx` (flush wait, PPA time, free PSRAM). It must never be shipped or
published over OTA.

[scripts/perf_tour.py](../scripts/perf_tour.py) does the whole run. Use the IDF Python, which has pyserial:

```bash
PY=~/.espressif/python_env/idf5.5_py3.12_env/bin/python
$PY scripts/perf_tour.py --restore                       # build, flash, capture, report, flash build/ back
$PY scripts/perf_tour.py --set CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLES=y --set CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLE_FIRST=3
$PY scripts/perf_tour.py --compare perf_reports/<run-a> perf_reports/<run-b>
```

- It copies the local `sdkconfig` to `build_perf/sdkconfig`, turns the tour and thumbnails on, moves the
  console to USB-Serial-JTAG (the UART is too slow for thumbnails), and builds in `build_perf/`. The normal
  `build/` is not touched. The perf image is built from your local config, so it is for your bench only.
- It picks the first `/dev/cu.usbmodem*` port unless `--port` is given, captures for up to `--timeout`
  seconds (420), and writes `perf_reports/<time>-<label>/` with `serial.log`, `report.md`, `summary.json`
  and thumbnails.
- Other options: `--no-build`, `--no-flash`, `--parse <log>`, `--image`/`--save-image <name>`
  (`perf_images/`), `--build-only`, `--manual` (no script: it only records frames while you use the clock).

Tour options (menuconfig › Touch Clock, or `--set`):

| Option | Effect |
|---|---|
| `..._START_DELAY_S` (25), `..._LOOPS` (2) | when it starts, how many loops |
| `..._STYLES`, `..._STYLE_FIRST` | each loop in the next style, starting at 0-3 |
| `..._START_FACE` | the face each loop starts on (-1 keeps the current one) |
| `..._SHORT` | settings, night face picker and alarms only |
| `..._MANUAL_ONLY` | no script, frame monitoring only |
| `..._FACE_COSTS` | every face in turn: first frames and ticks |
| `..._MICROBENCH` | the cost-model scenes ([main/ui/ui_microbench.c](../main/ui/ui_microbench.c)) |
| `..._THUMBNAILS`, `..._THUMB_STEP`, `..._FACE_GALLERY` | screen thumbnails, their downscale, full-size face shots |
| `..._PROFILE_SEGMENTS` | steps to trace with LVGL's builtin profiler; summarise with [scripts/prof_summary.py](../scripts/prof_summary.py) |

Results and what they led to are in [performance_audit.md](performance_audit.md).

## Threads and locks

Almost everything runs on one thread: the LVGL task. The rest are small tasks that each own one job and
hand results back under a mutex.

| Thread | Created in | Runs |
|---|---|---|
| main task | ESP-IDF | `app_main`: NVS, the display, `app_controller_core_bootstrap` (settings load, services' init), then the UI build under the display lock. It returns after that |
| `lvgl` (10 KB stack) | the LVGL adapter, configured in [main/main.c](../main/main.c) | all LVGL work: every screen, animation and LVGL timer; the 1 s controller tick (`clock_tick_cb` in [main/app_controller.c](../main/app_controller.c)); every UI callback and the app action it runs; the settings save to NVS (about 2.6 ms); Wi-Fi connect and forget calls to the co-processor |
| LVGL draw threads (2) | LVGL (`CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=2`, priority `CONFIG_LV_DRAW_THREAD_PRIO`) | software rendering of each frame's draw tasks |
| `glass_bake` (6 KB) | [main/ui/kit/ui_glass.c](../main/ui/kit/ui_glass.c) | Liquid Glass: the second half of a bake or frost, on the other core, while the UI thread does the first half and waits for it |
| `wifi_scan` (4 KB, priority 4) | [main/wifi_time.c](../main/wifi_time.c) | one scan, then exits |
| `ota_check` (stack in PSRAM) / `ota_install` (internal RAM), priority 4 | [main/ota_update.c](../main/ota_update.c) | one manifest check or one install, then exits |
| `clock_telemetry` (8 KB) | [main/telemetry_update.c](../main/telemetry_update.c) | one heartbeat, then exits |
| `alarm_audio` (5 KB, priority 4) | [main/alarm_audio.c](../main/alarm_audio.c) | owns the codec; plays what its command queue asks |
| `alarm_tones` (4 KB, lowest priority) | [main/alarm_audio.c](../main/alarm_audio.c) | renders the synthesised alarm sounds 20 s after boot |
| system event loop, network stack | ESP-IDF | Wi-Fi and IP events, the SNTP sync callback ([main/wifi_time.c](../main/wifi_time.c)) |
| `esp_timer` | ESP-IDF | the backlight fade (`bl_fade` in [main/platform/platform_esp_services.c](../main/platform/platform_esp_services.c)) |

Locks:

- **`bsp_display_lock()`** (the LVGL adapter's lock) guards LVGL. Code on the LVGL task already holds it.
  Anything else must take it before touching LVGL. Today only `app_controller_start` does, to build the UI
  and create the tick timer.
- **Module mutexes:** `s_wifi.lock`, `s_ota.lock`, `s_telemetry.lock` and `s_tone_lock` guard each module's
  state between its task, its event callbacks and the LVGL task. The audio task is fed through a queue.
- `glass_bake` is handed work with two binary semaphores.

The rule between the UI and the rest: the UI never calls a service or reads a module's state. Once a tick,
the controller copies Wi-Fi and OTA state into `app_runtime_state_t` (`wifi_time_snapshot`,
`ota_update_snapshot`, each under its module's mutex), and the UI reads only that and `app_settings_t`.
User actions go out through `clock_ui_callbacks_t` to `app_controller.c`, which runs the app action and
applies its effects. The controller and the UI share one thread, so neither the settings nor the runtime
snapshot need a lock of their own. A task that finishes work (a scan, a check) sets its module's state and
the next tick picks it up.

## Memory

- **PSRAM first for LVGL.** [main/ui/ui_lv_mem.c](../main/ui/ui_lv_mem.c) is LVGL's allocator
  (`CONFIG_LV_USE_CUSTOM_MALLOC`): objects, styles, text and draw tasks come from PSRAM, internal RAM only
  when PSRAM runs out. Through `malloc` they filled the internal RAM the network, DMA and stacks need. Draw
  buffers (snapshots, canvases, layers) also come from PSRAM (`draw_buf_malloc_psram` in
  [main/ui/ui_shell.c](../main/ui/ui_shell.c)).
- **Freeing after the frame.** A cache that drops an image while a frame is being drawn cannot free it at
  once: draw tasks on the other core may still read it. [main/ui/ui_frame_free.c](../main/ui/ui_frame_free.c)
  keeps such buffers until the display's next `REFR_READY`.
- **Bounded caches.** The kit dial cache keeps to 6 MB ([main/ui/kit/ui_kit_dial.c](../main/ui/kit/ui_kit_dial.c));
  the Liquid Glass bake cache to 6 MB and 96 shapes, and leaves 2 MB of PSRAM free
  ([main/ui/kit/ui_glass.c](../main/ui/kit/ui_glass.c)). The hand-drawn faces share one view canvas.
- **Alarm sounds.** Only the synthesised sounds the enabled alarms use stay rendered (`keep_sounds`); the
  others are rendered for a preview and freed after.
- **LP RAM is not heap** (`CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP` off): draw buffers that landed
  there were about 20 times slower.
- Stack sizes were set from the perf tour's `PERF stack` lines; check them again after adding work to a
  task.

The measurements behind each of these are in [performance_audit.md](performance_audit.md).
