// Clawd with the football built into the body, as three colour parts.
// The face prints on the bed, so the white hexagons are inlaid in the first
// `inlay` mm of the ball; the whole ball disc is black, the rest of the body red.
// Same cylinder as clawd_v1_housing.scad (ball_c, ball_r, ball_depth, $fn 96).
//   openscad -D 'part="red"'   -o out/clawd_ball_body_red.stl   ball_in_body.scad
//   (and "black", "white").  Needs out/clawd_ball_body_full.stl first.
include <football_panels.scad>

part   = "red";
inlay  = 1.2;              // white depth on the face: 6 layers at 0.2 mm
ball_c = [50.5, 4];  ball_r = 11;  ball_depth = 3;
y0     = -1.5;             // Clawd face plane

module full() import("out/clawd_ball_body_full.stl", convexity = 10);

module disc() {            // a little longer than the ball, same outline
    translate([ball_c[0], y0 - 0.5, ball_c[1]]) rotate([-90, 0, 0])
        cylinder(r = ball_r, h = ball_depth + 1, $fn = 96);
}

module hexagons() {
    translate([ball_c[0], y0 + inlay, ball_c[1]]) rotate([90, 0, 0])
        linear_extrude(inlay + 0.5) for (p = ball_panels) polygon(p);
}

if (part == "red") difference() { full(); disc(); }
else if (part == "black") difference() { intersection() { full(); disc(); } hexagons(); }
else if (part == "white") intersection() { full(); hexagons(); }
