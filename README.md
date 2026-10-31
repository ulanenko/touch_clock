# Touch Clock

Bedside clock firmware for the **Waveshare ESP32-P4-WIFI6-Touch-LCD-4C**: a 4-inch, 720x720 round touch
display. It is built with ESP-IDF 5.5.3 and LVGL 9.3. The clock tells the time, rings alarms, dims itself
at night and brings the screen up slowly before an alarm. Wi-Fi is used for time sync, firmware updates and
an optional status heartbeat. The clock keeps time and rings without it.

## Hardware

| Part | Spec |
|---|---|
| MCU | ESP32-P4 |
| Wi-Fi | onboard ESP32-C6, used through ESP-Hosted over SDIO (`esp_hosted` 2.12.0, `esp_wifi_remote` 1.4.0) |
| Display | 4-inch 720x720 round IPS, MIPI-DSI (JD9365), RGB565 |
| Touch | GT911 capacitive |
| Audio | ES8311 codec and speaker |
| Memory | 32 MB NOR flash; PSRAM |
| Board | [Waveshare ESP32-P4-WIFI6-Touch-LCD-4C](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4C) |

The board support package is vendored in [components/esp32_p4_wifi6_touch_lcd_xc](components/esp32_p4_wifi6_touch_lcd_xc)
(see its `VENDORED.md`): the panel runs at a 48 MHz pixel clock instead of 80, which stopped white flashes.

## Faces

Nine faces are in the carousel, in this order. The list and the order are in
[main/domain/face_catalog.c](main/domain/face_catalog.c), the ids in
[main/domain/clock_types.h](main/domain/clock_types.h). Every face has three themes, picked in the theme
sheet. The theme names are in [main/ui/ui_face_themes.c](main/ui/ui_face_themes.c).

| Face | Look | Themes |
|---|---|---|
| Digital | DSEG7 seven-segment digits with ghost segments, DSEG14 weekday and date | Green, Ice, Amber |
| Matrix | dot-matrix time | Green, Amber, Ice |
| Wharton | dot-matrix time inside a ring of dots that fills with the seconds | Gold, Rose, Ice |
| Sternglas | light minimalist analog face, Jost numerals | Porcelain, Copper, Midnight |
| Avenir | geometric analog face | Apricot, Navy, Forest |
| Modern Silver | analog face | Blue, Red, Graphite |
| Ember | dim analog face in warm reds, no numerals and no seconds | Ember, Rosewood, Kiln |
| Nocturne | the time in a large serif under a night sky, with today's moon (its real phase) over a hill | Indigo, Candle, Sea glass |
| Filament | four nixie-style tubes with the unlit digits faintly behind the lit one | Amber, Argon, Mercury |

Ember, Nocturne and Filament are the evening faces: dim and low-glare, for the bedside
([main/ui/ui_faces_evening.c](main/ui/ui_faces_evening.c)). The ids `CLOCK_FACE_SLAVA` and
`CLOCK_FACE_SLAVA_DARK` are disabled: they stay only so settings saved by older firmware still load, and
`settings_policy_sanitize` moves such a setting to an enabled face.

A tenth face, Daybreak (themes Dawn, Rose, Mist), is not in the carousel. It is the optional wake-up face of
the sunrise (see below).

## Interface styles

Everything around the faces (controls, sheets, settings, alarms) is drawn in one of four styles, chosen in
Settings › Other › Interface style. The default is Orbit. The faces look the same in every style.

| Style | Character |
|---|---|
| Instrument | graphite keys with one orange signal colour |
| Orbit | round, bright and colourful |
| Atelier | ink and gilt, like a fine watch |
| Liquid Glass | Orbit's layouts drawn as glass over the face: each shape refracts the face behind it through a convex rim, frosted and tinted |

How the styles are built, and the rules for adding a screen: [docs/ui_styles.md](docs/ui_styles.md).

## Languages

English, Polish, Ukrainian, Russian and Dutch, chosen in Settings › Other › Language. The shared strings are
in [main/localization.c](main/localization.c); strings used in one place are declared next to their use
with `UI_L10N_STR`.

## Using the clock

- **Faces.** Drag sideways: the faces follow the finger as two discs and settle on release. A release past
  140 px, or a flick, goes on to the next face; the carousel wraps round.
- **Controls.** A tap shows the controls over the face for a few seconds: Settings, Alarms, the page dots,
  the next alarm, an update badge when one is available, and hints for the two sheets.
- **Theme sheet.** Drag down on the face: the three themes of the current face, with previews.
- **Quick sheet.** Drag up from the bottom of the screen: Alarms, Night, Sound and Settings keys, and the
  brightness control on the lower rim. Night switches night mode on or off now, until the night window next
  ends. Sound switches the touch sounds on or off.
