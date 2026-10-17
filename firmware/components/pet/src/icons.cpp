// the menu logos, all in one place so they look like one family.
// small pixel art in text and plain rects, integers only (the C3 has no fpu).
// every logo fits a 52 x 26 box: x cx-26..cx+25, y cy-13..cy+12, ground at cy+11.
#include "icons.h"

void art(Canvas& c, int x, int y, const char* const* rows, int n) {
    for (int r = 0; r < n; r++)
        for (int i = 0; rows[r][i]; i++) {
            if (rows[r][i] == '#') c.px(x + i, y + r);
            else if (rows[r][i] == '.') c.px(x + i, y + r, false);
        }
}

#define ART(c, x, y, a) art(c, x, y, a, sizeof(a) / sizeof(a[0]))

// ------------------------------------------------------------------ parts

enum Arms { DOWN, UP, WINGS_UP, WINGS_DOWN };

// a blink for 140 ms every ~3 s, a little different per logo
static bool blinking(uint32_t now, uint32_t salt) { return (now + salt) % 3100 < 140; }

// wings instead of arms, the left one. the right one is the mirror
static const char* const WING_UP[] = {
    "#     ",
    "##    ",
    "###   ",
    " ###  ",
    "  ####",
};
static const char* const WING_DOWN[] = {
    "  ####",
    " ###  ",
    "###   ",
    "##    ",
    "#     ",
};

static void wing(Canvas& c, int x, int y, const char* const* w, bool mirror) {
    for (int r = 0; r < 5; r++)
        for (int i = 0; i < 6; i++)
            if (w[r][i] == '#') c.px(mirror ? x + 5 - i : x + i, y + r);
}

// mini Clawd, 20 x 11, top-left at x, y. the same block body as the big one:
// eyes are two tall slits, arms stick out at the sides, four thin legs
static void mini(Canvas& c, int x, int y, int arms, int look_x, int look_y, bool blink) {
    c.fill_rect(x + 2, y, x + 17, y + 8);
    switch (arms) {
    case UP:   // two steps up and out
        c.fill_rect(x, y + 2, x + 1, y + 3);
        c.fill_rect(x - 1, y, x, y + 1);
        c.fill_rect(x + 18, y + 2, x + 19, y + 3);
        c.fill_rect(x + 19, y, x + 20, y + 1);
        break;
    case WINGS_UP:
        wing(c, x - 4, y - 1, WING_UP, false);
        wing(c, x + 18, y - 1, WING_UP, true);
        break;
    case WINGS_DOWN:
        wing(c, x - 4, y + 4, WING_DOWN, false);
        wing(c, x + 18, y + 4, WING_DOWN, true);
        break;
    default:
        c.fill_rect(x, y + 5, x + 1, y + 6);
        c.fill_rect(x + 18, y + 5, x + 19, y + 6);
    }
    static const int LEGS[] = {3, 5, 14, 16};
    for (int lx : LEGS) c.vline(x + lx, y + 9, y + 10);
    int ex = x + 5 + look_x, ey = y + 2 + look_y;
    if (blink) {
        c.hline(ex, ex + 1, ey + 2, false);
        c.hline(ex + 8, ex + 9, ey + 2, false);
    } else {
        c.fill_rect(ex, ey, ex + 1, ey + 2, false);
        c.fill_rect(ex + 8, ey, ex + 9, ey + 2, false);
    }
}

// a little beetle, 9 x 8
static const char* const BUG[] = {
    "#       #",
    " #     # ",
    "  #####  ",
    " ##.#.## ",
    "#########",
    "####.####",
    " ###.### ",
    " # # # # ",
};

// a football, 9 x 9: a patch in the middle, bits of the others at the edge
static const char* const BALL[] = {
    "  #####  ",
    " #..#..# ",
    "#...#...#",
    "#..###..#",
    "##.###.##",
    "#.#...#.#",
    "##.....##",
    " #.....# ",
    "  #####  ",
};

// a token: a coin with a T on it, 9 x 9
static const char* const TOKEN[] = {
    "  #####  ",
    " ####### ",
    "##.....##",
    "####.####",
    "####.####",
    "####.####",
    "####.####",
    " ####### ",
    "  #####  ",
};

static const char* const DROP[] = {
    " # ",
    " # ",
    "###",
    "###",
    " # ",
};

static const char* const TICK[] = {
    "      #",
    "     ##",
    "#   ## ",
    "## ##  ",
    " ###   ",
    "  #    ",
};

static const char* const CROSS[] = {
    "#   #",
    " # # ",
    "  #  ",
    " # # ",
    "#   #",
};

// the same marks cut out of a filled button
static const char* const TICK_CUT[] = {
    "      .",
    "     ..",
    ".   .. ",
    ".. ..  ",
    " ...   ",
    "  .    ",
};

static const char* const CROSS_CUT[] = {
    ".   .",
    " . . ",
    "  .  ",
    " . . ",
    ".   .",
};

