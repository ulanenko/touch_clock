# Touch Clock UX And Architecture Invariants

This document defines the behavior that must remain stable while the internal structure is refactored.

## UX invariants

- The active face carousel remains `Digital`, `Matrix`, `Wharton`, `Sternglas`, `Avenir`, `Modern Silver` in that order; legacy `Slava` face IDs remain persistence-only.
- Face navigation keeps the existing swipe model, page-dot semantics, enabled-face ordering, and night-face substitution behavior.
- The quick-actions sheet keeps the same open zones, drag model, button set, and brightness interaction semantics.
- Alarm management keeps the current list layout, editor flow, settings surface, ringing overlay, snooze flow, stop flow, preview-tone behavior, cancel-next flow, and undo window behavior.
- Wi-Fi and settings surfaces keep the same hierarchy, saved-state labels, scan/connect/forget/sync behaviors, and timezone selection UX.
- Night mode keeps the same enable toggle, schedule semantics, brightness override semantics, and face override semantics.
- Brightness keeps the same mapping between the visible slider percentage and the hardware brightness range, including sunrise behavior and ringing override behavior.
- The only accepted behavior correction is alarm creation at capacity: when all `MAX_ALARMS` slots are occupied, the UI must not overwrite an existing alarm.

## Structural invariants

- `clock_ui.h` remains the public UI boundary.
- `app_controller` remains the integration point that connects persistence, services, domain logic, and UI callbacks.
- Durable state continues to live in `app_settings_t`; ephemeral runtime state continues to live in `app_runtime_state_t`.
- Wi-Fi scan cache and connection status flow into the UI only through controller-owned runtime snapshots.
- Pure business logic stays under `main/domain/` with no LVGL, FreeRTOS, ESP-IDF, or BSP dependencies.
- UI feature files compile as separate translation units and communicate through the private UI headers in `main/ui/`.
- Settings writes and runtime mutations should flow through app actions or controller-owned logic, not direct UI mutations.

## Test fixtures to preserve

- Face catalog defaults always resolve to an enabled face.
- Disabled legacy faces are sanitized to an enabled face instead of leaking invalid state into the UI.
- Weekly alarms, one-time alarms, snooze, cancel-next, skipped occurrences, and undo-cancel behaviors remain stable.
- Brightness policy preserves manual mode, night mode, sunrise ramp, and alarm-ringing overrides.
