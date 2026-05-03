# Touch Clock

Clock firmware for the **Waveshare ESP32-P4-WIFI6-Touch-LCD-4C**: a 4-inch, 720x720 round IPS touch display driven by ESP-IDF and LVGL.

This README reflects the current app structure and on-device behavior.

## Status

The app is in active iteration but already usable as a bedside clock:

- 6 production clock faces
- dedicated alarm UX separate from settings
- focused Wi-Fi/settings surfaces instead of the older tabbed form
- SNTP time sync through the onboard Wi-Fi companion
- manual time and timezone controls with DST-aware city/timezone selection
- Wi-Fi OTA update checks and install flow
- optional remote status heartbeat backend
- touch-first round-screen interactions

Recent work focused heavily on:

- alarm UX and low-friction sleepy-state flows
- Wi-Fi/settings refactor onto the same surface model
- digital vintage face visual tuning
- gesture reliability
- rendering/performance optimization on the heaviest face

## Hardware

| Component | Spec |
|-----------|------|
| MCU | ESP32-P4 |
| Display | 4-inch 720x720 round IPS, MIPI-DSI |
| Touch | GT911 capacitive touch |
| Memory | 32MB PSRAM, 32MB NOR flash |
| Module | [Waveshare ESP32-P4-WIFI6-Touch-LCD-4C](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4C) |

## Current Feature Set

### Clock Faces

- **Digital Vintage**: DSEG7 Classic Italic digits, DSEG14 lettering, ghost digits, weekday row, date line, square mesh overlay, retro green phosphor styling
- **Matrix**: green dot-matrix face
- **Wharton**: amber dot ring + center time treatment
- **Sternglas**: light minimalist analog face
- **Avenir**: bold geometric analog face
- **Modern Silver**: silver-toned analog face

### Main Interactions

- swipe left/right between faces
- swipe up from the bottom for quick actions
- tap the alarm badge when an alarm is near to cancel only the next occurrence
- tap the transient `Cancelled` badge again to restore that occurrence

### Quick Actions Sheet

- alarms
- settings / Wi-Fi
- brightness

The sheet is a draggable pull-up/pull-down surface, not a simple pop-in menu.

### Alarms

- dedicated alarm management surface separate from settings
- individual alarm cards in the main alarm list
- large `+` create affordance
- immediate enable/disable from the list
- swipe left on an alarm card to reveal delete
- focused editor for time, repeat, enabled state, delete, save
- ringing screen reduced to two centered actions
- upcoming/snooze badge on faces
- cancel next occurrence without disabling recurring alarms
- undo that cancellation from the same transient badge
- alarm preview tone and volume controls

### Wi-Fi / Settings

- dedicated full-screen settings surface
- Wi-Fi scan / connect / forget / sync
- timezone selection
- network list
- night mode scheduling options

### Time / Runtime

- SNTP sync
- persisted last-synced epoch
- clock starts from a seed/persisted time instead of Unix epoch zero
- optional manual time mode
- OTA update check/install support
- optional remote status telemetry

## Important Current Behavior

These points matter because they differ from some earlier documentation and earlier iterations of the app:

- the primary digital face is now the retro/digital-vintage face
- legacy Slava faces remain only as persistence-compatible IDs and are not part of the active carousel
- alarms are no longer embedded inside settings
- settings are no longer built around the old tabbed form
- the quick-actions sheet includes manual brightness again
- dynamic runtime brightness modulation is currently **not** active

That last point is intentional for now:

- the display uses the manual base brightness setting
- sunrise brightness ramp is not currently applied to the physical panel
- night mode still exists as a scheduling/config surface, but display brightness is currently pinned to manual brightness until a cleaner brightness path is reintroduced

## Architecture

### App Layer

- [main/main.c](main/main.c): bootstrap only
- [main/app_controller.c](main/app_controller.c): app shell, tick orchestration, settings save debounce, UI callback bridge, brightness application
- [main/clock_model.h](main/clock_model.h): shared model types
- [main/app_settings.c](main/app_settings.c): persistence
- [main/alarm_logic.c](main/alarm_logic.c): alarm/snooze/night/runtime calculations
- [main/alarm_audio.c](main/alarm_audio.c): audio playback
- [main/wifi_time.c](main/wifi_time.c): Wi-Fi + SNTP integration

### UI Layer

- [main/clock_ui.c](main/clock_ui.c): public UI entrypoints, shared UI context owner, top-level coordination
- [main/ui/ui_shell.c](main/ui/ui_shell.c): tileview, affordances, quick-actions behavior, navigation policy
- [main/ui/ui_faces.c](main/ui/ui_faces.c): clock face construction and updates
- [main/ui/ui_alarms.c](main/ui/ui_alarms.c): alarm list, editor, ringing overlay, face badge behavior
- [main/ui/ui_settings.c](main/ui/ui_settings.c): Wi-Fi/timezone/night-mode surfaces driven from controller runtime snapshots
- [main/ui/ui_brightness.c](main/ui/ui_brightness.c): quick-actions sheet and brightness panel behavior
- [main/ui/ui_controls.c](main/ui/ui_controls.c): shared controls
- [main/ui/ui_surface.c](main/ui/ui_surface.c): reusable full-screen surface shell and edge swipe sensors

