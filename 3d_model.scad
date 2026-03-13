/* Retro-Futuristic Sphere Clock (125.1mm Display Scale)
   - Display scaled to 125.1mm
   - Screen opening uses a squared-off recessed bezel
   - 4mm body wall with an internal support ring for the display
   - USB port remains a separate modular insert for cleaner printing
   - Internal base floor includes a parametric chamfer
*/

/* [Render Options] */
part_to_render = "all"; // ["all": "Full Assembly", "body": "Clock Body Only", "stand": "Stand Only", "plate": "Bottom Pad Only", "insert": "USB Port Insert Only"]
quality_profile = "preview"; // ["preview": "Browser Preview", "final": "Export / Print"]

/* [Hardware / Connection Parameters] */
m3_hole_d = 3.4;
m3_nut_d = 6.7;
m3_nut_depth = 2.5;
m3_bolt_head_d = 7;
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

/* [Clock Body Interior] */
body_is_hollow = true;
body_wall_thickness = 4;
floor_chamfer_width = 9;
floor_chamfer_height = 4;

/* [Stand Parameters] */
stand_height = 45;
stand_base_radius = 54;
stand_base_radius_bottom = 50;
stand_chamfer_height = 11;
stand_top_radius = 40;
stand_neck_radius = 18;
stand_neck_height = 25;
stand_bottom_curve = 2.5;
stand_top_curve = 2.5;
stand_is_hollow = false;
stand_wall_thickness = 4;
stand_top_rounding_radius = 1.0;

/* [Plate Parameters] */
plate_thickness = 10;
plate_bottom_thickness = 3;
plate_flange_width = 2.5;
plate_clearance = 0.3;

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

/* [USB Insert Parameters] */
rear_port_z = -40;
usb_insert_hole_d = 20;
usb_insert_clearance = 0.3;
usb_insert_body_d = usb_insert_hole_d - usb_insert_clearance;
usb_insert_body_thickness = body_wall_thickness;
usb_insert_flange_d = 24;
usb_insert_flange_thickness = 2;
usb_insert_cut_depth = 30;

usb_port_half_spacing = 3.25;
usb_port_end_d = 3.5;
usb_port_cut_depth = 10;

usb_recess_inner_half_spacing = 4.5;
usb_recess_inner_d = 6;
usb_recess_depth = 3;
usb_recess_outer_half_spacing = 7;
usb_recess_outer_d = 12;

// ==========================================
// SYSTEM & DERIVED CALCULATIONS
// ==========================================

preview_fn = 100;
final_fn = 300;
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

module validate_parameters() {
    assert(
        part_to_render == "all" ||
        part_to_render == "body" ||
        part_to_render == "stand" ||
        part_to_render == "plate" ||
        part_to_render == "insert",
        "part_to_render must be one of: all, body, stand, plate, insert"
    );
    assert(
        quality_profile == "preview" || quality_profile == "final",
        "quality_profile must be preview or final"
    );
    assert(display_d < body_d, "display_d must stay smaller than body_d");
    assert(stand_top_radius < sphere_radius(body_d), "stand_top_radius must fit within the body sphere");
    assert(abs(rear_port_z) < sphere_radius(body_d), "rear_port_z must intersect the sphere");
    assert(body_wall_thickness > 0 && body_wall_thickness < sphere_radius(body_d), "body_wall_thickness is out of range");
    assert(stand_wall_thickness >= 0, "stand_wall_thickness must be non-negative");
    assert(stand_chamfer_height > 0, "stand_chamfer_height must be greater than zero");
    assert(stand_neck_height > stand_chamfer_height, "stand_neck_height must exceed stand_chamfer_height");
    assert(stand_height > stand_top_rounding_radius, "stand_height must exceed stand_top_rounding_radius");
    assert(screen_thickness > 0, "screen_thickness must be greater than zero");
    assert(screen_ledge_margin_d < display_d, "screen_ledge_margin_d must stay smaller than display_d");
    assert(access_slot_outer_r > access_slot_inner_r, "access_slot_outer_r must exceed access_slot_inner_r");
    assert(usb_insert_hole_d > usb_insert_clearance, "usb_insert_clearance is larger than the hole");
    assert(speaker_ring_count >= 0, "speaker_ring_count must be non-negative");
}

cut_dist = chord_offset_for_diameter(body_d, display_d);
bottom_cut_dist = chord_offset_for_diameter(body_d, stand_top_radius * 2);
body_z = stand_height + bottom_cut_dist;

vase_chamfer_slope = (stand_base_radius - stand_base_radius_bottom) / stand_chamfer_height;
vase_rad_at_plate_height = stand_base_radius_bottom + (vase_chamfer_slope * plate_thickness);
plate_inner_radius_top = vase_rad_at_plate_height + plate_clearance;
plate_outer_radius_top = plate_inner_radius_top + plate_flange_width;
plate_outer_radius_bottom = plate_outer_radius_top - (vase_chamfer_slope * plate_thickness);
plate_outer_radius_floor = plate_outer_radius_bottom - (vase_chamfer_slope * plate_bottom_thickness);

rear_port_y_outer = rear_surface_y(body_d, rear_port_z);

validate_parameters();

// ==========================================
// RENDER ROUTER
// ==========================================

if (part_to_render == "all") {
    color("Silver") stand_part();
    color("Gold") plate();
    color("Gold") translate([0, 0, body_z]) clock_body();
    color("DarkSlateGray")
        translate([0, rear_port_y_outer - 15, body_z + rear_port_z])
            rotate([-90, 0, 0])
                usb_insert();
} else if (part_to_render == "body") {
    translate([0, 0, bottom_cut_dist]) clock_body();
} else if (part_to_render == "stand") {
    stand_part();
} else if (part_to_render == "plate") {
    plate();
} else if (part_to_render == "insert") {
    usb_insert();
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
        rotate([90, 0, 0])
            children();
}

