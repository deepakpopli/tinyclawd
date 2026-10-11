// Mochi outer housing, styled after the 1984 Macintosh 128K.
// The original housing is kept exactly as it is inside (cavity, OLED window,
// ear holes, touch pocket, back plate grooves, click dimples). A Mac-shaped
// shell is built around it. The back plate channel is carried down through
// the chin so the taller back plate (mac_back_plate.scad) slides in from the
// bottom like the original.
//
// Coordinates: x = width (0..46 is the original), y = depth (0 = original
// front face, 48 = back face), z = height (0 = original bottom, 41 = top).

$fn = 64;

use <mochi_fit.scad>   // crush ribs so the electronics sit snug

orig = "original/Housing-Body.stl";

// --- original features (from Housing.FCStd) ---
W = 46; D = 48; H = 41;
win   = [7, 39, 14.75, 30.75];    // OLED window x0, x1, z0, z1
ear_y = 18; ear_z = 25; ear_d = 7;
ear_holes = false;                // true = keep the side holes open to the outside
touch = [15.7, 30.3, 26.7, 41.3]; // touch pocket x0, x1, y0, y1
touch_floor = 40.4;
slot_front = 42.5;                // inner face of the back plate

// --- body ---
front     = 6;     // front wall in front of the original (screen recess depth)
side_f    = 3;     // side wall at the front
side_b    = 2;     // side wall at the back (body tapers toward the back)
top_f     = 46.5;  // top height at the front
top_b     = 45.5;  // top height at the back
chin      = 11.5;  // chin height below the original housing
bevel     = 2.5;   // 45 deg bevel around the top and sides of the front face
r_back    = 2;     // back vertical edges
r_topback = 2.5;   // top back edge

// --- screen recess (Mac CRT surround) ---
recess    = [3, 43, 8.25, 37.75]; // opening on the front face x0, x1, z0, z1
recess_r  = 1.5;
floor_y   = -1;                   // recess floor, 1 mm in front of the original
floor_in  = [2, 5, 4.5];             // floor inset: sides, top, bottom (bottom = sill)

// --- chin details ---
floppy    = [22, 43, -1.2];       // slot x0, x1, centre z
floppy_h  = 1.6;
floppy_end = [6, 3.4];            // taller part at the right end: width, height
floppy_depth = 2;
badge     = [6.5, -5, 5];         // badge centre x, z, size

// --- top handle (the touch pocket sits inside it) ---
handle    = [11, 35, 25];         // x0, x1, y0 (runs out the back)
handle_floor = 43;

// --- side seam between the front bezel and the back case ---
seam_y = 3; seam_w = 0.6; seam_depth = 0.4;

foot_h = 1.2; foot_in = 0.6;

y0 = -front;
zb = -chin;

module original() translate([W, 0, 0]) import(orig, convexity = 10);

function lerp(a, b, t) = a + (b - a) * t;
function t_at(y) = (y - (y0 + bevel)) / (D - (y0 + bevel));
function xl(y) = lerp(-side_f, -side_b, t_at(y));
function xr(y) = W - xl(y);
function zt(y) = lerp(top_f, top_b, t_at(y));

module rrect(x0, x1, y0, y1, r) {
    hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r])
        translate([x, y]) circle(r);
}

// thin cross section of the body at depth y
module slab(y, x0, x1, z0, z1, r = 0.5) {
    translate([0, y, 0]) rotate([90, 0, 0])
        linear_extrude(0.01) rrect(x0, x1, z0, z1, r);
}

module envelope() {
    yf = y0 + bevel;
    hull() {
        // front face, bevelled on top and sides
        slab(y0, xl(yf) + bevel, xr(yf) - bevel, zb, zt(yf) - bevel);
        slab(yf, xl(yf), xr(yf), zb, zt(yf));
        // back, with softened edges
        yb = D - r_topback;
        slab(yb, xl(yb), xr(yb), zb, zt(yb), r_back);
        slab(D, xl(D) + 0.6, xr(D) - 0.6, zb, zt(D) - r_topback, r_back);
    }
}

// back plate channel profile, same as the original grooves
module channel_2d() {
    translate([2.75, slot_front - 0.2]) square([W - 5.5, D - slot_front + 1.2]);
    for (m = [0, 1]) translate([m * W, 0]) mirror([m, 0])
        polygon([[1.25, slot_front - 0.2], [2.75, slot_front - 0.2],
                 [2.75, 46], [1.25, 44.5]]);
}