// ------------------------------------------------------------------ games

// bug run: Clawd in the air right over a bug, the dots show the jump
void icon_run(Canvas& c, int cx, int cy, uint32_t now) {
    int g = cy + 11;
    int hop = (now / 300) % 2;
    c.hline(cx - 26, cx + 25, g);
    for (int x = cx - 22; x < cx + 24; x += 9) c.px(x, g + 1);
    ART(c, cx + 2, g - 8, BUG);
    mini(c, cx - 4, cy - 12 - hop, DOWN, 1, 0, blinking(now, 0));
    c.hline(cx - 9, cx - 8, cy - 3);
    c.hline(cx - 13, cx - 12, cy + 1);
    c.hline(cx - 17, cx - 16, cy + 5);
    c.hline(cx - 21, cx - 20, cy + 9);
}

// permission panic: an allow / deny box and a sweating Clawd
void icon_panic(Canvas& c, int cx, int cy, uint32_t now) {
    // the box: title bar, two lines of text, the two buttons
    int l = cx - 1, r = cx + 25, t = cy - 12, b = cy + 10;
    c.rect(l, t, r, b);
    c.fill_rect(l, t, r, t + 3);
    c.hline(l + 3, l + 17, t + 6);
    c.hline(l + 3, l + 11, t + 8);
    bool yes = (now / 450) % 2;   // he can not decide
    int y0 = b - 10;
    if (yes) {
        c.fill_rect(l + 2, y0, l + 12, b - 2);
        ART(c, l + 4, y0 + 1, TICK_CUT);
        c.rect(l + 14, y0, r - 2, b - 2);
        ART(c, l + 17, y0 + 2, CROSS);
    } else {
        c.rect(l + 2, y0, l + 12, b - 2);
        ART(c, l + 4, y0 + 1, TICK);
        c.fill_rect(l + 14, y0, r - 2, b - 2);
        ART(c, l + 17, y0 + 2, CROSS_CUT);
    }
    // Clawd, arms up, eyes on the box, sweating
    mini(c, cx - 24, cy, UP, 1, 0, false);
    if ((now / 300) % 2) {   // a little ! over his head
        c.fill_rect(cx - 15, cy - 11, cx - 14, cy - 6);
        c.fill_rect(cx - 15, cy - 4, cx - 14, cy - 3);
    }
    int d = (now / 200) % 2;
    ART(c, cx - 10, cy - 7 + d, DROP);
}

// flappy clawd: Clawd flapping his wings, heading for the gap
void icon_flappy(Canvas& c, int cx, int cy, uint32_t now) {
    bool flap = (now / 220) % 2;
    // pipes: top one hangs down, bottom one comes up, both with a lip
    int px = cx + 14;
    c.rect(px, cy - 13, px + 7, cy - 7);
    c.vline(px + 2, cy - 13, cy - 8);
    c.fill_rect(px - 2, cy - 6, px + 9, cy - 4);
    c.fill_rect(px - 2, cy + 4, px + 9, cy + 6);
    c.rect(px, cy + 7, px + 7, cy + 12);
    c.vline(px + 2, cy + 8, cy + 12);
    int x = cx - 18, y = cy - 5 + (flap ? -1 : 1);
    mini(c, x, y, flap ? WINGS_UP : WINGS_DOWN, 1, 0, blinking(now, 900));
    // speed lines
    c.hline(cx - 26, cx - 24, y + 3);
    c.hline(cx - 26, cx - 25, y + 7);
}

// keepy uppy: the ball bouncing on his head
void icon_keepy(Canvas& c, int cx, int cy, uint32_t now) {
    static const int H[] = {0, 2, 4, 5, 5, 4, 2, 0};
    int k = (now / 90) % 8;
    ART(c, cx - 4, cy - 9 - H[k], BALL);
    mini(c, cx - 10, cy + 1, UP, 0, -1, blinking(now, 400));
    if (H[k] == 0) {   // bump marks
        c.hline(cx - 9, cx - 7, cy - 4);
        c.hline(cx + 7, cx + 9, cy - 4);
    }
    c.hline(cx - 26, cx + 25, cy + 12);
}

// penalties: the ball in the top corner, the keeper went the other way
void icon_penalty(Canvas& c, int cx, int cy, uint32_t now) {
    int l = cx - 22, r = cx + 21, t = cy - 12, g = cy + 11;
    // net: a dotted grid
    for (int y = t + 3; y < g; y += 3)
        for (int x = l + 3; x < r - 1; x += 3) c.px(x, y);
    c.fill_rect(l, t, l + 1, g);
    c.fill_rect(r - 1, t, r, g);
    c.fill_rect(l, t, r, t + 1);
    c.hline(cx - 26, cx + 25, g);
    // the keeper, clear the net around him first
    int kx = cx - 16, ky = g - 11;
    c.fill_rect(kx - 2, ky - 1, kx + 21, g - 1, false);
    mini(c, kx, ky, UP, -1, 0, blinking(now, 1500));
    // the ball, it shakes the net now and then
    int bump = (now / 160) % 6 == 0 ? 1 : 0;
    int bx = r - 12 + bump, by = t + 2 - bump;
    c.fill_rect(bx - 1, by, bx + 9, by + 9, false);
    ART(c, bx, by, BALL);
}