### Assets

- [main/assets/dseg7_classic_italic_112.c](main/assets/dseg7_classic_italic_112.c): main retro digits
- [main/assets/dseg7_classic_italic_56.c](main/assets/dseg7_classic_italic_56.c): retro seconds
- [main/assets/dseg14_classic_italic_36.c](main/assets/dseg14_classic_italic_36.c): large retro text
- [main/assets/dseg14_classic_italic_24.c](main/assets/dseg14_classic_italic_24.c): date text
- [main/assets/dseg14_classic_italic_20.c](main/assets/dseg14_classic_italic_20.c): weekday text
- [main/assets/alarm_pcm.c](main/assets/alarm_pcm.c): alarm tone PCM

## Performance and Optimization Notes

These are the main implementation decisions worth knowing before future UI work.

### Digital Vintage Face

The digital vintage face is the heaviest screen in the app because it combines:

- large DSEG7 labels
- ghost digits
- smaller seconds
- DSEG14 weekday/date labels
- mesh texture
- additional styling layers

To keep it usable:

- the face has a dedicated transparent swipe layer so horizontal face switching does not depend entirely on the tileview gesture path
- LVGL momentum was disabled for face-to-face swiping to avoid skipping across multiple faces
- the face is now snapshot-cached using LVGL snapshot support
- the live composed face is rendered to an image buffer and reused instead of repainting the full stack every frame

Snapshot support is enabled in:

- local `sdkconfig`
- [sdkconfig.defaults](sdkconfig.defaults)

Important caveat:

- RGB565 snapshotting can make soft gradients look banded
- the digital vintage face background was flattened to avoid visible striping in the cached output

### Face Navigation

- tileview is configured for one-face-at-a-time behavior
- horizontal momentum is disabled
- the digital vintage face uses a face-specific swipe workaround because it remained the hardest screen for LVGL to scroll smoothly

### Quick Actions Sheet

- the bottom sheet now follows the finger instead of appearing abruptly
- close/open settles with animation
- on dense screens, gesture hit-testing can be affected by overlay order, so the affordance and sheet sensors may need to stay explicitly in the foreground

### Alarm Surface

- bottom delete/save/preview responsiveness was previously blocked by oversized close-swipe sensors
- bottom edge zones were reduced so buttons remain tappable
- list resize/center-emphasis effects were quantized to avoid layout thrash during scroll

## Current Refactor State

The UI and controller boundaries were refactored without changing the on-device UX:

- [main/ui/clock_ui_internal.h](main/ui/clock_ui_internal.h) now holds the single private UI context and state definition
- [main/ui/clock_ui_private.h](main/ui/clock_ui_private.h) holds cross-feature private API declarations
- Wi-Fi scan data now flows through `app_runtime_state_t` snapshots instead of direct UI calls into `wifi_time`
- the active face catalog is the six-face manifest in [main/domain/face_catalog.c](main/domain/face_catalog.c)

## Build and Flash

```bash
source ~/esp/v5.5.3/esp-idf/export.sh
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/cu.usbmodem2101 flash
idf.py -p /dev/cu.usbmodem2101 monitor
```

Typical macOS port:

```bash
/dev/cu.usbmodem2101
```

## Useful Commands

| Command | Purpose |
|---------|---------|
| `idf.py build` | Full firmware build |
| `idf.py -p /dev/cu.usbmodem2101 flash` | Flash device |
| `idf.py -p /dev/cu.usbmodem2101 monitor` | Serial monitor |
| `ninja -C build esp-idf/main/CMakeFiles/__idf_main.dir/clock_ui.c.obj` | Fast UI object rebuild |
| `idf.py size` | Firmware size breakdown |

## Notes For Future Work

- If you want the biggest UX payoff with the smallest risk, work through reusable surface and control patterns first, then change feature flows.
- If you want the biggest performance payoff, reduce live object count on the heaviest face or keep leaning into snapshot/cached rendering.
- If brightness behavior comes back, keep manual control and runtime automation separated clearly so interactions do not regress again.
- When doing round-screen UI, prefer focused control surfaces over phone-style dense forms.

## Additional Docs

- face creation know-how: [FACE_CREATION_KNOWHOW.md](FACE_CREATION_KNOWHOW.md)
- operations and public-safety rules: [docs/operations.md](docs/operations.md)
- OTA release procedure: [docs/ota_releases.md](docs/ota_releases.md)
- Railway status backend: [docs/railway_status.md](docs/railway_status.md)
- feature backlog: [docs/features.md](docs/features.md)
