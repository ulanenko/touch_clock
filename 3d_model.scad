/* Retro-Futuristic Sphere Clock (125.1mm Display Scale)
   - Display scaled to 125.1mm
   - Screen opening uses a squared-off recessed bezel
   - 4mm body wall with an internal support ring for the display
   - USB port is cut directly into the body shell
   - Stand replaced with a 3-part modular pedestal using tapered hex joints
*/

/* [Render Options] */
part_to_render = "body"; // ["all": "Full Assembly", "body": "Clock Body Only", "stand": "Assembled Stand Only", "stand_base": "Base Plate Only", "stand_stem": "Stem Only", "stand_top": "Top Plate Only", "stand_print": "Stand Print Layout"]
quality_profile = "final"; // ["preview": "Browser Preview", "final": "Export / Print"]

/* [Hardware / Connection Parameters] */
m3_hole_d = 3.4;
m3_nut_d = 6.7;
m3_nut_depth = 2.5;
m3_bolt_head_access_d = 9.0;
stand_ceiling_thickness = 3;
body_floor_thickness = 4;

/* [Clock Body Parameters] */
display_nominal_d = 125.1;
display_fit_clearance = 0.7;
display_d = display_nominal_d + display_fit_clearance;
body_d = 152;
screen_tilt = 15;
screen_thickness = 5.4;
screen_bezel_inset = 1.5;
screen_pocket_clearance = 0.4;
screen_ledge_margin_d = 6;
screen_ledge_backset = 5;
screen_ledge_thickness = 5.1;
screen_ear_hole_d = 1.9;
screen_ear_hole_angle = 20;
screen_ear_margin = 2;
screen_ear_bridge_w = 2;
screen_ear_base_w = 4.2;
screen_ear_base_depth = 1.6;
screen_ear_overlap = 0;
screen_ear_thickness = 1.2;

/* [Clock Body Interior] */
body_is_hollow = true;
body_wall_thickness = 4;
floor_chamfer_width = 9;
floor_chamfer_height = 4;

/* [Stand Parameters] */
stand_height = 45;
stand_base_diameter = 95;
stand_base_thickness = 9;
stand_top_diameter = 78;
stand_top_thickness = 2.5;
anti_rotation_hole_d = 1.8;
anti_rotation_hole_radius = 14;
anti_rotation_hole_angle = 0;
anti_rotation_hole_depth = 7;
anti_rotation_hole_offset_x = 0;
anti_rotation_hole_offset_y = -4.0;
anti_rotation_hole_offset_z = 0;
stem_diameter = 22;
stem_height = stand_height - stand_base_thickness - stand_top_thickness;

/* [Rigid Joint Parameters] */
joint_depth = 12;
joint_bottom_d = 16;
joint_top_d = 14;
fit_tolerance = 0.3;

/* [Access Slot Parameters] */
access_slot_outer_r = 30;
access_slot_inner_r = 10;
access_slot_angle = 120;
access_slot_extra_depth = 4;

/* [Speaker Grill Parameters] */
speaker_hole_d = 2.0;
speaker_ring_spacing = 3.5;
speaker_ring_count = 5;
speaker_depth = 15;
speaker_face_margin = 4;

/* [USB Port Parameters] */
rear_port_z = -40;
usb_mount_angle = 35;
usb_port_half_spacing = 3.25;
usb_port_end_d = 3.5;
usb_port_cut_depth = 10;

/* [USB Mounting Holes Parameters] */
usb_holder_bolt_half_spacing = 10;
usb_holder_boss_d = 6;
usb_holder_boss_depth = 1.6;
usb_holder_boss_pilot_d = 1.8;
usb_holder_shell_overlap = 2;

// ==========================================
// SYSTEM & DERIVED CALCULATIONS
// ==========================================

preview_fn = 100;
final_fn = 400;
$fn = quality_profile == "final" ? final_fn : preview_fn;
access_slot_sweep_fn = quality_profile == "final" ? 100 : 100;
profile_corner_steps = quality_profile == "final" ? 20 : 20;
speaker_hole_fn = quality_profile == "final" ? 18 : 18;
preview_speaker_ring_count = speaker_ring_count;
active_speaker_ring_count = quality_profile == "final"
    ? speaker_ring_count
    : min(speaker_ring_count, preview_speaker_ring_count);

