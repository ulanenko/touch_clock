# Hardware Acceptance Checklist

This checklist is the release gate for maintainability refactors. Internal architecture may change, but these behaviors must remain unchanged on device.

## Boot And Idle

- Device boots to the current face without visual corruption.
- Time renders correctly after boot.
- Settings button is visible and responsive.
- Face swipe affordances appear and fade as before.

## Face Navigation

- Horizontal face swipes change only between enabled faces.
- Page dots match the active face.
- Face changes persist across re-entry to the main screen.
- Night mode shows the configured night face and exits back to the configured day face.

## Brightness Surface

- Bottom-edge swipe opens the brightness sheet.
- The sheet can be closed by tap, swipe, and drag as before.
- Day-mode brightness changes persist as base brightness.
- Night-mode live slider changes preview runtime brightness immediately.
- Saving night brightness persists the configured night brightness.
- Ringing still forces the expected visible brightness behavior.

## Alarm Banner And Ringing

- Alarm banner appears only when expected.
- Ringing overlay shows the same controls and labels.
- `Snooze` starts the configured snooze duration.
- `Stop` stops ringing immediately.
- Alarm test starts and stops correctly from management settings.

## Alarm Management

- Opening alarm management does not disturb the active face state.
- Existing alarms render the same metadata and enabled state.
- Toggling an alarm on or off persists correctly.
- Editing an existing alarm preserves all current repeat/day semantics.
- Deleting an alarm removes only that alarm.
- Creating a new alarm never overwrites an existing alarm when all slots are full.
- `Cancel next` and `Undo` behave exactly as before, including timeout behavior.

## Alarm Settings

- Snooze duration changes persist and affect later alarms.
- Alarm volume changes persist and affect preview/ringing volume.
- The management screen summaries stay in sync with saved values.

## Wi-Fi And Time

- Wi-Fi scan shows available networks.
- Saving credentials connects with the chosen SSID/password.
- Forget Wi-Fi clears saved credentials and updates labels.
- Manual time sync triggers the same sync flow as before.
- Timezone changes update displayed local time correctly.

## Night Mode

- Night mode enable/disable switch persists.
- Night schedule start/end edits persist.
- Night brightness persists separately from day brightness.
- Night face picker persists the selected face.
- Night-mode status text and summaries stay in sync with current runtime state.

## Persistence Regression Checks

- Settings survive power cycle.
- Legacy settings upgraded from previous firmware remain sane after first boot.
- Disabled or invalid face selections are sanitized to an enabled face without breaking navigation.

## Pass Criteria

- Host tests pass.
- Firmware build passes.
- Device flashes and boots successfully.
- Every checklist item above passes on hardware.
