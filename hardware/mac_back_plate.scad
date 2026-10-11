// Back plate for the Mac style housing (also used by the Clawd housing through
// back_plate(chin, port_strip), see clawd_back_plate.scad).
// The original back plate is kept as it is (grooves, click arms, USB-C
// cutout, ring logo). It is extended downward by the chin height so it
// covers the back of the chin and slides in from the bottom.
//
// Plate coordinates (same as the original STL, print as is, outer face up):
// x = width, y = 0 bottom edge .. 40 top edge, z = 0 inside .. 5.5 outside.

$fn = 64;

orig = "original/Back Plate-Body.stl";

chin    = 11.5; // must match mac_housing.scad
arm_gap = 1;    // gap under the click arms so they still flex
// recessed port strip along the bottom, like the back of the Mac
strip   = [-38, -5.5, -9.5, -3]; // x0, x1, y0, y1
strip_depth = 0.8;
ports   = [[-34, 4], [-27.5, 6], [-20.5, 4], [-14.5, 4]]; // centre x, width
port_h  = 2;

// cross section of the plate, from the original
profile = [[-43.25, 0], [-0.25, 0], [-0.25, 1.9], [-1.75, 3.4], [-1.75, 5.5],
           [-41.75, 5.5], [-41.75, 3.4], [-43.25, 1.9]];

module extension(chin, port_strip) {
    difference() {
        rotate([90, 0, 0]) linear_extrude(chin) polygon(profile);
        // keep the click arms free
        translate([-44, -arm_gap, -1]) cube([44 - 39, arm_gap + 0.01, 8]);
        translate([-4.5, -arm_gap, -1]) cube([5.5, arm_gap + 0.01, 8]);
        // port strip with small port shapes and an audio jack
        if (port_strip) translate([strip[0], strip[2], 5.5 - strip_depth]) cube([strip[1] - strip[0], strip[3] - strip[2], 1]);
        if (port_strip) for (p = ports)
            translate([p[0] - p[1] / 2, (strip[2] + strip[3]) / 2 - port_h / 2, 5.5 - strip_depth - 0.6])
                cube([p[1], port_h, 1]);
        if (port_strip) translate([-9, (strip[2] + strip[3]) / 2, 5.5 - strip_depth - 0.6]) cylinder(d = 1.6, h = 1);
        // finger edge to pull the plate out
        translate([-30, -chin, 5.5]) rotate([45, 0, 0]) cube([16.5, 1.7, 1.7], center = false);
    }
}

module back_plate(chin = chin, port_strip = true) {
    union() {
        import(orig, convexity = 10);
        extension(chin, port_strip);
    }
}

back_plate();