function sphere_radius(d) = d / 2;
function chord_offset_for_diameter(sphere_d, chord_d) =
    sqrt(pow(sphere_radius(sphere_d), 2) - pow(chord_d / 2, 2));
function sphere_radius_at_z(sphere_d, z) =
    sqrt(max(0, pow(sphere_radius(sphere_d), 2) - pow(z, 2)));
function rear_surface_y(sphere_d, z) = -sphere_radius_at_z(sphere_d, z);
function stand_top_body_curve_depth(r) =
    sqrt(max(0, pow(sphere_radius(body_d), 2) - pow(r, 2))) - bottom_cut_dist;
function screen_ledge_inner_r() = (display_d - screen_ledge_margin_d) / 2;
function screen_ear_pad_d() = screen_ear_hole_d + (screen_ear_margin * 2);
function screen_ear_center_y() = screen_ledge_inner_r() - (screen_ear_pad_d() / 2) + screen_ear_overlap;
function screen_ear_bridge_x() = (screen_ear_hole_d + screen_ear_bridge_w) / 2;
function screen_ear_bridge_anchor_y() = screen_ledge_inner_r() + (screen_ear_bridge_w / 2);
function screen_ear_base_y() = screen_ear_bridge_anchor_y() - screen_ear_base_depth;

module validate_parameters() {
    assert(
        part_to_render == "all" ||
        part_to_render == "body" ||
        part_to_render == "stand" ||
        part_to_render == "stand_base" ||
        part_to_render == "stand_stem" ||
        part_to_render == "stand_top" ||
        part_to_render == "stand_print",
        "part_to_render must be one of: all, body, stand, stand_base, stand_stem, stand_top, stand_print"
    );
    assert(
        quality_profile == "preview" || quality_profile == "final",
        "quality_profile must be preview or final"
    );
    assert(display_d < body_d, "display_d must stay smaller than body_d");
    assert(stand_top_diameter < body_d, "stand_top_diameter must stay smaller than body_d");
    assert(abs(rear_port_z) < sphere_radius(body_d), "rear_port_z must intersect the sphere");
    assert(body_wall_thickness > 0 && body_wall_thickness < sphere_radius(body_d), "body_wall_thickness is out of range");
    assert(stand_base_thickness > 0, "stand_base_thickness must be greater than zero");
    assert(stand_top_thickness > 0, "stand_top_thickness must be greater than zero");
    assert(anti_rotation_hole_d > 0, "anti_rotation_hole_d must be greater than zero");
    assert(anti_rotation_hole_radius > (joint_bottom_d / 2) + (anti_rotation_hole_d / 2), "anti_rotation_hole_radius must clear the center joint");
    assert(anti_rotation_hole_radius < (stand_top_diameter / 2) - (anti_rotation_hole_d / 2), "anti_rotation_hole_radius must stay inside the stand top");
    assert(anti_rotation_hole_depth > 0, "anti_rotation_hole_depth must be greater than zero");
    assert(anti_rotation_hole_depth < stand_top_body_curve_depth(anti_rotation_hole_radius), "anti_rotation_hole_depth is deeper than the stand top at the chosen radius");
    assert(joint_depth > 0, "joint_depth must be greater than zero");
    assert(stem_height > (joint_depth * 2), "stem_height must exceed twice the joint depth");
    assert(screen_thickness > 0, "screen_thickness must be greater than zero");
    assert(screen_ledge_margin_d < display_d, "screen_ledge_margin_d must stay smaller than display_d");
    assert(screen_ear_hole_d > 0, "screen_ear_hole_d must be greater than zero");
    assert(abs(screen_ear_hole_angle) < 85, "screen_ear_hole_angle must stay between -85 and 85 degrees");
    assert(screen_ear_margin > 0, "screen_ear_margin must be greater than zero");
    assert(screen_ear_bridge_w > 0, "screen_ear_bridge_w must be greater than zero");
    assert(screen_ear_base_w > 0, "screen_ear_base_w must be greater than zero");
    assert(screen_ear_base_depth >= 0, "screen_ear_base_depth must be non-negative");
    assert(screen_ear_overlap >= 0, "screen_ear_overlap must be non-negative");
    assert(screen_ear_thickness > 0 && screen_ear_thickness <= screen_ledge_thickness, "screen_ear_thickness must be between 0 and screen_ledge_thickness");
    assert(access_slot_outer_r > access_slot_inner_r, "access_slot_outer_r must exceed access_slot_inner_r");
    assert(abs(usb_mount_angle) < 85, "usb_mount_angle must stay below 85 degrees");
    assert(usb_port_end_d > 0, "usb_port_end_d must be greater than zero");
    assert(usb_port_cut_depth > 0, "usb_port_cut_depth must be greater than zero");
    assert(usb_holder_boss_depth > 0, "usb_holder_boss_depth must be greater than zero");
    assert(usb_holder_boss_d > usb_holder_boss_pilot_d, "usb_holder_boss_d must exceed the M2 pilot hole");
    assert(speaker_ring_count >= 0, "speaker_ring_count must be non-negative");
}

