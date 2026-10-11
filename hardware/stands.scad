// Desk stands that tilt the Mochi housings back so the screen faces up.
// Each stand is built in the housing's own coordinates (x width, y depth with
// the screen at the front, z up), then tilted by `angle` and cut flat at the
// desk. Print every stand flat on its bottom, no supports.
//
//   openscad -D 'stand="mac_ramp"'    -o out/stand_mac_ramp.stl    stands.scad
//   openscad -D 'stand="mac_floppy"'  -o out/stand_mac_floppy.stl  stands.scad
//   openscad -D 'stand="clawd"'       -o out/stand_clawd.stl       stands.scad
//   add -D 'preview=true' to see the housing sitting on it
//   add -D 'riser=5' (or 10) to lift it, so a long USB-C plug clears the desk
//   Clawd name plate: -D 'label="CLAWD"' -D 'part="body"' (and part="label") -> two colour parts

include <BOSL2/std.scad>
include <clawd_emblem.scad>
use <fonts/ChakraPetch-Bold.ttf>      // free (SIL OFL); strokes ~0.85 mm at this size, easy on a 0.4 nozzle

$fn = 48;

stand   = "mac_ramp";
angle   = 30;        // screen tilt up from vertical
preview = false;

min_t   = 3;         // thinnest part of the wedge (rear edge)
riser   = 0;         // extra height under the wedge, room for the USB-C plug
clear   = 0.3;       // gap around the housing

// --- housings (from mac_housing.scad and clawd_v1_housing.scad) ---
MAC = [[-3, 49], [-6, 48], -11.5];      // x range, y range, bottom z
CLAWD = [[-8.5, 54.5], [-1.5, 48], -10.5];
clawd_u = 10.5;
clawd_legs_x = [for (k = [0.5, 1.5, 4, 5]) -8.5 + k * clawd_u];
clawd_leg_w = clawd_u / 2;
clawd_leg_y = [-1.5, 41];

module tilt() rotate([-angle, 0, 0]) children();

// lowest point of the tilted platform, so the wedge can be cut flat there
function desk_z(y_back, zb) = (y_back) * sin(-angle) + zb * cos(angle) - min_t - riser;

// tilted wedge under a platform: x0..x1, y0..y1 at height zb (housing coords)
module wedge(x0, x1, y0, y1, zb) {
    dz = desk_z(y1, zb);
    yf = y0 * cos(angle) + zb * sin(angle);      // front edge of the platform, tilted
    translate([0, 0, -dz]) intersection() {
        tilt() translate([x0, y0, zb - 200]) cube([x1 - x0, y1 - y0, 200]);
        translate([-500, yf, dz]) cube(1000);    // flat on the desk, vertical front
    }
}

module on_stand(zb, y1) {
    translate([0, 0, -desk_z(y1, zb)]) tilt() children();
}

// ---------------------------------------------------------------- stands

module mac_ramp() {
    m = 4;
    x0 = MAC[0][0] - clear - m; x1 = MAC[0][1] + clear + m;
    y0 = MAC[1][0] - 2; y1 = MAC[1][1] + clear + 3;
    zb = MAC[2];
    wedge(x0, x1, y0, y1, zb);
    on_stand(zb, y1) {
        // back lip, low enough to stay under the USB-C port (16 mm up)
        translate([x0, MAC[1][1] + clear, zb]) cuboid([x1 - x0, 3, 8], chamfer = 1, except = BOTTOM, anchor = FRONT + LEFT + BOTTOM);
        // side guides
        for (x = [x0, x1 - m]) translate([x, MAC[1][0] + 8, zb])
            cuboid([m, MAC[1][1] - MAC[1][0] - 8 + clear, 4], chamfer = 1, except = BOTTOM, anchor = FRONT + LEFT + BOTTOM);
    }
}

module mac_floppy() {
    // the Mac sits on a giant 3.5" floppy disk, shutter showing at the front
    m = 5;
    x0 = MAC[0][0] - clear - m; x1 = MAC[0][1] + clear + m;
    y0 = MAC[1][0] - 16; y1 = MAC[1][1] + clear + 3;
    zb = MAC[2];
    cx = (x0 + x1) / 2;
    difference() {
        union() {
            wedge(x0, x1, y0, y1, zb);
            on_stand(zb, y1)
                translate([x0, MAC[1][1] + clear, zb]) cuboid([x1 - x0, 3, 8], chamfer = 1, except = BOTTOM, anchor = FRONT + LEFT + BOTTOM);
        }
        on_stand(zb, y1) {
            // metal shutter recess with the slot
            translate([cx - 15, y0 - 1, zb - 0.8]) cube([30, 15, 1]);
            translate([cx - 2, y0 + 2, zb - 1.6]) cube([7, 9, 2]);
            // write-protect notch and the cut corner of a floppy
            translate([x0 + 2, y0 + 3, zb - 1.6]) cube([4, 5, 2]);
            translate([0, 0, zb - 20]) linear_extrude(40) polygon([[x1 + 1, y0 - 1], [x1 - 5, y0 - 1], [x1 + 1, y0 + 5]]);
            // label area grooves along the sides of the shutter
            for (x = [x0 + 8, x1 - 12]) translate([x, y0 + 2, zb - 0.6]) cube([4, 11, 1]);
        }
    }
}

