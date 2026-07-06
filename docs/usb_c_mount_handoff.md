# USB-C Bosses And Holes Handoff

This note packages the USB-C shell-side geometry from [3d_model.scad](/Users/borysulanenko/PycharmProjects/touch_clock/3d_model.scad) into a form that can be transplanted into another OpenSCAD project.

## Included files

- Reusable code extract: [usb_c_mount_reference.scad](/Users/borysulanenko/PycharmProjects/touch_clock/usb_c_mount_reference.scad)
- Original shell implementation: [3d_model.scad](/Users/borysulanenko/PycharmProjects/touch_clock/3d_model.scad)
- Matching holder model: [usb_c_holder.scad](/Users/borysulanenko/PycharmProjects/touch_clock/usb_c_holder.scad)

## What the shell-side geometry does

- Cuts a pill-shaped USB-C port opening through the shell.
- Adds two interior mounting bosses for the holder flange screws.
- Cuts pilot holes through those bosses for the mounting screws.

In the original clock body, the bosses are added as positive geometry inside the hollow shell, while the port opening and pilot holes are subtracted during the final `difference()`.

## Current project dimensions

These are the values used in the touch clock model.

- Shell thickness: `4.0 mm`
- USB mount angle relative to the local face normal: `35 deg`
- Port cut depth: `10.0 mm`
- Port end diameter: `3.5 mm`
- Port half spacing: `3.25 mm`
- Resulting port opening envelope: about `10.0 mm` wide by `3.5 mm` tall
- Boss center spacing: `20.0 mm` center-to-center
- Boss outer diameter: `6.0 mm`
- Boss pilot hole diameter: `1.8 mm`
- Boss protrusion behind inner wall: `1.6 mm`
- Boss overlap into shell wall: `2.0 mm`

## Matching holder assumptions

The boss pattern matches the flange holes in [usb_c_holder.scad](/Users/borysulanenko/PycharmProjects/touch_clock/usb_c_holder.scad):

- Holder screw spacing: `20.0 mm`
- Holder screw diameter: `1.8 mm`

That means the shell bosses and the holder flange are already dimensionally paired in this repo.

## Coordinate convention in the extracted file

In [usb_c_mount_reference.scad](/Users/borysulanenko/PycharmProjects/touch_clock/usb_c_mount_reference.scad), the local USB mount frame is:

- Origin: center of the USB-C opening on the exterior shell face
- `X`: left/right across the screw pattern
- `Y`: up/down across the port opening
- Negative `Z`: inward, into the enclosure
- Positive `Z`: outward, away from the enclosure

This is important because the bosses sit on the negative-`Z` side of the shell plane.

## Touch clock placement

The clock body is spherical, so the anchor point is derived from the sphere surface:

- Body diameter: `152 mm`
- Rear port Z location: `-40 mm`
- Derived anchor point: `[0, -64.622, -40]`

The extracted helper `sphere_rear_port_anchor(body_d=152, rear_port_z=-40)` recreates that anchor.

## Integration recipe for another project

1. Copy [usb_c_mount_reference.scad](/Users/borysulanenko/PycharmProjects/touch_clock/usb_c_mount_reference.scad) into the target project.
2. Decide the anchor point on the new enclosure where the center of the USB-C opening should land.
3. Decide the local mount angle for the holder.
4. Add `usb_c_mount_positive(...)` to the enclosure union.
5. Subtract `usb_c_mount_negative(...)` from the enclosure difference.
6. If the target project uses a different holder, adjust the boss spacing and pilot diameter first.

## Minimal usage example

```scad
include <usb_c_mount_reference.scad>

usb_anchor = [0, 0, 0];

difference() {
    union() {
        enclosure_shell();
        usb_c_mount_positive(
            anchor=usb_anchor,
            mount_angle=35,
            shell_thickness=4
        );
    }

    usb_c_mount_negative(
        anchor=usb_anchor,
        mount_angle=35,
        shell_thickness=4
    );
}
```

## Notes for handoff

- The shell-side geometry is designed around the holder in [usb_c_holder.scad](/Users/borysulanenko/PycharmProjects/touch_clock/usb_c_holder.scad), not around raw USB-C connector dimensions alone.
- The port opening is intentionally a little simplified and should be treated as a printable clearance cut, not as a standards drawing.
- If the new enclosure wall is much thinner or thicker than `4 mm`, the boss start depth and overlap should be revisited so the bosses are neither floating nor overly buried in the wall.