// token catch: tokens falling into his arms
void icon_catch(Canvas& c, int cx, int cy, uint32_t now) {
    int a = (now / 60) % 16;
    if (a < 12) ART(c, cx - 4, cy - 13 + a, TOKEN);
    if (a >= 10 && a < 14) {   // got it: sparkles
        c.px(cx - 12, cy - 4);
        c.px(cx + 12, cy - 4);
        c.hline(cx - 14, cx - 13, cy - 7);
        c.hline(cx + 13, cx + 14, cy - 7);
    }
    mini(c, cx - 10, cy + 1, UP, 0, -1, blinking(now, 2200));
    c.hline(cx - 26, cx + 25, cy + 12);
}

// whack a bug: a bug pops out of its hole, the mallet comes down
void icon_whack(Canvas& c, int cx, int cy, uint32_t now) {
    int g = cy + 11, hx = cx - 4;   // the hole's middle
    uint32_t p = now % 1600;
    bool hit = p >= 800 && p < 1100;
    // how far the bug is out: rises, waits, gets bonked down, comes back
    int up = p < 300 ? 2 + (int)p / 60 : (p < 800 ? 7 : (p < 1100 ? 4 : (p < 1400 ? 2 : 2 + ((int)p - 1400) / 40)));
    if (up > 7) up = 7;
    ART(c, hx - 4, g - 3 - up, BUG);
    // the hole, its front edge hides the bottom of the bug
    c.fill_rect(hx - 9, g, hx + 9, cy + 13, false);
    c.hline(cx - 26, cx + 25, g);
    c.fill_rect(hx - 8, g, hx + 8, g + 1, false);
    c.hline(hx - 9, hx + 9, g + 2);
    c.px(hx - 9, g + 1);
    c.px(hx + 9, g + 1);
    // the mallet: a fat head with two bands, the handle goes up to the right
    int mx = hit ? hx - 6 : hx - 1, my = hit ? cy - 3 : cy - 8;
    c.fill_rect(mx, my, mx + 11, my + 5);
    c.vline(mx + 2, my + 1, my + 4, false);
    c.vline(mx + 9, my + 1, my + 4, false);
    c.px(mx, my, false);
    c.px(mx + 11, my, false);
    c.px(mx, my + 5, false);
    c.px(mx + 11, my + 5, false);
    for (int i = 0; i < 5; i++) c.fill_rect(mx + 7 + i, my - 1 - i, mx + 8 + i, my - 1 - i);
    if (hit) {   // bonk
        c.hline(hx - 15, hx - 13, my + 3);
        c.hline(hx + 13, hx + 15, my + 3);
        c.px(hx - 12, my);
        c.px(hx + 12, my);
    }
}

// ------------------------------------------------------------------ top menu

// a game pad
void icon_games(Canvas& c, int cx, int cy, uint32_t now) {
    (void)now;
    c.fill_rect(cx - 15, cy - 7, cx + 15, cy + 5);
    c.fill_rect(cx - 17, cy - 5, cx + 17, cy + 5);
    c.fill_rect(cx - 17, cy + 6, cx - 8, cy + 8);   // grips
    c.fill_rect(cx + 8, cy + 6, cx + 17, cy + 8);
    c.fill_rect(cx - 13, cy - 2, cx - 5, cy, false);   // d-pad
    c.fill_rect(cx - 10, cy - 5, cx - 8, cy + 3, false);
    c.fill_rect(cx + 6, cy - 1, cx + 8, cy + 1, false);   // buttons
    c.fill_rect(cx + 11, cy - 4, cx + 13, cy - 2, false);
}

// bars of the limits, Clawd looks at them
void icon_usage(Canvas& c, int cx, int cy, uint32_t now) {
    int g = cy + 11;
    c.hline(cx - 26, cx + 25, g);
    c.fill_rect(cx + 2, g - 8, cx + 7, g - 1);
    c.fill_rect(cx + 10, g - 15, cx + 15, g - 1);
    c.fill_rect(cx + 18, g - 22, cx + 23, g - 1);
    mini(c, cx - 22, g - 11, DOWN, 1, -1, blinking(now, 700));
}

void icon_back(Canvas& c, int cx, int cy, uint32_t now) {
    (void)now;
    for (int i = 0; i < 9; i++) c.vline(cx - 14 + i, cy - i, cy + i);
    c.fill_rect(cx - 5, cy - 3, cx + 14, cy + 3);
}

void icon_play(Canvas& c, int cx, int cy, uint32_t now) {
    (void)now;
    for (int i = 0; i < 9; i++) c.vline(cx - 4 + i, cy - 9 + i, cy + 9 - i);
}
