#include "clawd.h"

#include <math.h>
#include <string.h>

#include "motion.h"

// quadrant bits for each block char: top-left, top-right, bottom-left, bottom-right
static uint8_t quads(uint16_t ch) {
    switch (ch) {
    case 0x2580: return 0b1100;  // upper half
    case 0x2584: return 0b0011;  // lower half
    case 0x2588: return 0b1111;  // full
    case 0x258c: return 0b1010;  // left half
    case 0x2590: return 0b0101;  // right half
    case 0x2596: return 0b0010;
    case 0x2597: return 0b0001;
    case 0x2598: return 0b1000;
    case 0x2599: return 0b1011;
    case 0x259a: return 0b1001;
    case 0x259b: return 0b1110;
    case 0x259c: return 0b1101;
    case 0x259d: return 0b0100;
    case 0x259e: return 0b0110;
    case 0x259f: return 0b0111;
    default: return 0;
    }
}

void block_rows(Canvas& c, int x, int y, const char* const* rows, int n, float scale, bool on) {
    int cw = iround(4 * scale), chh = iround(8 * scale);
    int qw = cw / 2 > 0 ? cw / 2 : 1, qh = chh / 2 > 0 ? chh / 2 : 1;
    for (int r = 0; r < n; r++) {
        const char* s = rows[r];
        int col = 0;
        while (*s) {
            uint16_t ch = utf8_next(s);
            int px = x + col * cw, py = y + r * chh;
            if (ch == 0x2582) {  // closed eye: solid cell with a thin lid line
                c.fill_rect(px, py, px + cw - 1, py + chh - 1, on);
                c.fill_rect(px + qw, py + chh - 2, px + cw - 1, py + chh - 2, !on);
            } else {
                uint8_t q = quads(ch);
                for (int k = 0; k < 4; k++)
                    if (q & (8 >> k)) {
                        int qx = px + (k % 2) * qw, qy = py + (k / 2) * qh;
                        c.fill_rect(qx, qy, qx + qw - 1, qy + qh - 1, on);
                    }
            }
            col++;
        }
    }
}

// poses, as in Claude Code
static const char* const R1L[] = {" ▐", "▗▟", " ▐"};
static const char* const R1R[] = {"", "▄", "▄"};
static const char* const R2L[] = {"▝▜", " ▜", "▝▜"};
static const char* const R2R[] = {"█▀", "█▘", "█▘"};
static const char* const EYE_ROW[] = {"▛███▛█", "▟███▟█", "█▟███▟", "▂███▂█", "▛███▂█"};
static const char* const FEET_ROW[] = {" ▝▝   ▝▝ ", " ▝▝      ", "      ▝▝ ", "         "};

void clawd(Canvas& c, int x, int y, const Pose& p, float scale) {
    char r1[64], r2[64];
    // row 1: arm + eyes, padded to 8 cells, then the right arm
    strcpy(r1, R1L[p.arms]);
    strcat(r1, EYE_ROW[p.eyes]);
    strcat(r1, R1R[p.arms]);
    strcpy(r2, R2L[p.arms]);
    strcat(r2, "█████");
    strcat(r2, R2R[p.arms]);
    const char* rows[] = {r1, r2, FEET_ROW[p.feet]};
    // R1L is 2 cells, eyes 6 -> 8 cells, right arm lands on cell 9 (or blank)
    block_rows(c, x, y, rows, 3, scale);
}

static const char* const TURN[TURN_STEPS][3] = {
    {" ▐█▜██▛█ ", "▝▜██████▀", " ▝▝   ▝▝ "},
    {"  █▛██▛▌ ", " ▝█████▛ ", "  ▘▘  ▘▘ "},
    {"  ▐█▛█▜  ", "  ▐████  ", "  ▝▝ ▝▝  "},
    {"   ██▛▌  ", "   ███▌  ", "   ▘  ▘  "},
    {"   ▐██   ", "   ▐██   ", "   ▝ ▝   "},
    {"   ███▌  ", "   ███▌  ", "   ▘  ▘  "},
    {"  ▐████  ", "  ▐████  ", "  ▝▝ ▝▝  "},
    {"  █████▌ ", " ▝█████▛ ", "  ▘▘  ▘▘ "},
    {" ▐██████ ", "▝▜██████▀", " ▝▝   ▝▝ "},
    {"   ▛██▌  ", "   ███▌  ", "   ▘  ▘  "},
    {"  ▐▜▛██  ", "  ▐████  ", "  ▝▝ ▝▝  "},
    {"  ▛██▛█▌ ", " ▝█████▛ ", "  ▘▘  ▘▘ "},
    {" ▐▛███▜█ ", "▝▜██████▀", " ▝▝   ▝▝ "},
};

void clawd_turn(Canvas& c, int x, int y, int k, float scale) {
    if (k < 0) k = 0;
    if (k >= TURN_STEPS) k = TURN_STEPS - 1;
    block_rows(c, x, y, TURN[k], 3, scale);
}

void kick_leg(Canvas& c, int x, int y, bool right) {
    if (right) c.line(x + 29, y + 16, x + 36, y + 13, 2);
    else c.line(x + 6, y + 16, x - 1, y + 13, 2);
}

void clawd_rotated(Canvas& c, float cx, float cy, float deg, const Pose& p) {
    // draw upright into a scratch canvas, then sample it rotated
    static Canvas tmp;
    tmp.clear();
    const int ox = 4, oy = 4;
    clawd(tmp, ox, oy, p);
    float a = deg * (float)M_PI / 180, ca = cosf(a), sa = sinf(a);
    float sx = ox + SPRITE_W / 2.0f, sy = oy + SPRITE_H / 2.0f;
    const int R = 24;   // half diagonal of the sprite
    for (int dy = -R; dy <= R; dy++)
        for (int dx = -R; dx <= R; dx++) {
            // inverse rotate dest -> source (screen y points down)
            float u = ca * dx - sa * dy, v = sa * dx + ca * dy;
            if (tmp.get(iround(sx + u), iround(sy + v))) c.px(iround(cx) + dx, iround(cy) + dy);
        }
}