- **Alarms.** Up to five. The list shows each alarm with a switch; swipe a row left to delete it. The editor
  sets the time (each style has its own time setter), weekly days or once, the sound and an optional maths
  challenge to stop it. Alarm settings: snooze length, default sound, volume and an ascending volume.
  Sounds: Morning light (recorded), Wind chimes, Marimba and Singing bowl (synthesised on the clock).
- **Ringing.** Snooze, or stop with a deliberate gesture (hold, slide or draw, by style). An alarm that is
  not answered stops after 10 minutes.
- **Alarm banner.** Within half an hour of an alarm, a banner on the face shows it. A tap cancels only that
  occurrence; a second tap on "Cancelled" soon after brings it back.
- **Settings.** Wi-Fi (networks, join with the on-screen keyboard, forget, time zone, automatic sync, sync
  now, manual time), Night, and Other (language, interface style, touch sound and its volume, software
  update, About, restart).
- **Night mode.** A schedule, a night brightness and a night face. While it is on, the night face shows with
  a small overlay: a moon at 12 and the next alarm at 6. Touch sounds and the start-up chime stay silent at
  night.
- **Sunrise.** With night mode and the sunrise on, the backlight rises out of the night level over the 10,
  20 or 30 minutes before an alarm, to a peak of 40, 60 or 80 %. It lights the screen only when it starts
  in the night. Halfway through, the night face gives way to the day face. Optionally a soft chime a minute
  before the alarm, and the Daybreak face, whose sun climbs with the light. Settings › Night › Sunrise has
  a preview that plays it all in under a minute.
- **Brightness.** The day brightness by day, the night brightness in night mode, the sunrise ramp before an
  alarm, full brightness while ringing.

## Time and network

- **Time sync.** SNTP polls `pool.ntp.org`, `time.google.com` and `time.cloudflare.com`, the next tried when
  one does not answer (`CONFIG_LWIP_SNTP_MAX_SERVERS=3`). Sync starts when Wi-Fi gets an address, if
  automatic sync is on. Manual time can be set instead.
- **Keeping time.** A known time is saved to NVS every 5 minutes. After a restart the clock keeps the time
  it has; after a power loss it starts from the last saved time until it syncs.
- **Time zones.** 19 cities and UTC, with their daylight-saving rules as POSIX TZ strings
  ([main/domain/timezone_rules.c](main/domain/timezone_rules.c)). Fixed UTC offsets remain for settings
  from older firmware.
- **Wi-Fi.** With a password saved, the clock will not join an open network that takes the same name. If
  the Wi-Fi co-processor does not start, the clock runs without it and Settings shows "Wi-Fi unavailable".
- **Updates (OTA).** The clock reads a JSON manifest over https 30 s after boot and every 6 hours, and from
  Settings › Other › Software update. The firmware URL must be https and the image must match the
  manifest's SHA-256. Two 8 MB app slots, with rollback: a new image is kept only after it has run for a
  minute, so one that crashes before then goes back to the old image at its next boot; one whose Wi-Fi
  does not start goes back at once. Releases: [docs/ota_releases.md](docs/ota_releases.md).
- **Telemetry (optional).** A heartbeat (device id, IP, signal, time sync, next alarm, OTA status, uptime,
  free memory) is posted every 5 minutes by default, over https only, with a token. It is off unless a URL
  is configured. Backend: [docs/railway_status.md](docs/railway_status.md).

The OTA and telemetry URLs and the token are set in `idf.py menuconfig` › Touch Clock and live in the
untracked `sdkconfig`. Never commit it. See [docs/operations.md](docs/operations.md).

## Architecture

The code is layered so that the logic can be tested on a desktop. Each layer only calls the ones below it.

| Layer | Files | What it does |
|---|---|---|
| Boot | [main/main.c](main/main.c) | NVS init, display config (`TRIPLE_FULL`, 10 KB UI stack), starts the controller |
| ESP glue | [main/app_controller.c](main/app_controller.c) | wires the ESP services into the core, turns UI callbacks into app actions, runs the 1 s tick timer |
| Controller core | [main/app/](main/app) | `app_controller_core.c`: boot, tick, applying action results, debounced settings save, brightness, alarm audio. `app_actions.c`: one function per user action, returning the effects to apply. `app_settings_storage.c`: settings to and from a key-value store. `app_state.h`: the state |
| Domain | [main/domain/](main/domain) | pure logic: `alarm_scheduler`, `brightness_policy`, `face_catalog`, `settings_policy` (defaults, sanitising), `timezone_rules`, `ota_manifest` (parsing, version compare). Types in `clock_types.h` and `app_settings_types.h` |
| Platform | [main/platform/](main/platform) | `platform_services.h`: service interfaces (clock, audio, Wi-Fi, display, OTA, telemetry, settings store). `platform_esp_services.c`: their ESP implementations |
| ESP services | [main/wifi_time.c](main/wifi_time.c), [main/ota_update.c](main/ota_update.c), [main/telemetry_update.c](main/telemetry_update.c), [main/alarm_audio.c](main/alarm_audio.c), [main/alarm_tones.c](main/alarm_tones.c), [main/app_settings.c](main/app_settings.c) | Wi-Fi and SNTP, OTA, heartbeat, audio playback and synthesised tones, NVS |
| UI | [main/clock_ui.h](main/clock_ui.h), [main/clock_ui.c](main/clock_ui.c), [main/ui/](main/ui) | LVGL. Public boundary `clock_ui.h` (boot, tick, refresh, callbacks) |