cut_dist = chord_offset_for_diameter(body_d, display_d);
bottom_cut_dist = chord_offset_for_diameter(body_d, stand_top_diameter);
body_z = stand_height + bottom_cut_dist;

rear_port_y_outer = rear_surface_y(body_d, rear_port_z);

validate_parameters();

// ==========================================
// RENDER ROUTER
// ==========================================

if (part_to_render == "all") {
    color("DarkGray") base_piece();
    color("Silver") translate([0, 0, stand_base_thickness]) stem_piece();
    color("DarkGray") translate([0, 0, stand_height]) rotate([180, 0, 0]) top_piece();
    color("Gold") translate([0, 0, body_z]) clock_body();
} else if (part_to_render == "body") {
    translate([0, 0, bottom_cut_dist]) clock_body();
} else if (part_to_render == "stand") {
    stand_part();
} else if (part_to_render == "stand_base") {
    base_piece();
} else if (part_to_render == "stand_stem") {
    stem_piece();
} else if (part_to_render == "stand_top") {
    top_piece();
} else if (part_to_render == "stand_print") {
    stand_print_layout();
}

// ==========================================
// MODULES
// ==========================================

module screen_axis() {
    rotate([screen_tilt, 0, 0])
        rotate([-90, 0, 0])
            children();
}

module rear_port_axis() {
    translate([0, rear_port_y_outer, rear_port_z])
        rotate([90 + usb_mount_angle, 0, 0])
            children();
}

module usb_port_hole() {
    outer_overshoot = 1;
    cut_start_z = -usb_port_cut_depth - outer_overshoot;
    cut_height = usb_port_cut_depth + outer_overshoot * 2;

    hull() {
        translate([-usb_port_half_spacing, 0, cut_start_z])
            cylinder(d=usb_port_end_d, h=cut_height);
        translate([usb_port_half_spacing, 0, cut_start_z])
            cylinder(d=usb_port_end_d, h=cut_height);
    }
}

module usb_holder_bosses() {
    boss_front_z = -body_wall_thickness - usb_holder_boss_depth;
    boss_height = usb_holder_boss_depth + usb_holder_shell_overlap;

    for (x = [-usb_holder_bolt_half_spacing, usb_holder_bolt_half_spacing]) {
        translate([x, 0, boss_front_z])
            cylinder(d=usb_holder_boss_d, h=boss_height);
    }
}

module usb_holder_boss_holes() {
    boss_front_z = -body_wall_thickness - usb_holder_boss_depth - 0.5;
    boss_hole_height = usb_holder_boss_depth + usb_holder_shell_overlap + 1;

    rear_port_axis()
        for (x = [-usb_holder_bolt_half_spacing, usb_holder_bolt_half_spacing]) {
            translate([x, 0, boss_front_z])
                cylinder(d=usb_holder_boss_pilot_d, h=boss_hole_height);
        }
}

module stand_part() {
    color("DarkGray") base_piece();
    color("Silver") translate([0, 0, stand_base_thickness]) stem_piece();
    color("DarkGray") translate([0, 0, stand_height]) rotate([180, 0, 0]) top_piece();
}

module stand_print_layout() {
    translate([-stand_base_diameter / 2 - 10, 0, 0])
        base_piece();
    translate([stand_base_diameter / 2 + 10, 0, 0])
        stem_piece();
    translate([0, stand_base_diameter / 2 + stand_top_diameter / 2 + 15, 0])
        top_piece();
}

