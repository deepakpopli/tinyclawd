// Clawd as a flat emblem, from Claude Code's block art (clawd_sprite.py rows()).
// Each quadrant is 1 x 2 units (terminal cells are twice as tall as wide); 18 x 12 units total.
// Generated, do not edit. clawd_emblem(w) draws it w mm wide, centred, y up.
clawd_quads = [[3, 0], [3, 1], [4, 0], [5, 0], [4, 1], [6, 0], [7, 0], [6, 1], [7, 1], [8, 0], [9, 0], [8, 1], [9, 1], [10, 0], [11, 0], [10, 1], [11, 1], [12, 0], [13, 0], [12, 1], [14, 0], [15, 0], [14, 1], [15, 1], [1, 2], [2, 2], [3, 2], [3, 3], [4, 2], [5, 2], [4, 3], [5, 3], [6, 2], [7, 2], [6, 3], [7, 3], [8, 2], [9, 2], [8, 3], [9, 3], [10, 2], [11, 2], [10, 3], [11, 3], [12, 2], [13, 2], [12, 3], [13, 3], [14, 2], [15, 2], [14, 3], [15, 3], [16, 2], [17, 2], [3, 4], [5, 4], [13, 4], [15, 4]];

module clawd_emblem(w = 18) {
    u = w / 18;
    translate([-9 * u, 6 * u])
        for (q = clawd_quads) translate([q[0] * u, -(q[1] + 1) * 2 * u]) square([u + 0.001, 2 * u + 0.001]);
}
