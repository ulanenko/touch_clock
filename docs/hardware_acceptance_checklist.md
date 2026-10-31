# Hardware Acceptance Checklist

The release gate, run on the real clock before a firmware is published or kept. Internal architecture may
change; these behaviours must not. Run the [host tests and a firmware build](development.md) first.

Tips: a second clock or a phone timer helps with the alarm and sunrise items. Set an alarm a few minutes
ahead to test ringing; use Settings › Night › Sunrise's preview rather than waiting for a real sunrise.
Do not use a build with the perf tour (`CONFIG_TOUCH_CLOCK_PERF_TOUR`) for this run.

## Boot

- [ ] The boot screen shows the wordmark and a line that fills as the faces are built; then the face comes
      up out of the dark, without corruption or a white frame.
- [ ] The start-up chime plays once, at the touch-sound volume; it is silent with touch sounds off or in
      night mode.
- [ ] The time is right after boot (synced, or kept through the restart).

## Faces

- [ ] All nine faces are in the carousel, in this order: Digital, Matrix, Wharton, Sternglas, Avenir,
      Modern Silver, Ember, Nocturne, Filament. No Slava face appears.
- [ ] A sideways drag moves the faces with the finger; released past about a fifth of the screen, or
      flicked, it goes on to the next face; otherwise it springs back. The carousel wraps round.
- [ ] A quick flick changes face even when the clock is busy (just after boot, or right after a page closes).
- [ ] Every face ticks: seconds where the face shows them, minutes on the others; the analog hands move
      without leaving trails.
- [ ] Nocturne's moon matches today's phase.
- [ ] The face chosen survives a restart.

## Controls, theme sheet, quick sheet

- [ ] A tap shows the controls (Settings, Alarms, page dots, next alarm, hints for the sheets); they hide
      after a few seconds or on a second tap. The page dots match the face.
- [ ] Dragging down on the face pulls the theme sheet: three themes of the current face with previews and
      names, in the current language. Picking one changes the face at once and persists.
- [ ] Each face's three themes all render correctly (27 in total).
- [ ] Dragging up from the bottom edge pulls the quick sheet: Alarms, Night, Sound, Settings, and the
      brightness control on the rim. The sheet follows the finger and settles; it closes by tap, drag or
      on its own after a while.
- [ ] Dragging the brightness control changes the backlight live; by day it sets the day brightness, in
      night mode the night level.
- [ ] Sound switches touch sounds on and off; Night switches night mode on or off now (until the night
      window next ends).

## Interface styles and languages

- [ ] Each of the four styles (Instrument, Orbit, Atelier, Liquid Glass) can be chosen in Settings › Other ›
      Interface style. The chrome rebuilds in the new style and returns to the same settings page; the face
      is unchanged.
- [ ] In each style, open the controls, both sheets, every settings page, the alarm list, the editor and
      the ringing screen: nothing overlaps, nothing is cut off at the round edge, buttons respond.
- [ ] Liquid Glass: sheets and pages look frosted over the face, with no face detail sharp through them and
      no noise at the screen edge. After a minute idle, opening the controls, the quick sheet and settings
      does not stall.
- [ ] Each of the five languages (English, Polski, Українська, Русский, Nederlands) can be chosen in
      Settings › Other › Language; the faces' weekdays, the chrome and the theme names follow at once, and
      no label is cut off.

## Alarms

- [ ] The alarm list shows each alarm's time, days and switch; the switch toggles and persists.
- [ ] A new alarm opens enabled, without the Enabled switch or Delete. Its time setter works in every style.
- [ ] Weekly days and "once" save and show correctly; editing keeps every field.
- [ ] Swiping a row left reveals Delete; Delete removes only that alarm.
- [ ] With five alarms, adding one does not overwrite an existing alarm.
- [ ] Each sound (Morning light, Wind chimes, Marimba, Singing bowl) previews with Test and stops with Stop.
- [ ] Alarm settings: snooze length, default sound, volume and ascending volume persist and apply to the
      next alarm.

## Ringing

- [ ] At the alarm time the ringing screen appears at full brightness, with the chosen sound.
- [ ] Snooze stops the sound and rings again after the snooze length; the face shows the snooze banner.
- [ ] Stop needs the style's deliberate gesture (hold, slide or draw along the groove) and stops at once.
- [ ] With the maths challenge on, stop asks for the answer first.
- [ ] An alarm nobody answers stops by itself after 10 minutes and the backlight fades back.
- [ ] A "once" alarm disables itself after ringing; a weekly one moves to its next day.