module base_piece() {
    difference() {
        union() {
            hull() {
                cylinder(d=stand_base_diameter, h=stand_base_thickness - 1);
                translate([0, 0, stand_base_thickness - 1])
                    cylinder(d=stand_base_diameter - 2, h=1);
            }

            translate([0, 0, stand_base_thickness])
                cylinder(h=joint_depth, d1=joint_bottom_d, d2=joint_top_d, $fn=6);
        }

        translate([0, 0, -1])
            cylinder(d=m3_bolt_head_access_d, h=stand_base_thickness + joint_depth + 2);
    }
}

module top_piece_profile() {
    outer_r = stand_top_diameter / 2;
    curve_steps = profile_corner_steps * 4;

    polygon(concat(
        [
            [0, 0],
            [outer_r, 0]
        ],
        [
            for (i = [curve_steps : -1 : 0])
            let(
                t = i / curve_steps,
                r = outer_r * t,
                z = stand_top_body_curve_depth(r)
            )
            [r, z]
        ]
    ));
}

module anti_rotation_hole_positions() {
    for (side = [-1, 1]) {
        translate([
            anti_rotation_hole_offset_x + side * anti_rotation_hole_radius * cos(anti_rotation_hole_angle),
            anti_rotation_hole_offset_y + side * anti_rotation_hole_radius * sin(anti_rotation_hole_angle),
            anti_rotation_hole_offset_z
        ])
            children();
    }
}

module top_piece() {
    difference() {
        union() {
            rotate_extrude()
                top_piece_profile();

            translate([0, 0, stand_top_thickness])
                cylinder(h=joint_depth, d1=joint_bottom_d, d2=joint_top_d, $fn=6);
        }

        translate([0, 0, stand_ceiling_thickness])
            cylinder(d=m3_bolt_head_access_d, h=stand_top_thickness + joint_depth + 2);

        translate([0, 0, -1])
            cylinder(d=m3_hole_d, h=stand_ceiling_thickness + 2);

        anti_rotation_hole_positions()
            translate([0, 0, -0.1])
                cylinder(d=anti_rotation_hole_d, h=anti_rotation_hole_depth + 0.1);
    }
}

module stem_piece() {
    difference() {
        cylinder(d=stem_diameter, h=stem_height);

        translate([0, 0, -0.1])
            cylinder(
                h=joint_depth + 0.5,
                d1=joint_bottom_d + fit_tolerance,
                d2=joint_top_d + fit_tolerance,
                $fn=6
            );

        translate([0, 0, stem_height + 0.1])
            rotate([180, 0, 0])
                cylinder(
                    h=joint_depth + 0.5,
                    d1=joint_bottom_d + fit_tolerance,
                    d2=joint_top_d + fit_tolerance,
                    $fn=6
                );

        translate([0, 0, -1])
            cylinder(d=m3_bolt_head_access_d, h=stem_height + 2);
    }
}

module curved_access_slot() {
    arc_width = access_slot_outer_r - access_slot_inner_r;
    arc_center_r = access_slot_inner_r + (arc_width / 2);
    start_angle = 90 - (access_slot_angle / 2);
    slot_depth = body_floor_thickness + access_slot_extra_depth;

    translate([0, 0, -bottom_cut_dist - 1]) {
        rotate([0, 0, start_angle]) {
            rotate_extrude(angle=access_slot_angle, $fn=access_slot_sweep_fn)
                translate([access_slot_inner_r, 0])
                    square([arc_width, slot_depth]);

            translate([arc_center_r, 0, 0])
                cylinder(d=arc_width, h=slot_depth);

            rotate([0, 0, access_slot_angle])
                translate([arc_center_r, 0, 0])
                    cylinder(d=arc_width, h=slot_depth);
        }
    }
}

module screen_ear_profile() {
    ear_pad_d = screen_ear_pad_d();
    ear_center_y = screen_ear_center_y();
    bridge_w = screen_ear_bridge_w;
    bridge_x = screen_ear_bridge_x();
    bridge_anchor_y = screen_ear_bridge_anchor_y();
    base_w = screen_ear_base_w;
    base_y = screen_ear_base_y();

    union() {
        translate([0, ear_center_y])
            circle(d=ear_pad_d);

