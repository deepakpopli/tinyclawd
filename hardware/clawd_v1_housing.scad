// Clawd v1 (blocky pixel version, uses the ORIGINAL back plate unchanged).
// Mochi outer housing shaped like Clawd, the Claude Code mascot.
// Pixel grid from the Claude Code banner, in units u:
//   body 6u wide x 4u tall, arms 1u x 1u sticking out at 2u..3u from the top,
//   four legs 0.5u wide x 1u tall under the body.
// The original housing is kept exactly as it is inside (cavity, OLED window,
// touch pocket, back plate grooves, click dimples). The body bottom sits at the
// original bottom, so the ORIGINAL back plate slides in unchanged; the legs stop
// short of its path. The OLED is the face: the eyes are drawn on the screen.
//
// Coordinates: x = width (0..46 is the original), y = depth (0 = original
// front face, 48 = back face), z = height (0 = original bottom, 41 = top).

include <BOSL2/std.scad>

$fn = 64;

use <mochi_fit.scad>   // crush ribs so the electronics sit snug

orig = "original/Housing-Body.stl";

// --- original features (from Housing.FCStd) ---
W = 46; D = 48; H = 41;
win   = [7, 39, 14.75, 30.75];    // OLED window x0, x1, z0, z1
touch = [15.7, 30.3, 26.7, 41.3]; // touch pocket x0, x1, y0, y1
touch_floor = 40.4;
slot_front = 42.5;                // inner face of the back plate

// --- Clawd ---
u      = 10.5;       // one pixel unit: body is 6u x 4u
top    = 4 * u;      // body top (original top is 41)
front  = 1.5;        // face wall in front of the original
edge   = 0.8;        // chamfer on all outer edges
arm_d  = D + front; // arms run the full depth, like the extruded pixel sprite
leg_back = 41;      // legs end here, clear of the back plate path (starts at 42.3)

// a little snugger than the Mac (real Mac print: the screen moved a little), not tight
rib_side  = 0.15;   // gentle: just takes up the play (Mac without ribs wobbled a little)
rib_floor = 0.3;

// football badge (football_badge.py): two holes for 1.75 mm filament pins
badge_pins = [[48, 7], [46.6, -3.5]];   // x, z on the face (body, right front leg)
pin_d = 2.0; pin_depth = 3;

// or the football built into the body (colours inlaid by ball_in_body.py)
ball_in_body = false;
ball_c = [50.5, 4];  ball_r = 11;  ball_depth = 3;   // same centre and size as the badge; 3 mm deep is plenty strong

cx = W / 2;
bx0 = cx - 3 * u; bx1 = cx + 3 * u;  // body x
y0 = -front;

module original() translate([W, 0, 0]) import(orig, convexity = 10);

module block(x0, x1, y0, y1, z0, z1) {
    translate([x0, y0, z0]) cuboid([x1 - x0, y1 - y0, z1 - z0], chamfer = edge, anchor = FRONT + LEFT + BOTTOM);
}

module clawd() {
    if (ball_in_body)
        translate([ball_c[0], y0, ball_c[1]]) rotate([-90, 0, 0]) cylinder(r = ball_r, h = ball_depth, $fn = 96);
    // body
    block(bx0, bx1, y0, D, 0, top);
    // arms, 2u..3u down from the top
    for (s = [-1, 1]) {
        ax = s < 0 ? bx0 - u : bx1 - 1;
        block(ax, ax + u + 1, y0, y0 + arm_d, top - 3 * u, top - 2 * u);
    }
    // legs, 0.5u wide, at 0.5u, 1.5u, 4u and 5u across the body
    for (lx = [0.5, 1.5, 4, 5])
        block(bx0 + lx * u, bx0 + (lx + 0.5) * u, y0, leg_back, -u, 1);
}

// back plate channel profile, same as the original grooves
module channel_2d() {
    translate([2.75, slot_front - 0.2]) square([W - 5.5, D - slot_front + 1.2]);
    for (m = [0, 1]) translate([m * W, 0]) mirror([m, 0])
        polygon([[1.25, slot_front - 0.2], [2.75, slot_front - 0.2],
                 [2.75, 46], [1.25, 44.5]]);
}

module rrect(x0, x1, y0, y1, r) {
    hull() for (x = [x0 + r, x1 - r], y = [y0 + r, y1 - r])
        translate([x, y]) circle(r);
}

module cuts() {
    // pin holes for the football badge
    if (!ball_in_body) for (p = badge_pins) translate([p[0], y0 - 1, p[1]]) rotate([-90, 0, 0]) cylinder(d = pin_d, h = pin_depth + 1);
    // face window: continues the original 1 mm window chamfer at 45 deg
    hull() {
        translate([win[0] - 2.5, y0 - 0.01, win[2] - 2.5])
            cube([win[1] - win[0] + 5, 0.01, win[3] - win[2] + 5]);
        translate([win[0], 1, win[2]]) cube([win[1] - win[0], 0.01, win[3] - win[2]]);
    }
    // touch pocket carried up through the head, keeping its 0.5 mm rim chamfer
    translate([0, 0, touch_floor]) linear_extrude(top - touch_floor + 1)
        rrect(touch[0], touch[1], touch[2], touch[3], 1);
    hull() {
        translate([0, 0, H - 0.5]) linear_extrude(0.01) rrect(touch[0], touch[1], touch[2], touch[3], 1);
        translate([0, 0, top - 0.6]) linear_extrude(0.01)
            rrect(touch[0] - 0.5, touch[1] + 0.5, touch[2] - 0.5, touch[3] + 0.5, 1.5);
        translate([0, 0, top + 0.01]) linear_extrude(0.01)
            rrect(touch[0] - 1.1, touch[1] + 1.1, touch[2] - 1.1, touch[3] + 1.1, 2);
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
    translate([0, 0, -u - 1]) linear_extrude(39.25 + u + 1) channel_2d();
}

module clawd_housing() {
    union() {
        original();
        fit_ribs(side = rib_side, floor = rib_floor);
        difference() {
            clawd();
            difference() { shrunk_hull(); original(); }
            open_original();
            cuts();
        }
    }
}

clawd_housing();
