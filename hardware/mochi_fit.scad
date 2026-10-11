// Crush ribs inside the original Mochi cavity, so the electronics sit snug.
// (Real fit test of the Mac housing: the inner frame moved a little.)
// Thin triangular ribs run front-to-back on the side walls and the floor,
// with a ramp at the back so the frame slides in from the back easily.
// Cavity of the original: x 2.75..43.25, z 1.75..39.25, y 2..42.5.
// Use: use <mochi_fit.scad>  then  fit_ribs();  or  fit_ribs(side = 0.25, floor = 0.45);

rib_side  = 0.15;           // how far each side rib sticks out (frame is 40.0, cavity 40.5)
rib_floor = 0.3;            // floor ribs, lift the frame off the floor
rib_base  = 1.2;            // rib width at the wall
rib_y     = [5, 39];        // ribs run from here to here (front to back)
ramp      = 3;              // lead-in at the back end
side_z    = [6, 14, 34];    // side rib heights (clear of the side pocket at z 21.5..28.5)
floor_x   = [8, 23, 38];

module rib(h) {
    // triangle profile in x-z, extruded along y, tapering to nothing over `ramp`
    hull() {
        translate([0, rib_y[0], 0]) rotate([-90, 0, 0])
            linear_extrude(rib_y[1] - ramp - rib_y[0])
                polygon([[-rib_base / 2, 0], [rib_base / 2, 0], [0, -h]]);
        translate([0, rib_y[1], 0]) rotate([-90, 0, 0])
            linear_extrude(0.01) polygon([[-rib_base / 2, 0], [rib_base / 2, 0], [0, -0.01]]);
    }
}

module fit_ribs(side = rib_side, floor = rib_floor) {
    // side walls (rib points into the cavity)
    for (z = side_z) {
        translate([2.75, 0, z]) rotate([0, 90, 0]) rib(side);
        translate([43.25, 0, z]) rotate([0, -90, 0]) rib(side);
    }
    // floor
    for (x = floor_x) translate([x, 0, 1.75]) rib(floor);
}
