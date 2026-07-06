/*
   Reusable USB-C shell-side mount reference extracted from 3d_model.scad.

   Coordinate convention in local mount space:
   - origin: center of the USB-C opening on the exterior shell face
   - X: left/right across the screw pattern
   - Y: up/down across the port opening
   - negative Z: inward, into the enclosure
   - positive Z: outward, away from the enclosure

   Use usb_c_mount_positive() in a union() to add the bosses.
   Use usb_c_mount_negative() in a difference() to subtract the port opening
   and the boss pilot holes.
*/

$fn = 120;

function sphere_rear_port_anchor(body_d=152, rear_port_z=-40) =
    [0, -sqrt(max(0, pow(body_d / 2, 2) - pow(rear_port_z, 2))), rear_port_z];

module usb_c_mount_axis(anchor=[0, 0, 0], mount_angle=35) {
    translate(anchor)
        rotate([90 + mount_angle, 0, 0])
            children();
}

module usb_c_port_cutout_local(
    port_half_spacing=3.25,
    port_end_d=3.5,
    port_cut_depth=10,
    outer_overshoot=1
) {
    cut_start_z = -port_cut_depth - outer_overshoot;
    cut_height = port_cut_depth + outer_overshoot * 2;

    hull() {
        translate([-port_half_spacing, 0, cut_start_z])
            cylinder(d=port_end_d, h=cut_height);
        translate([port_half_spacing, 0, cut_start_z])
            cylinder(d=port_end_d, h=cut_height);
    }
}

module usb_c_holder_bosses_local(
    shell_thickness=4,
    holder_bolt_half_spacing=10,
    holder_boss_d=6,
    holder_boss_depth=1.6,
    holder_shell_overlap=2
) {
    boss_front_z = -shell_thickness - holder_boss_depth;
    boss_height = holder_boss_depth + holder_shell_overlap;

    for (x = [-holder_bolt_half_spacing, holder_bolt_half_spacing]) {
        translate([x, 0, boss_front_z])
            cylinder(d=holder_boss_d, h=boss_height);
    }
}

module usb_c_holder_boss_holes_local(
    shell_thickness=4,
    holder_bolt_half_spacing=10,
    holder_boss_depth=1.6,
    holder_boss_pilot_d=1.8,
    holder_shell_overlap=2,
    front_clearance=0.5,
    back_clearance=1
) {
    boss_front_z = -shell_thickness - holder_boss_depth - front_clearance;
    boss_hole_height = holder_boss_depth + holder_shell_overlap + back_clearance;

    for (x = [-holder_bolt_half_spacing, holder_bolt_half_spacing]) {
        translate([x, 0, boss_front_z])
            cylinder(d=holder_boss_pilot_d, h=boss_hole_height);
    }
}

module usb_c_mount_positive(
    anchor=[0, 0, 0],
    mount_angle=35,
    shell_thickness=4,
    holder_bolt_half_spacing=10,
    holder_boss_d=6,
    holder_boss_depth=1.6,
    holder_shell_overlap=2
) {
    usb_c_mount_axis(anchor, mount_angle)
        usb_c_holder_bosses_local(
            shell_thickness=shell_thickness,
            holder_bolt_half_spacing=holder_bolt_half_spacing,
            holder_boss_d=holder_boss_d,
            holder_boss_depth=holder_boss_depth,
            holder_shell_overlap=holder_shell_overlap
        );
}

module usb_c_mount_negative(
    anchor=[0, 0, 0],
    mount_angle=35,
    shell_thickness=4,
    port_half_spacing=3.25,
    port_end_d=3.5,
    port_cut_depth=10,
    outer_overshoot=1,
    holder_bolt_half_spacing=10,
    holder_boss_depth=1.6,
    holder_boss_pilot_d=1.8,
    holder_shell_overlap=2,
    front_clearance=0.5,
    back_clearance=1
) {
    usb_c_mount_axis(anchor, mount_angle)
        usb_c_port_cutout_local(
            port_half_spacing=port_half_spacing,
            port_end_d=port_end_d,
            port_cut_depth=port_cut_depth,
            outer_overshoot=outer_overshoot
        );

    usb_c_mount_axis(anchor, mount_angle)
        usb_c_holder_boss_holes_local(
            shell_thickness=shell_thickness,
            holder_bolt_half_spacing=holder_bolt_half_spacing,
            holder_boss_depth=holder_boss_depth,
            holder_boss_pilot_d=holder_boss_pilot_d,
            holder_shell_overlap=holder_shell_overlap,
            front_clearance=front_clearance,
            back_clearance=back_clearance
        );
}

/*
Example: reproduce the touch_clock placement on the spherical shell.

usb_anchor = sphere_rear_port_anchor(body_d=152, rear_port_z=-40);

difference() {
    union() {
        shell();
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
*/
