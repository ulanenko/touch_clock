# Feature Backlog

Public-safe product notes and future feature ideas. Keep customer-specific
notes, credentials, deployment tokens, and private support context out of this
file.

Items marked **Done** were checked against the code on 2026-10-31; they stay
here for a while so the history of the list is clear.

## Alarm UX

- **Done:** new alarms are enabled by default and the editor shows neither
  the `Enabled` switch nor Delete for them
  ([main/ui/ui_alarms_editor.c](../main/ui/ui_alarms_editor.c)).
- Keep repeat/periodicity controls visible enough that users do not need to
  discover them. Instrument shows the repeat presets and day keys on the one
  editor page; Orbit and Atelier put them on the editor's second page.
- Make the alarm-time selector influence preview sound timing where useful.
- **Done:** a wake-up challenge for a harder dismissal: the per-alarm maths
  challenge (`math_unlock_enabled`, the ringing screen in
  [main/ui/ui_alarms_ring.c](../main/ui/ui_alarms_ring.c)).

## Face Navigation

- **Done:** a tap on the face shows the controls: Settings, Alarms, the page
  dots, and pull hints for the theme sheet and quick actions
  ([main/ui/ui_face_controls.c](../main/ui/ui_face_controls.c)).

## Sound And Night Mode

- **Done:** touch sounds by day, with an on/off key in the quick sheet and a
  volume in Settings › Other.
- **Done:** night mode is silent for touch sounds and the start-up chime
  (`on_ui_click_feedback`, `on_boot_chime` in
  [main/app_controller.c](../main/app_controller.c)).

## Localization

Shipped: English, Polish, Ukrainian, Russian, Dutch
([main/localization.c](../main/localization.c)).

Candidates still to add:

- German
- French
- Italian

## Hardware / Enclosure

Potential 3D-print fit improvements:

- Add roughly 0.3 mm diameter interference where a friction fit is needed.
- Add three small retention bumps instead of relying only on uniform friction.