## Alarm banner

- [ ] Within half an hour of an alarm the banner shows on the face, in the face's own colours and clear of
      its numerals.
- [ ] A tap cancels only that occurrence ("Cancelled"); a second tap within a few seconds restores it. A
      weekly alarm still rings the next time.

## Night mode and sunrise

- [ ] Settings › Night: the enable switch, schedule, night brightness and night face persist. With night
      mode off, the rest of the page is dimmed and inert.
- [ ] At the start of the window the night face and night brightness take over; at the end the day face and
      day brightness come back.
- [ ] The night overlay shows a moon at 12 (on faces with room for it) and the next alarm with the time left
      at 6, in the face's colours.
- [ ] Sliding faces in night mode changes the night face, not the day face.
- [ ] Touch sounds are silent in night mode.
- [ ] Sunrise options (10/20/30 min, 40/60/80 % peak, the chime, the wake-up face) persist.
- [ ] The sunrise preview plays in under a minute: the backlight rises, the chime plays once, Daybreak (if
      chosen) rises with it, and a tap ends it and returns to the page.
- [ ] A real sunrise before an alarm in the night: the backlight rises smoothly, the day face takes over
      halfway, the chime plays a minute before, and the alarm rings at full brightness.

## Wi-Fi and time

- [ ] Settings › Wi-Fi shows the connected network with its signal meter and dBm, and a scan list; a scan
      finishes (or says it failed).
- [ ] Wi-Fi and update statuses ("Connecting to ...", "Connected", "Downloading...", "Validating...") show
      in the clock's language, with the network's name kept as it is.
- [ ] Joining a network with the on-screen keyboard connects; the password eye toggles.
- [ ] With a password saved, an open network with the same name is not joined.
- [ ] Forget clears the network and the labels update.
- [ ] Sync now syncs (status "Time synchronized"); with the first NTP server blocked, sync still succeeds
      from another (`pool.ntp.org`, `time.google.com`, `time.cloudflare.com`).
- [ ] Manual time: with automatic sync off, setting the date and time takes effect and survives a restart.
- [ ] Time zone: picking a city changes the local time at once; a DST city shows summer or winter time
      correctly for today's date.
- [ ] Unplugged from Wi-Fi (router off), the clock keeps time and rings; Settings shows the Wi-Fi status.
- [ ] If the Wi-Fi co-processor does not start, the clock still boots, keeps time and rings, and Settings
      shows "Wi-Fi unavailable" instead of rebooting.

## Updates (OTA)

Use a test manifest ([ota_releases.md](ota_releases.md)), never a private build.

- [ ] Settings › Other › Software update shows the installed version; a check finds a newer version and
      the face controls show the update badge.
- [ ] Install shows progress, reboots into the new version, and settings and alarms are kept.
- [ ] A manifest with an http firmware URL, a wrong SHA-256, or a version not newer is refused; the clock
      says so and stays on its version.
- [ ] Rollback: a new image that cannot start its Wi-Fi goes back to the previous image after one reboot.
- [ ] Rollback: a new image that resets in its first minute goes back to the previous image at its next
      boot; one that runs a minute is kept (serial log: "New firmware kept").
- [ ] An update installed within a new image's first minute still installs (the running image is kept
      first).
- [ ] An image already kept that later loses Wi-Fi keeps running; it does not reboot.

## Persistence and power

- [ ] All settings and alarms survive a power cycle and Settings › Other › Restart.
- [ ] After a power cut without Wi-Fi, the clock starts from the last saved time (within 5 minutes) rather
      than from 1970 or the build date.
- [ ] Settings saved by the previous release load sanely after the update (faces, themes, alarms, style,
      language, time zone).
- [ ] A disabled or invalid face in saved settings falls back to an enabled face without breaking
      navigation.

## Soak

- [ ] Use the clock for 30 minutes in each style (sheets, pages, editor, face changes): no reset, freeze or
      panic on the serial log, and no visible slowdown over time.

## Pass criteria

- Host tests pass and the firmware builds (CI green).
- The device flashes and boots.
- Every item above passes on the clock.
