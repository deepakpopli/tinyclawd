// Back plate for the Clawd housing: the original plate extended down by the
// Clawd chin, plain (no Mac port strip). Print as is, outer face up.
use <mac_back_plate.scad>

chin = 11; // must match clawd_housing.scad

back_plate(chin = chin, port_strip = false);