// coloured name plate on the Clawd stand front (print the stand face down,
// so the letters are inlaid in the first `label_depth` mm: few colour swaps)
label       = "";             // "tinyclawd" = Clawd + name lockup, "CLAWD" = old name plate, "" = plain (engraved spinner)
label_depth = 1.2;            // 6 layers at 0.2 mm
part        = "all";          // "all", "body" (stand without the letters), "label" (letters only)

// "tinyclawd": pixel Clawd on top, "tiny clawd" in Chakra Petch Bold underneath, stacked and centred
// (picked by Deepak from out/font_options). Text about 51 x 9 mm, thinnest strokes ~0.85 mm.
module tinyclawd_label_2d() {
    emblem_w = 16;                       // 16 x 10.7 mm, 0.89 mm pixels
    translate([0, 5.9]) clawd_emblem(emblem_w);
    translate([0.56, -6.6]) text("tiny clawd", size = 6.9, font = "Chakra Petch:style=Bold", spacing = 1.12,
                              halign = "center", valign = "center");
}

module clawd_label_2d() {
    if (label == "tinyclawd") tinyclawd_label_2d();
    else clawd_label_2d_old();
}

module clawd_label_2d_old() {
    // ✻ (8.4 mm wide), a 3 mm gap, then the name (37 mm at size 6.5), centred as a group
    translate([-20, 0]) text("✻", size = 9, font = "DejaVu Sans", halign = "center", valign = "center");
    translate([-13.2, 0]) text(label, size = 6.5, font = "DejaVu Sans:style=Bold", halign = "left", valign = "center",
                               spacing = 1.05);
}

module clawd_stand() {
    // four leg pockets so Clawd sits locked in, Claude's spinner on the front
    m = 4;
    x0 = clawd_legs_x[0] - clear - m; x1 = clawd_legs_x[3] + clawd_leg_w + clear + m;
    y0 = clawd_leg_y[0] - m; y1 = clawd_leg_y[1] + clear + m;
    depth = 3;                       // pocket depth
    zb = CLAWD[2] + depth;           // platform top; legs go 3 mm into it
    yf = y0 * cos(angle) + zb * sin(angle);   // the vertical front face
    module plate_prism()             // the letters, from just in front of the face to label_depth behind it
        translate([(x0 + x1) / 2, yf + label_depth, label == "tinyclawd" ? 14.2 : 13]) rotate([90, 0, 0])
            linear_extrude(label_depth + 0.5) clawd_label_2d();
    module body() difference() {
        wedge(x0, x1, y0, y1, zb);
        on_stand(zb, y1) {
            for (lx = clawd_legs_x)
                translate([lx - clear, clawd_leg_y[0] - clear, zb - depth])
                    cube([clawd_leg_w + 2 * clear, clawd_leg_y[1] - clawd_leg_y[0] + 2 * clear, depth + 1]);
        }
        if (label == "")             // plain: ✻ engraved on the front face
            translate([(x0 + x1) / 2, yf + 0.6, 12]) rotate([90, 0, 0])
                linear_extrude(2) text("✻", size = 9, font = "DejaVu Sans", halign = "center", valign = "center");
    }
    if (label == "" || part == "all") body();
    else if (part == "body") difference() { body(); plate_prism(); }
    else if (part == "label") intersection() { body(); plate_prism(); }
}

// --------------------------------------------------------------- preview

module housing_preview(file, zb, y1) {
    %on_stand(zb, y1) import(file, convexity = 10);
}

if (stand == "mac_ramp") {
    mac_ramp();
    if (preview) housing_preview("out/mac_housing.stl", MAC[2], MAC[1][1] + clear + 3);
} else if (stand == "mac_floppy") {
    mac_floppy();
    if (preview) housing_preview("out/mac_housing.stl", MAC[2], MAC[1][1] + clear + 3);
} else if (stand == "clawd") {
    clawd_stand();
    if (preview) housing_preview("out/clawd_v1_housing.stl", CLAWD[2] + 3, clawd_leg_y[1] + clear + 4);
}
