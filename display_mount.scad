/*
   Display Mount Plate
   Based on the hand sketch:
   - 2 mounting holes
   - 75 mm center-to-center spacing
   - 4.2 mm hole diameter
   - Hole centers shifted 3 mm upward from the original centered position
*/

/* [Part Parameters] */
plate_thickness = 1.7;
ear_thickness = 1.5;
center_strip_thickness = 5.7;
plate_offset_x = 0.0;
plate_offset_y = -4.5;
plate_offset_z = 4;
ear_offset_x = 0.0;
ear_offset_y = 0.0;
ear_offset_z = 0.0;
center_strip_offset_x = 0.0;
center_strip_offset_y = -5.0;
center_strip_offset_z = 0.0;
hole_spacing = 75.0;
hole_diameter = 4.0;
bottom_hole_diameter = 1.9;
bottom_hole_angle = -20;
hole_center_y = 3.0;
ear_raise = 3.0;
plate_drop = 0.0;
bottom_hole_y = -8;
ear_overlap = 1.5;

/* [Shape Parameters] */
ear_radius = 4.5;
top_join_angle = 20;
bottom_join_angle = 350;
body_base_center_y = -4.65;
body_thickness = 23.7;
center_strip_width = 66.0;
center_strip_height = 4.0;

/* [Sampling] */
curve_steps = 64;

/* [Render Quality] */
$fn = 180;

half_hole_spacing = hole_spacing / 2;
left_body_ear = [-half_hole_spacing, hole_center_y];
right_body_ear = [half_hole_spacing, hole_center_y];
left_hole = [left_body_ear[0], left_body_ear[1] + ear_raise];
right_hole = [right_body_ear[0], right_body_ear[1] + ear_raise];
bottom_hole = [0, bottom_hole_y];
join_marker_r = 1.0;
body_center_y = body_base_center_y - plate_drop;
top_bridge_y = body_center_y + body_thickness / 2;
bottom_bridge_y = body_center_y - body_thickness / 2;

function point_on_circle(center, radius, angle) =
    [center[0] + radius * cos(angle), center[1] + radius * sin(angle)];

function cubic_bezier_point(p0, p1, p2, p3, t) =
    let(u = 1 - t)
    [
        u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0],
        u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1]
    ];

function bezier_points(p0, p1, p2, p3, steps, include_last=true) =
    [for (i = [0 : include_last ? steps : steps - 1]) cubic_bezier_point(p0, p1, p2, p3, i / steps)];

left_top_join = point_on_circle(left_body_ear, ear_radius, top_join_angle);
right_top_join = point_on_circle(right_body_ear, ear_radius, 180 - top_join_angle);
left_bottom_join = point_on_circle(left_body_ear, ear_radius, bottom_join_angle);
right_bottom_join = point_on_circle(right_body_ear, ear_radius, 180 - bottom_join_angle);
left_top_anchor = [left_top_join[0] + ear_overlap, left_top_join[1]];
right_top_anchor = [right_top_join[0] - ear_overlap, right_top_join[1]];
left_bottom_anchor = [left_bottom_join[0] + ear_overlap, left_bottom_join[1]];
right_bottom_anchor = [right_bottom_join[0] - ear_overlap, right_bottom_join[1]];

top_curve = bezier_points(
    left_top_join,
    [left_top_join[0] + 18, top_bridge_y + 0.2],
    [right_top_join[0] - 18, top_bridge_y + 0.2],
    right_top_join,
    curve_steps,
    true
);

bottom_curve = bezier_points(
    left_bottom_join,
    [left_bottom_join[0] + 18, bottom_bridge_y],
    [right_bottom_join[0] - 18, bottom_bridge_y],
    right_bottom_join,
    curve_steps,
    true
);

web_profile = concat(
    top_curve,
    [for (i = [len(bottom_curve) - 1 : -1 : 0]) bottom_curve[i]]
);

module web_2d() {
    polygon(points=web_profile);
}

module ears_2d() {
    hull() {
        translate(left_hole)
            circle(r=ear_radius);
        translate(left_top_anchor)
            circle(r=join_marker_r);
        translate(left_bottom_anchor)
            circle(r=join_marker_r);
    }

    hull() {
        translate(right_hole)
            circle(r=ear_radius);
        translate(right_top_anchor)
            circle(r=join_marker_r);
        translate(right_bottom_anchor)
            circle(r=join_marker_r);
    }
}

module ear_holes_2d() {
    for (hole_center = [left_hole, right_hole]) {
        translate(hole_center)
            circle(d=hole_diameter);
    }
}

module bottom_hole_2d() {
    translate(bottom_hole)
        circle(d=bottom_hole_diameter);
}

module center_strip_2d() {
    translate([-center_strip_width / 2, top_bridge_y - center_strip_height / 2])
        square([center_strip_width, center_strip_height]);
}

difference() {
    bottom_hole_cut_len = plate_thickness + bottom_hole_diameter + 4;

    union() {
        translate([plate_offset_x, plate_offset_y, plate_offset_z])
            linear_extrude(height=plate_thickness)
                web_2d();

        translate([ear_offset_x, ear_offset_y, ear_offset_z])
            linear_extrude(height=ear_thickness)
                ears_2d();

        translate([center_strip_offset_x, center_strip_offset_y, center_strip_offset_z])
            linear_extrude(height=center_strip_thickness)
                center_strip_2d();
    }

    translate([ear_offset_x, ear_offset_y, ear_offset_z - 0.1])
        linear_extrude(height=ear_thickness + 0.2)
            ear_holes_2d();

    translate([
        plate_offset_x + bottom_hole[0],
        plate_offset_y + bottom_hole[1],
        plate_offset_z + (plate_thickness / 2)
    ])
        rotate([bottom_hole_angle, 0, 0])
            cylinder(d=bottom_hole_diameter, h=bottom_hole_cut_len, center=true);
}
