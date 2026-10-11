// Mochi outer housing shaped like Clawd, the Claude Code mascot.
// Pixel grid from the Claude Code banner, in units u:
//   body 6u wide x 4u tall, arms 1u x 1u sticking out at 2u..3u from the top,
//   four legs 0.5u wide x 1u tall under the body.
// Built as soft blocks (rounded corners, 45 deg chamfer on the face edges so it
// prints face down without supports) instead of hard pixels.
// The original housing is kept exactly as it is inside (cavity, OLED window,
// touch pocket, back plate grooves, click dimples). The back plate channel runs
// down through the chin; use clawd_back_plate.scad (original plate + chin).
// The OLED is the face: the eyes are drawn on the screen.
//
// Coordinates: x = width (0..46 is the original), y = depth (0 = original
// front face, 48 = back face), z = height (0 = original bottom, 41 = top).

$fn = 48;

use <mochi_fit.scad>   // crush ribs so the electronics sit snug

orig = "original/Housing-Body.stl";

// --- original features (from Housing.FCStd) ---
W = 46; D = 48; H = 41;
win   = [7, 39, 14.75, 30.75];    // OLED window x0, x1, z0, z1
touch = [15.7, 30.3, 26.7, 41.3]; // touch pocket x0, x1, y0, y1
touch_floor = 40.4;
slot_front = 42.5;                // inner face of the back plate

// --- Clawd ---
u      = 14;         // one pixel unit: body is 6u x 4u
top    = 45;         // body top (original top is 41)
chin   = 4 * u - top; // body goes this far below the original bottom (= 11)
front  = 2;          // face wall in front of the original
face_chamfer = 1.2;  // 45 deg chamfer on the face edges (printable face down)
r_body = 7;          // body corner radius
r_arm  = 4;
r_leg  = 2.5;
r_back = 3;          // rounding of the back edges
arm_d  = 24;         // arm depth from the face (short and stubby)
leg_back = 41;       // legs end here, clear of the back plate path (starts at 42.3)
sink   = 3;          // arms and legs reach this far into the body

cx = W / 2;
bx0 = cx - 3 * u; bx1 = cx + 3 * u;  // body x
y0 = -front;
zb = -chin;

module original() translate([W, 0, 0]) import(orig, convexity = 10);

module rrect(x0, x1, y0, y1, r) {
    hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r])
        translate([x, y]) circle(r);
}

// thin cross section at depth y, outline in the x-z plane
module slab(y, x0, x1, z0, z1, r) {
    translate([0, y, 0]) rotate([90, 0, 0])
        linear_extrude(0.01) rrect(x0, x1, z0, z1, r);
}

// soft block: rounded corners, chamfered face edges, rounded back edges
module soft_block(x0, x1, z0, z1, ya, yb, r, rb = 0) {
    c = face_chamfer;
    hull() {
        slab(ya, x0 + c, x1 - c, z0 + c, z1 - c, max(r - c, 0.5));
        slab(ya + c, x0, x1, z0, z1, r);
        if (rb > 0) {
            for (a = [0 : 15 : 90])
                slab(yb - rb + rb * sin(a), x0 + rb * (1 - cos(a)), x1 - rb * (1 - cos(a)),
                     z0 + rb * (1 - cos(a)), z1 - rb * (1 - cos(a)), max(r - rb * (1 - cos(a)), 0.5));
        } else {
            slab(yb, x0, x1, z0, z1, r);
        }
    }
}

module clawd() {
    // body
    soft_block(bx0, bx1, zb, top, y0, D, r_body, r_back);
    // arms, 2u..3u down from the top
    for (s = [-1, 1]) {
        ax0 = s < 0 ? bx0 - u : bx1 - sink;
        soft_block(ax0, ax0 + u + sink, top - 3 * u, top - 2 * u, y0, y0 + arm_d, r_arm, 2);
    }
    // legs, 0.5u wide, at 0.5u, 1.5u, 4u and 5u across the body
    for (lx = [0.5, 1.5, 4, 5])
        soft_block(bx0 + lx * u, bx0 + (lx + 0.5) * u, zb - u, zb + sink, y0, leg_back, r_leg, 1.5);
}

// back plate channel profile, same as the original grooves
module channel_2d() {
    translate([2.75, slot_front - 0.2]) square([W - 5.5, D - slot_front + 1.2]);
    for (m = [0, 1]) translate([m * W, 0]) mirror([m, 0])
        polygon([[1.25, slot_front - 0.2], [2.75, slot_front - 0.2],
                 [2.75, 46], [1.25, 44.5]]);
}

module cuts() {
    // face window: continues the original 1 mm window chamfer at 45 deg
    hull() {
        translate([win[0] - 3, y0 - 0.01, win[2] - 3])
            cube([win[1] - win[0] + 6, 0.01, win[3] - win[2] + 6]);
        translate([win[0], 1, win[2]]) cube([win[1] - win[0], 0.01, win[3] - win[2]]);
    }
    // touch pocket carried up through the head, keeping its 0.5 mm rim chamfer
    translate([0, 0, touch_floor]) linear_extrude(top - touch_floor + 1)
        rrect(touch[0], touch[1], touch[2], touch[3], 1);
    hull() {
        translate([0, 0, H - 0.5]) linear_extrude(0.01) rrect(touch[0], touch[1], touch[2], touch[3], 1);
        translate([0, 0, H]) linear_extrude(0.01)
            rrect(touch[0] - 0.5, touch[1] + 0.5, touch[2] - 0.5, touch[3] + 0.5, 1.5);
    }
    hull() {
        translate([0, 0, top - 1]) linear_extrude(0.01)
            rrect(touch[0] - 0.5, touch[1] + 0.5, touch[2] - 0.5, touch[3] + 0.5, 1.5);
        translate([0, 0, top + 0.01]) linear_extrude(0.01)
            rrect(touch[0] - 1.5, touch[1] + 1.5, touch[2] - 1.5, touch[3] + 1.5, 2.5);
    }
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

module clawd_housing() {
    union() {
        original();
        fit_ribs();
        difference() {
            clawd();
            difference() { shrunk_hull(); original(); }
            open_original();
            cuts();
        }
    }
}

clawd_housing();