module front_cuts() {
    // screen recess: steep top wall, sloped sill at the bottom
    hull() {
        slab(y0 - 0.5, recess[0], recess[1], recess[2], recess[3], recess_r);
        slab(floor_y, recess[0] + floor_in[0], recess[1] - floor_in[0],
             recess[2] + floor_in[2], recess[3] - floor_in[1], recess_r);
    }
    // window: continue the original 1 mm window chamfer out to the recess floor
    hull() {
        slab(floor_y + 0.01, win[0] - 2, win[1] + 2, win[2] - 2, win[3] + 2, 0.01);
        slab(1, win[0], win[1], win[2], win[3], 0.01);
    }
    // floppy slot, taller at the right end
    fz = floppy[2];
    translate([floppy[0], y0 - 1, fz - floppy_h / 2])
        cube([floppy[1] - floppy[0], floppy_depth + 1, floppy_h]);
    translate([floppy[1] - floppy_end[0], y0 - 1, fz - floppy_end[1] / 2])
        cube([floppy_end[0], 0.8 + 1, floppy_end[1]]);
    // badge: small square with the ring mark from the back plate
    translate([badge[0], y0 - 1, badge[1]]) rotate([-90, 0, 0])
        linear_extrude(1 + 0.4) square(badge[2], center = true);
    translate([badge[0], y0 - 1, badge[1]]) rotate([-90, 0, 0])
        linear_extrude(1 + 0.9) difference() {
            circle(d = 3.8);
            circle(d = 2.6);
        }
    translate([badge[0], y0 - 1, badge[1]]) rotate([-90, 0, 0])
        linear_extrude(1 + 0.9) circle(d = 1.2);
}

module outer_cuts() {
    // ear holes: closed on the outside by default (the original's holes stay
    // as shallow pockets on the inside, so nothing inside changes)
    if (ear_holes) {
        translate([-10, ear_y, ear_z]) rotate([0, 90, 0]) cylinder(d = ear_d, h = W + 20);
        translate([xl(ear_y) - 0.01, ear_y, ear_z]) rotate([0, 90, 0])
            cylinder(d1 = ear_d + 2, d2 = ear_d, h = 1);
        translate([xr(ear_y) + 0.01, ear_y, ear_z]) rotate([0, -90, 0])
            cylinder(d1 = ear_d + 2, d2 = ear_d, h = 1);
    }
    // carry handle on top, open at the back, chamfered edges
    hull() {
        translate([0, 0, handle_floor]) linear_extrude(0.01)
            rrect(handle[0], handle[1], handle[2], D + 5, 1.5);
        translate([0, 0, top_f + 1]) linear_extrude(0.01)
            rrect(handle[0] - 1.5, handle[1] + 1.5, handle[2] - 1.5, D + 5, 2);
    }
    // touch pocket carried up to the handle floor, keeping its 0.5 mm rim chamfer
    translate([0, 0, touch_floor]) linear_extrude(handle_floor - touch_floor + 0.5)
        rrect(touch[0], touch[1], touch[2], touch[3], 1);
    hull() {
        translate([0, 0, H - 0.5]) linear_extrude(0.01) rrect(touch[0], touch[1], touch[2], touch[3], 1);
        translate([0, 0, H]) linear_extrude(handle_floor - H + 0.5)
            rrect(touch[0] - 0.5, touch[1] + 0.5, touch[2] - 0.5, touch[3] + 0.5, 1.5);
    }
    // seam groove around the sides and top
    difference() {
        translate([-10, seam_y, zb - 1]) cube([W + 20, seam_w, 60]);
        translate([xl(seam_y) + seam_depth, seam_y - 1, zb - 2])
            cube([xr(seam_y) - xl(seam_y) - 2 * seam_depth, seam_w + 2,
                  zt(seam_y) - seam_depth - zb + 2]);
    }
    // small foot step on the front and sides
    translate([0, 0, zb - 1]) linear_extrude(foot_h + 1) difference() {
        translate([-10, y0 - 1]) square([W + 20, 40 - y0 + 1]);
        offset(delta = -foot_in) projection() envelope();
    }
    front_cuts();
}

module shrunk_hull(e = 0.12) {
    translate([W / 2, D / 2, H / 2])
        scale([(W - 2 * e) / W, (D - 2 * e) / D, (H - 2 * e) / H])
            translate([-W / 2, -D / 2, -H / 2]) hull() original();
}

// openings that reach the outer faces, cut generously (the original is
// added back afterwards, so cutting into its walls here changes nothing)
module open_original() {
    translate([win[0], -1, win[2]]) cube([win[1] - win[0], 3.5, win[3] - win[2]]);
    translate([2.75, 40, 1.75]) cube([W - 5.5, 10, 39.25 - 1.75]);
    translate([0, 0, zb - 1]) linear_extrude(39.25 - zb + 1) channel_2d();
}

module mac_housing() {
    union() {
        original();
        fit_ribs();
        difference() {
            envelope();
            difference() { shrunk_hull(); original(); }
            open_original();
            outer_cuts();
        }
    }
}

mac_housing();