module stand_part() {
    difference() {
        vase();

        translate([0, 0, -1])
            cylinder(d=m3_hole_d, h=stand_height + 2);

        translate([0, 0, -1])
            cylinder(d=m3_bolt_head_d, h=stand_height - stand_ceiling_thickness + 1);
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

module screen_support_ring(pocket_bottom) {
    screen_axis()
        translate([0, 0, pocket_bottom - screen_ledge_backset])
            difference() {
                cylinder(d=body_d, h=screen_ledge_thickness);
                translate([0, 0, -1])
                    cylinder(d=display_d - screen_ledge_margin_d, h=screen_ledge_thickness + 2);
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
            cylinder(d=usb_insert_hole_d, h=usb_insert_cut_depth, center=true);
    }
}

module usb_insert() {
    difference() {
        union() {
            cylinder(d=usb_insert_body_d, h=usb_insert_body_thickness);
            translate([0, 0, usb_insert_body_thickness])
                cylinder(d=usb_insert_flange_d, h=usb_insert_flange_thickness);
        }

        hull() {
            translate([-usb_port_half_spacing, 0, -1])
                cylinder(d=usb_port_end_d, h=usb_port_cut_depth);
            translate([usb_port_half_spacing, 0, -1])
                cylinder(d=usb_port_end_d, h=usb_port_cut_depth);
        }

        hull() {
            translate([-usb_recess_inner_half_spacing, 0, usb_recess_depth])
                cylinder(d=usb_recess_inner_d, h=0.1, center=true);
            translate([usb_recess_inner_half_spacing, 0, usb_recess_depth])
                cylinder(d=usb_recess_inner_d, h=0.1, center=true);

            translate([-usb_recess_outer_half_spacing, 0, -0.1])
                cylinder(d=usb_recess_outer_d, h=0.1, center=true);
            translate([usb_recess_outer_half_spacing, 0, -0.1])
                cylinder(d=usb_recess_outer_d, h=0.1, center=true);
        }
    }
}

module vase_profile(h, b_rad, b_rad_bot, c_h, t_rad, n_rad, n_h, b_curve, t_curve) {
    profile_steps = $fn * 2;
    h_rescaled_end = h - stand_top_rounding_radius;

    curve_points = [
        for (i = [0 : profile_steps - 1])
        let (
            z_step = h_rescaled_end / (profile_steps - 1),
            z = i * z_step,
            r = (z <= c_h)
                ? b_rad_bot + (b_rad - b_rad_bot) * (z / c_h)
                : (z < n_h)
                    ? n_rad + (b_rad - n_rad) * pow((n_h - z) / (n_h - c_h), b_curve)
                    : n_rad + (t_rad - n_rad) * pow((z - n_h) / (h_rescaled_end - n_h), t_curve)
        )
        [r, z]
    ];

    corner_r = t_rad - stand_top_rounding_radius;
    corner_z = h - stand_top_rounding_radius;

    corner_points = [
        for (j = [1 : profile_corner_steps])
        let (
            a = 90 * j / profile_corner_steps,
            x = corner_r + stand_top_rounding_radius * cos(a),
            y = corner_z + stand_top_rounding_radius * sin(a)
        )
        [x, y]
    ];

    polygon(concat([[0, 0]], curve_points, corner_points, [[0, h]]));
}

module vase() {
    if (stand_is_hollow) {
        difference() {
            rotate_extrude()
                vase_profile(
                    stand_height,
                    stand_base_radius,
                    stand_base_radius_bottom,
                    stand_chamfer_height,
                    stand_top_radius,
                    stand_neck_radius,
                    stand_neck_height,
                    stand_bottom_curve,
                    stand_top_curve
                );

            translate([0, 0, stand_wall_thickness])
                rotate_extrude()
                    vase_profile(
                        stand_height,
                        max(0.1, stand_base_radius - stand_wall_thickness),
                        max(0.1, stand_base_radius_bottom - stand_wall_thickness),
                        stand_chamfer_height,
                        max(0.1, stand_top_radius - stand_wall_thickness),
                        max(0.1, stand_neck_radius - stand_wall_thickness),
                        stand_neck_height,
                        stand_bottom_curve,
                        stand_top_curve
                    );
        }
    } else {
        rotate_extrude()
            vase_profile(
                stand_height,
                stand_base_radius,
                stand_base_radius_bottom,
                stand_chamfer_height,
                stand_top_radius,
                stand_neck_radius,
                stand_neck_height,
                stand_bottom_curve,
                stand_top_curve
            );
    }
}

module plate_profile(thickness, floor_thickness, r_bot_in, r_top_in, r_top_out, r_floor_out) {
    polygon([
        [0, -floor_thickness],
        [r_floor_out, -floor_thickness],
        [r_top_out, thickness],
        [r_top_in, thickness],
        [r_bot_in, 0],
        [0, 0]
    ]);
}

module plate() {
    difference() {
        rotate_extrude()
            plate_profile(
                plate_thickness,
                plate_bottom_thickness,
                stand_base_radius_bottom + plate_clearance,
                plate_inner_radius_top,
                plate_outer_radius_top,
                plate_outer_radius_floor
            );

        translate([0, 0, -plate_bottom_thickness - 1])
            cylinder(d=m3_bolt_head_access_d, h=plate_thickness + plate_bottom_thickness + 2);
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
