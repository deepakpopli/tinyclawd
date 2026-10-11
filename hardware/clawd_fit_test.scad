// Quick fit test for the Clawd housing: only the back 22 mm (crush ribs, back
// plate grooves and click dimples, no legs). Slide the electronics in from the back and
// click the original back plate on. Print with the back face down.
// Render out/clawd_v1_housing.stl first.

cut_y = 26;    // keep everything behind this depth

translate([0, 0, 48]) rotate([-90, 0, 0])        // back face on the bed
    intersection() {
        import("out/clawd_v1_housing.stl", convexity = 10);
        translate([-50, cut_y, 0]) cube([200, 100, 200]);   // z >= 0 drops the legs
    }
