// Tolerance test rings: thin slices of the Mochi cavity (40.5 x 37.5) with
// crush ribs of different heights, same rib layout as mochi_fit.scad.
// Slide the electronics frame through each one and pick the snug one.
// Ring k has k notches on top. Print as is, rings standing up, no supports.

$fn = 32;

// [side rib, floor rib] per ring
ribs  = [[0, 0], [0.10, 0.2], [0.15, 0.3], [0.20, 0.4], [0.25, 0.5]];
depth = 8;          // ring height on the bed
wall  = 1.6;
base  = 1.2;        // rib width at the wall
lead  = 1.5;        // ribs taper at both ends so the frame enters either way
gap   = 6;          // space between rings

cav = [40.5, 37.5];                 // cavity x, z
side_z  = [6, 14, 34] - [1.75, 1.75, 1.75];   // rib heights above the cavity floor
floor_x = [8, 23, 38] - [2.75, 2.75, 2.75];   // rib positions from the left wall

module rib(h) {
    // triangle profile, height h, tapering over `lead` at both ends
    if (h > 0) hull() {
        translate([0, 0, lead]) linear_extrude(depth - 2 * lead)
            polygon([[-base / 2, 0], [base / 2, 0], [0, h]]);
        linear_extrude(depth) polygon([[-base / 2, 0], [base / 2, 0], [0, 0.01]]);
    }
}

module ring(side, flo, k) {
    difference() {
        translate([-wall, -wall, 0]) cube([cav[0] + 2 * wall, cav[1] + 2 * wall, depth]);
        translate([0, 0, -1]) cube([cav[0], cav[1], depth + 2]);
        // k notches on the top edge
        for (n = [0 : k - 1]) translate([cav[0] / 2 - (k - 1) * 2.5 + n * 5, cav[1] + wall + 0.01, depth / 2])
            rotate([0, 0, 45]) cube([1.6, 1.6, depth + 2], center = true);
    }
    // ribs: x = side walls, y = floor (ring lies with the cavity's z along +y)
    for (z = side_z) {
        translate([0, z, 0]) rotate([0, 0, -90]) rib(side);
        translate([cav[0], z, 0]) rotate([0, 0, 90]) rib(side);
    }
    for (x = floor_x) translate([x, 0, 0]) rib(flo);
}

for (i = [0 : len(ribs) - 1])
    translate([(i % 3) * (cav[0] + 2 * wall + gap), floor(i / 3) * (cav[1] + 2 * wall + gap), 0])
        ring(ribs[i][0], ribs[i][1], i + 1);
