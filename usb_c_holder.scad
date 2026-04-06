/*
   Panel-Mount USB-C Female Holder - Stepped Trap Version
   Designed to lock a bare PCB/Ribbon cable connector in place.
*/

// --- Parameters ---

// [Part Selection]
part = "assembled";

// Panel Mount Flange (The front "ears")
flange_width = 28.0;
flange_height = 10.0;
flange_thickness = 1.8;

// Hardware & Screw Holes
screw_spacing = 20.0;
screw_diameter = 1.8;

// Assembly Ears (To clamp the two halves together)
assembly_ear_width = 2.0;
assembly_ear_length = 4.0;
assembly_ear_offset = 8.0;

// Main Body Dimensions
body_width = 12.0;
body_height = 8.0;
body_depth = 18.0;

// --- Internal Cavity Dimensions ---

// 1. USB Metal Shroud (Front Zone)
usb_width = 9.1;
usb_height = 3.2;
usb_depth = 2.6;

// 2. PCB & Solder Trap (Middle Zone)
pcb_width = 9.0;
pcb_height = 3.2;
pcb_length = 9.0;

// 3. Ribbon Cable Pass-through (Back Zone)
cable_width = 7.6;
cable_thickness = 1.0;

// Smoothness
$fn = 150;

module usb_c_holder() {
    difference() {
        union() {
            hull() {
                translate([-(flange_width / 2) + (flange_height / 2), 0, 0])
                    cylinder(d=flange_height, h=flange_thickness);
                translate([(flange_width / 2) - (flange_height / 2), 0, 0])
                    cylinder(d=flange_height, h=flange_thickness);
            }

            translate([-body_width / 2, -body_height / 2, 0])
                cube([body_width, body_height, body_depth]);

            hull() {
                translate([-body_width / 2, -body_height / 2, assembly_ear_offset])
                    cube([0.1, body_height, assembly_ear_length]);
                translate([-body_width / 2 - assembly_ear_width + (assembly_ear_length / 2), 0, assembly_ear_offset + assembly_ear_length / 2])
                    rotate([90, 0, 0])
                        cylinder(d=assembly_ear_length, h=body_height, center=true);
            }

            hull() {
                translate([body_width / 2 - 0.1, -body_height / 2, assembly_ear_offset])
                    cube([0.1, body_height, assembly_ear_length]);
                translate([body_width / 2 + assembly_ear_width - (assembly_ear_length / 2), 0, assembly_ear_offset + assembly_ear_length / 2])
                    rotate([90, 0, 0])
                        cylinder(d=assembly_ear_length, h=body_height, center=true);
            }
        }

        translate([-screw_spacing / 2, 0, -1])
            cylinder(d=screw_diameter, h=flange_thickness + 2);
        translate([screw_spacing / 2, 0, -1])
            cylinder(d=screw_diameter, h=flange_thickness + 2);

        translate([-body_width / 2 - assembly_ear_width + (assembly_ear_length / 2), body_height / 2 + 1, assembly_ear_offset + assembly_ear_length / 2])
            rotate([90, 0, 0])
                cylinder(d=screw_diameter, h=body_height + 2);
        translate([body_width / 2 + assembly_ear_width - (assembly_ear_length / 2), body_height / 2 + 1, assembly_ear_offset + assembly_ear_length / 2])
            rotate([90, 0, 0])
                cylinder(d=screw_diameter, h=body_height + 2);

        translate([0, 0, -1])
            hull() {
                translate([-(usb_width / 2) + (usb_height / 2), 0, 0])
                    cylinder(d=usb_height, h=usb_depth + 1);
                translate([(usb_width / 2) - (usb_height / 2), 0, 0])
                    cylinder(d=usb_height, h=usb_depth + 1);
            }

        translate([-pcb_width / 2, -pcb_height / 2, usb_depth - 0.1])
            cube([pcb_width, pcb_height, pcb_length]);

        translate([-cable_width / 2, -cable_thickness / 2, usb_depth + pcb_length - 0.2])
            cube([cable_width, cable_thickness, body_depth]);
    }
}

module top_half() {
    intersection() {
        usb_c_holder();
        translate([-50, 0, -10])
            cube([100, 50, 100]);
    }
}

module bottom_half() {
    intersection() {
        usb_c_holder();
        translate([-50, -50, -10])
            cube([100, 50, 100]);
    }
}

if (part == "assembled") {
    color("LightSteelBlue") top_half();
    color("SlateGray") bottom_half();
} else if (part == "exploded") {
    translate([0, 3, 0])
        color("LightSteelBlue") top_half();
    translate([0, -3, 0])
        color("SlateGray") bottom_half();
} else if (part == "top") {
    rotate([90, 0, 0])
        top_half();
} else if (part == "bottom") {
    rotate([-90, 0, 0])
        bottom_half();
} else if (part == "flat_for_printing") {
    translate([0, 15, 0])
        rotate([90, 0, 0])
            top_half();
    translate([0, -15, 0])
        rotate([-90, 0, 0])
            bottom_half();
}
