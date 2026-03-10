# Touch Clock

Clock firmware for the **Waveshare ESP32-P4-WIFI6-Touch-LCD-4C**: a 4-inch, 720x720 round IPS touch display driven by ESP-IDF and LVGL.

This README reflects the current app structure and on-device behavior.

## Status

The app is in active iteration but already usable as a bedside clock:

- 5 production clock faces
- dedicated alarm UX separate from settings
- focused Wi-Fi/settings surfaces instead of the older tabbed form
- SNTP time sync through the onboard Wi-Fi companion
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
- **Slava**: light analog Slava face
- **Slava Dark**: dark analog Slava face

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

## Important Current Behavior

These points matter because they differ from some earlier documentation and earlier iterations of the app:

- the old extra seven-segment face was removed from the carousel
- the primary digital face is now the retro/digital-vintage face
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

- [main/main.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/main.c): bootstrap only
- [main/app_controller.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/app_controller.c): app shell, tick orchestration, settings save debounce, UI callback bridge, brightness application
- [main/clock_model.h](/Users/borysulanenko/PycharmProjects/touch_clock/main/clock_model.h): shared model types
- [main/app_settings.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/app_settings.c): persistence
- [main/alarm_logic.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/alarm_logic.c): alarm/snooze/night/runtime calculations
- [main/alarm_audio.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/alarm_audio.c): audio playback
- [main/wifi_time.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/wifi_time.c): Wi-Fi + SNTP integration

### UI Layer

- [main/clock_ui.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/clock_ui.c): public UI entrypoints, shared UI state, top-level coordination
- [main/ui/ui_shell.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/ui/ui_shell.c): tileview, affordances, quick-actions behavior, navigation policy
- [main/ui/ui_faces.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/ui/ui_faces.c): clock face construction and updates
- [main/ui/ui_alarms.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/ui/ui_alarms.c): alarm list, editor, ringing overlay, face badge behavior
- [main/ui/ui_settings.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/ui/ui_settings.c): Wi-Fi/timezone/night-mode surfaces
- [main/ui/ui_brightness.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/ui/ui_brightness.c): quick-actions sheet and brightness panel behavior
- [main/ui/ui_controls.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/ui/ui_controls.c): shared controls
- [main/ui/ui_surface.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/ui/ui_surface.c): reusable full-screen surface shell and edge swipe sensors

### Assets

- [main/assets/dseg7_classic_italic_112.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/assets/dseg7_classic_italic_112.c): main retro digits
- [main/assets/dseg7_classic_italic_56.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/assets/dseg7_classic_italic_56.c): retro seconds
- [main/assets/dseg14_classic_italic_36.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/assets/dseg14_classic_italic_36.c): large retro text
- [main/assets/dseg14_classic_italic_24.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/assets/dseg14_classic_italic_24.c): date text
- [main/assets/dseg14_classic_italic_20.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/assets/dseg14_classic_italic_20.c): weekday text
- [main/assets/slava_face_img.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/assets/slava_face_img.c), [main/assets/slava_dark_face_img.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/assets/slava_dark_face_img.c): analog dial art
- [main/assets/alarm_pcm.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/assets/alarm_pcm.c): alarm tone PCM

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

- [sdkconfig](/Users/borysulanenko/PycharmProjects/touch_clock/sdkconfig)
- [sdkconfig.defaults](/Users/borysulanenko/PycharmProjects/touch_clock/sdkconfig.defaults)

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

## Known Cleanup Opportunities

The app is in a much better state than the earlier monolith, but there is still useful cleanup left:

- [main/clock_ui.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/clock_ui.c) still owns shared global UI state
- feature UI files are still included into `clock_ui.c` rather than being completely isolated at the state/interface level
- there are old unused face helpers still present in [main/ui/ui_faces.c](/Users/borysulanenko/PycharmProjects/touch_clock/main/ui/ui_faces.c)
- brightness runtime behavior should eventually be reintroduced cleanly instead of mixing manual and dynamic paths

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