        for (side = [-1, 1]) {
            hull() {
                translate([side * bridge_x, ear_center_y])
                    circle(d=bridge_w);
                translate([side * bridge_x, bridge_anchor_y])
                    circle(d=bridge_w);
            }
        }

        for (side = [-1, 1]) {
            hull() {
                translate([side * bridge_x, bridge_anchor_y])
                    circle(d=base_w);
                translate([side * bridge_x, base_y])
                    circle(d=base_w);
            }
        }

        hull() {
            translate([-bridge_x, base_y])
                circle(d=base_w);
            translate([bridge_x, base_y])
                circle(d=base_w);
        }
    }
}

module screen_support_ring(pocket_bottom) {
    inner_d = display_d - screen_ledge_margin_d;
    ear_center_y = screen_ear_center_y();
    ear_z = 0;
    hole_cut_len = screen_ear_thickness + screen_ear_hole_d + 4;

    screen_axis()
        translate([0, 0, pocket_bottom - screen_ledge_backset])
            difference() {
                cylinder(d=body_d, h=screen_ledge_thickness);
                difference() {
                    translate([0, 0, -1])
                        cylinder(d=inner_d, h=screen_ledge_thickness + 2);
                    translate([0, 0, ear_z - 1])
                        linear_extrude(height=screen_ear_thickness + 2)
                            screen_ear_profile();
                }
                translate([0, ear_center_y, ear_z + (screen_ear_thickness / 2)])
                    rotate([screen_ear_hole_angle, 0, 0])
                        cylinder(d=screen_ear_hole_d, h=hole_cut_len, center=true);
            }
}

module clock_body() {
    pocket_top = cut_dist - screen_bezel_inset;
    pocket_bottom = pocket_top - screen_thickness;

    difference() {
        sphere(d=body_d);

        if (body_is_hollow) {
            inner_r = (body_d - 2 * body_wall_thickness) / 2;
            floor_z = -bottom_cut_dist + body_floor_thickness;
            void_r = sphere_radius_at_z(inner_r * 2, floor_z);
            chamfer_h_safe = max(0.01, floor_chamfer_height);
            chamfer_slope = floor_chamfer_width / chamfer_h_safe;

            difference() {
                intersection() {
                    sphere(r=inner_r);

                    translate([0, 0, floor_z])
                        cylinder(
                            r1=max(0.1, void_r - floor_chamfer_width),
                            r2=max(0.1, void_r - floor_chamfer_width) + (body_d * chamfer_slope),
                            h=body_d
                        );
                }

                screen_support_ring(pocket_bottom);
                rear_port_axis()
                    usb_holder_bosses();
            }
        }

        screen_axis()
            rotate_extrude()
                polygon([
                    [0, pocket_bottom],
                    [display_d / 2 + screen_pocket_clearance, pocket_bottom],
                    [display_d / 2 + screen_pocket_clearance, pocket_top],
                    [body_d, pocket_top],
                    [body_d, body_d],
                    [0, body_d]
                ]);

        translate([0, 0, -bottom_cut_dist - body_d / 2])
            cube([body_d * 2, body_d * 2, body_d], center=true);

        translate([0, 0, -body_d / 2 - 1])
            cylinder(d=m3_hole_d, h=body_d / 2 + 2);

        translate([0, 0, -bottom_cut_dist + body_floor_thickness - m3_nut_depth])
            cylinder(d=m3_nut_d, h=body_d / 2, $fn=6);

        curved_access_slot();
        speaker_holes();

        rear_port_axis()
            usb_port_hole();
        usb_holder_boss_holes();
    }
}

module speaker_hole_pattern() {
    circle(d=speaker_hole_d, $fn=speaker_hole_fn);

    if (active_speaker_ring_count > 0) {
        for (ring = [1 : active_speaker_ring_count]) {
            num_holes = ring * 6;
            for (i = [0 : num_holes - 1]) {
                angle = i * (360 / num_holes);
                radius = ring * speaker_ring_spacing;
                translate([radius * cos(angle), radius * sin(angle)])
                    circle(d=speaker_hole_d, $fn=speaker_hole_fn);
            }
        }
    }
}

module speaker_holes() {
    translate([0, -body_d / 2 + speaker_face_margin, 0])
        rotate([90, 0, 0])
            linear_extrude(height=speaker_depth, center=true, convexity=6)
                speaker_hole_pattern();
}