Inside [main/ui/](main/ui):

| Part | Files |
|---|---|
| Shell and faces | `ui_shell.c` (tileview, face slide, sheet backdrop), `ui_faces.c`, `ui_faces_evening.c`, `ui_sunrise.c`, `ui_boot.c` |
| Chrome core | `ui_chrome.c` (chrome lifecycle, debug calls), `ui_areas.h` (what the areas share), `ui_l10n.c`, `ui_format.c` |
| Face chrome | `ui_face_controls.c`, `ui_face_sheet.c`, `ui_face_themes.c`, `ui_brightness.c` (quick sheet), `ui_face_night.c` |
| Settings | `ui_settings.c`, `ui_settings_wifi.c`, `ui_settings_night.c`, `ui_settings_other.c` |
| Alarms | `ui_alarms.c`, `ui_alarms_list.c`, `ui_alarms_editor.c`, `ui_alarms_timeset.c`, `ui_alarms_ring.c` |
| Kit | `kit/ui_skin.c` (style tokens), `kit/ui_kit*.c` (components, controls, text, cached dials), `kit/ui_glass.c` (Liquid Glass) |
| Rendering support | `ui_ppa_blend.c` (hardware blending), `ui_lv_mem.c` (LVGL allocator), `ui_frame_free.c` |
| Diagnostics | `ui_perf_tour.c`, `ui_microbench.c` (built only with `CONFIG_TOUCH_CLOCK_PERF_TOUR`) |

The UI never calls a service. It reads the settings and a runtime snapshot (`app_runtime_state_t`) and
reports user actions through `clock_ui_callbacks_t`. The rules that keep this so are in
[docs/architecture_invariants.md](docs/architecture_invariants.md). Threads and locks:
[docs/development.md](docs/development.md#threads-and-locks).

## Build and flash

```bash
. ~/esp/v5.5.3/esp-idf/export.sh
idf.py build
idf.py -p /dev/cu.usbmodem2101 flash monitor
```

The target (`esp32p4`) comes from [sdkconfig.defaults](sdkconfig.defaults). CI builds the firmware in the
`espressif/idf:v5.5.3` container ([.github/workflows/ci.yml](.github/workflows/ci.yml)). Toolchain setup and
the other build directories: [docs/development.md](docs/development.md).

## Tests and simulator

- **Host tests** ([tests/host](tests/host)): the domain layer, the controller core and the string tables,
  built with the system C compiler and run under AddressSanitizer and UBSan in CI.
- **Simulator** ([sim/](sim)): the real UI on the desktop (macOS or Linux), driven by scenario scripts in
  [sim/scenarios](sim/scenarios), writing 720x720 PNGs. CI runs a few scenarios under the sanitizers.
- **Perf tour**: a diagnostic firmware that plays the UI on the clock and reports frame times
  ([scripts/perf_tour.py](scripts/perf_tour.py)).

Commands and details: [docs/development.md](docs/development.md).

## Other docs

| Doc | About |
|---|---|
| [docs/development.md](docs/development.md) | toolchain, builds, tests, simulator, perf tour, threads, memory |
| [docs/architecture_invariants.md](docs/architecture_invariants.md) | behaviour and structure that refactors must keep |
| [docs/ui_styles.md](docs/ui_styles.md) | how the four styles are built; performance rules for the UI |
| [FACE_CREATION_KNOWHOW.md](FACE_CREATION_KNOWHOW.md) | adding a face |
| [docs/performance_audit.md](docs/performance_audit.md) | what was measured on the clock, what was fixed, what is open |
| [docs/hardware_acceptance_checklist.md](docs/hardware_acceptance_checklist.md) | the on-device release check |
| [docs/ota_releases.md](docs/ota_releases.md) | publishing a firmware update |
| [docs/operations.md](docs/operations.md) | public-safety rules, partitions, remote status |
| [docs/railway_status.md](docs/railway_status.md) | the status backend |
| [docs/features.md](docs/features.md) | backlog |
| [docs/usb_c_mount_handoff.md](docs/usb_c_mount_handoff.md) | enclosure: the USB-C mount geometry |
