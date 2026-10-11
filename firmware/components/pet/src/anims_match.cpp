// match mode: the session as a football match. score, cards, VAR, subs
#include <stdio.h>

#include "anim.h"

static void whistle(Canvas& c, int x, int y, bool flip) {
    // ring + mouthpiece, x, y = centre of the ring
    c.ellipse(x - 4, y - 4, x + 4, y + 4, 2);
    if (flip) c.fill_rect(x - 16, y - 3, x - 6, y);
    else c.fill_rect(x + 6, y - 3, x + 16, y);
}

// sound arcs coming off the whistle, k = 0..1 how many are lit
static void peep_arcs(Canvas& c, int x, int y, bool flip, uint32_t t) {
    for (int k = 0; k < 3; k++) {
        if ((t / 90 + k) % 3 == 0) continue;
        int r = 5 + k * 4;
        if (flip) c.arc(x - r, y - r, x + r, y + r, 200, 250, 1);   // up and left
        else c.arc(x - r, y - r, x + r, y + r, 290, 340, 1);   // up and right
    }
}

// ------------------------------------------------------------- scoreboard

static void a_scoreboard(Canvas& c, uint32_t t, const Ctx& x) {
    int now = x.done < 0 ? 0 : (x.done > 99 ? 99 : x.done);
    int bugs = x.bugs < 0 ? 0 : (x.bugs > 99 ? 99 : x.bugs);
    bool rolls = now > 0;
    bool word = hash32(x.seed + 77) & 1;   // flash GOAL on the bottom line instead of a box
    const uint32_t ROLL = 900, ROLLED = ROLL + 280, FLASH = ROLLED + 300, DONE = FLASH + 750;
    char a[4], b[4];

    // left score rolls up from the old one to the new one
    float roll = rolls ? 26 * ease(seg(t, ROLL, ROLLED)) : 26;
    int y = 22 - iround(roll);
    snprintf(a, sizeof(a), "%d", rolls ? now - 1 : now);
    snprintf(b, sizeof(b), "%d", now);
    if (rolls) c.text_center(34, y, a, font_bold24);
    c.text_center(34, y + 26, b, font_bold24);
    c.fill_rect(1, 1, 62, 18, false);       // window for the rolling digit
    c.fill_rect(1, 48, 62, 62, false);

    snprintf(a, sizeof(a), "%d", bugs);
    c.text_center(94, 22, a, font_bold24);
    c.fill_rect(60, 32, 67, 35);
    c.rect(0, 0, 127, 63);
    c.text_center(34, 5, "CLAUDE", font_5x7);
    c.text_center(94, 5, "BUGS", font_5x7);

    bool flashing = rolls && t >= FLASH && t < DONE;
    if (flashing && word) {
        if ((t - FLASH) % 300 < 200) c.text_center(64, 53, "GOAL!", font_5x7);
        return;
    }
    if (flashing && (t - FLASH) % 150 < 75) c.rect(18, 19, 50, 47);
    char line[16];
    if (x.pct5h >= 0) snprintf(line, sizeof(line), "5h %d%%", x.pct5h);
    else snprintf(line, sizeof(line), "5h --");
    c.text_center(64, 53, line, font_5x7);
}

// ------------------------------------------------------------------ cards

static void card(Canvas& c, int x, int y, bool red) {
    if (red) {
        c.fill_rect(x, y, x + 15, y + 21);
        return;
    }
    c.rect(x, y, x + 15, y + 21);
    for (int k = y + 3; k < y + 20; k += 3) c.hline(x + 3, x + 12, k);
}

static void card_anim(Canvas& c, uint32_t t, const Ctx& x, bool red) {
    uint32_t h = hash32(x.seed + (red ? 31 : 37));
    bool left = h & 1;                      // which side the ref holds it
    bool extra = h & 2;                     // tear (red) or sweat drop (yellow)
    int cx = left ? 8 : 104;
    const uint32_t UP = 300, SHOWN = 650, HOLD = 2410, GONE = 2710, BLINK = GONE + BLINK_MS;

    Eye react = red ? eye(0, 4, EYE_W, EYE_H, SAD) : eye(0, -2, 15, 26);
    react.dx = left ? 3 : -3;               // leans away from the card
    Eye e;
    if (t < SHOWN) e = Eye();
    else if (t < GONE) {
        if (red) e = react;
        else {
            float k = ease(seg(t, SHOWN, SHOWN + 120));
            e = eye(react.dx * k, -2 * k, lerp(EYE_W, 15, k), lerp(EYE_H, 26, k));
        }
    } else if (t < BLINK) {
        float k = (t - GONE) / (float)BLINK_MS;
        e = blink_eye(k < 0.5f ? react : Eye(), k);
    }
    face(c, e);

    if (t >= UP && t < GONE) {
        float y;
        if (t < SHOWN) y = 64 - 54 * ease(seg(t, UP, SHOWN));
        else if (t < HOLD) y = 10 - ((t - SHOWN) % 360 < 90 ? 1 : 0);
        else y = 10 + 54 * ease_in(seg(t, HOLD, GONE));
        card(c, cx, iround(y), red);
    }
    if (extra && t >= SHOWN + 200 && t < GONE) {
        float k = seg(t, SHOWN + 200, GONE);
        float ex = (left ? RIGHT_X : LEFT_X) + react.dx;
        if (red) tear(c, ex + (left ? 4 : -4), 46 + 14 * k);
        else sweat(c, left ? 112 : 16, 10 + 6 * k);
    }
}

static void a_yellow_card(Canvas& c, uint32_t t, const Ctx& x) { card_anim(c, t, x, false); }
static void a_red_card(Canvas& c, uint32_t t, const Ctx& x) { card_anim(c, t, x, true); }

// --------------------------------------------------------------- var check
// loops while Claude waits for you: replay on the screen, then the scan line

static void var_eyes(Canvas& c, uint32_t t, uint32_t seed) {
    uint32_t p = t / 4000, in = t % 4000;
    uint32_t h = hash32(p * 7919 + seed);
    float dx = 2 + 2.5f * sinf(t / 260.0f);
    Eye e = eye(dx, 0, 8, 16);
    uint32_t b = 500 + h % 2800;            // a blink somewhere in this loop
    if (in >= b && in < b + BLINK_MS) e = blink_eye(e, (in - b) / (float)BLINK_MS);
    if ((h & 0x30000) == 0) {               // sometimes a double blink
        uint32_t b2 = b + BLINK_MS + 100;
        if (in >= b2 && in < b2 + BLINK_MS) e = blink_eye(e, (in - b2) / (float)BLINK_MS);
    }
    draw_eye(c, 16, 30, e);
    Eye r = e;
    r.mirror = true;
    draw_eye(c, 38, 30, r);
}

static void a_var_check(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t p = t / 4000, in = t % 4000;
    uint32_t h = hash32(p * 104729 + x.seed);
    bool flip = h & 1;                      // replay runs the other way
    float stop = 0.7f + 0.25f * hashf(h >> 3);   // where the ball ends up

    var_eyes(c, t, x.seed);

    c.rect(62, 8, 122, 50);
    c.text_center(92, 11, "VAR", font_bold12);
    c.hline(64, 120, 23);
    c.hline(65, 119, 47);                   // the pitch in the replay

    // replay: plays, rewinds fast, plays again slowly and freezes
    float k;
    bool rew = false;
    if (in < 1100) k = in / 1100.0f;
    else if (in < 1400) {
        k = 1 - (in - 1100) / 300.0f;
        rew = true;
    } else if (in < 2300) k = (in - 1400) / 900.0f;
    else k = 1;
    k *= stop;
    float bx = flip ? 98 - 30 * k : 86 + 30 * k;
    float px = flip ? bx + 4 : bx - 22;     // little player behind the ball
    int step = (int)(k * 30) % 2;
    clawd(c, iround(px), 34 - step, pose(EYES_OPEN, ARMS_DOWN, step ? FEET_LEFT : FEET_RIGHT), 0.5f);
    ball(c, bx, 44, 2);
    if (rew) {                              // << while rewinding
        float a[] = {74, 28, 70, 31, 74, 34};
        float b[] = {79, 28, 75, 31, 79, 34};
        c.fill_poly(a, 3);
        c.fill_poly(b, 3);
    }
    if (in >= 2300) {
        // frozen: offside line at the ball, scan line going over it
        int lx = iround(bx);
        for (int y = 25; y < 47; y += 3) c.vline(lx, y, y + 1);
        float s = ((in - 2300) % 850) / 850.0f;
        int sy = iround(25 + 21 * (s < 0.5f ? s * 2 : 2 - s * 2));
        c.hline(64, 120, sy);
    }

    // "checking" with dots that do not push the word around
    const char* w = "checking...";
    int x0 = 92 - c.text_width(w, font_5x7) / 2;
    int end = c.text(x0, 54, "checking", font_5x7);
    int dots = (t / 300) % 4;
    for (int i = 0; i < dots; i++) end = c.text(end, 54, ".", font_5x7);
}

// ------------------------------------------------------------ substitution

static void a_substitution(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t h = hash32(x.seed + 4242);
    bool numbers = h & 1;                   // a real LED board with shirt numbers
    int out = 2 + (h >> 4) % 22, in = 2 + (h >> 12) % 22;
    if (in == out) in = out % 23 + 2;
    const uint32_t UP = 300, DOWN = 2400, END = 2700;

    float y = 0;
    if (t < UP) y = 60 * (1 - ease(seg(t, 0, UP)));
    else if (t >= DOWN) y = 60 * ease_in(seg(t, DOWN, END));
    int oy = iround(y);
    if (oy < 60) {
        c.fill_rect(24, 4 + oy, 104, 44 + oy, false);
        c.rect(24, 4 + oy, 104, 44 + oy);
        c.fill_rect(62, 44 + oy, 66, 52 + oy);
        bool blink = (t / 180) % 2 == 0;
        if (numbers) {
            char s[4];
            c.text(30, 10 + oy, "OUT", font_5x7);
            c.text(30, 29 + oy, "IN", font_5x7);
            snprintf(s, sizeof(s), "%d", out);
            c.text_center(80, 6 + oy, s, font_bold16);
            snprintf(s, sizeof(s), "%d", in);
            c.text_center(80, 25 + oy, s, font_bold16);
            if (blink) {
                float d[] = {55, 11.f + oy, 61, 11.f + oy, 58, 15.f + oy};
                float u[] = {55, 34.f + oy, 61, 34.f + oy, 58, 30.f + oy};
                c.fill_poly(d, 3);
                c.fill_poly(u, 3);
            }
        } else {
            c.text(34, 10 + oy, "OUT", font_5x7);
            c.text(58, 10 + oy, "SONNET", font_5x7);
            c.text(34, 28 + oy, "IN", font_5x7);
            c.text(58, 28 + oy, "OPUS", font_5x7);
            if (blink) {
                float d[] = {26, 11.f + oy, 32, 11.f + oy, 29, 15.f + oy};
                float u[] = {26, 33.f + oy, 32, 33.f + oy, 29, 29.f + oy};
                c.fill_poly(d, 3);
                c.fill_poly(u, 3);
            }
        }
    }
    // the fourth official peeking from both sides
    Eye e = eye(0, 0, 6, 10);
    uint32_t b = t % 1300;
    if (b >= 900 && b < 900 + BLINK_MS) e = blink_eye(e, (b - 900) / (float)BLINK_MS);
    draw_eye(c, 14, 54, e);
    e.mirror = true;
    draw_eye(c, 114, 54, e);
}

// ----------------------------------------------------------- stoppage time

static void a_stoppage_time(Canvas& c, uint32_t t, const Ctx& x) {
    static const int ADD[] = {2, 3, 3, 4, 4, 5, 6, 7};
    int n = ADD[hash32(x.seed + 9) % 8];
    const uint32_t LOOK = 250, UP = 700, DOWN = 2350, GONE = 2650, BACK = 2900;

    float k = t < DOWN ? ease(seg(t, 0, LOOK)) : 1 - ease(seg(t, GONE, BACK));
    Eye e = eye(-14 * k, 4 * k);
    e.lid = 0.2f * k;
    face(c, e);

    float by;
    if (t < LOOK) return;
    if (t < UP) by = 64 - 60 * ease(seg(t, LOOK, UP));
    else if (t < DOWN) by = 4;
    else by = 4 + 60 * ease_in(seg(t, DOWN, GONE));
    int y = iround(by);
    if (y > 63) return;
    c.fill_rect(80, y, 126, y + 34, false);
    c.rect(80, y, 126, y + 34);
    char s[4];
    snprintf(s, sizeof(s), "+%d", n);
    bool on = t < UP || t >= DOWN || (t - UP) % 440 < 330;
    if (on) c.text_center(103, y + 6, s, font_bold24);
}

// ----------------------------------------------------------------- full time

static void a_full_time(Canvas& c, uint32_t t, const Ctx& x) {
    bool left = hash32(x.seed + 11) & 1;
    const uint32_t IN = 200, OUT = 2150;
    static const float B[] = {0, -2, -3, -2};
    if (t < IN) {
        face(c);
        return;
    }
    // three blasts of the whistle: short, short, long
    uint32_t u = t - IN;
    bool peep = u < 250 || (u >= 400 && u < 650) || (u >= 800 && u < 1500);
    int wx = left ? 22 : 106, wy = 54;
    if (t < OUT) {
        whistle(c, wx, wy, !left);
        if (peep) peep_arcs(c, wx + (left ? -6 : 6), wy - 4, left, u);
        c.text_center(64, 1, "FULL TIME", font_6x10);
    }

    if (t < IN + BLINK_MS) {                // blink into the happy face
        float k = (t - IN) / (float)BLINK_MS;
        if (k < 0.5f) face(c, blink_eye(Eye(), k));
        else face(c, eye(0, 4, EYE_W, EYE_H, HAPPY));
        return;
    }
    if (t < OUT) {
        face(c, eye(0, 4 + pickf(B, 4, u / 90.0f), EYE_W, EYE_H, HAPPY));
        return;
    }
    if (t < OUT + BLINK_MS) {
        float k = (t - OUT) / (float)BLINK_MS;
        face(c, k < 0.5f ? eye(0, 4, EYE_W, EYE_H, HAPPY) : blink_eye(Eye(), k));
        return;
    }
    face(c);
}

// ------------------------------------------------------------------ kickoff
// centre circle, whistle, mini Clawd taps the ball and runs after it

static void a_kickoff(Canvas& c, uint32_t t, const Ctx& x) {
    bool m = hash32(x.seed + 21) & 1;
    auto mx = [m](float v) { return m ? 127 - v : v; };
    const uint32_t PEEP = 300, TAP = 1200, RUN = 1400;
    const int Y = 31;

    // pitch in perspective: halfway line and a flat centre circle
    c.ellipse(24, 40, 104, 60);
    c.vline(64, 36, 63);
    c.fill_circle(64, 50, 1);

    if (t >= PEEP && t < TAP) {
        uint32_t u = t - PEEP;
        if (m) whistle(c, 97, 12, true);
        else whistle(c, 30, 12, false);
        if (u % 450 < 330) {
            peep_arcs(c, m ? 101 : 26, 8, !m, u);
            if (m) c.text(78 - c.text_width("PEEP!", font_bold12), 6, "PEEP!", font_bold12);
            else c.text(50, 6, "PEEP!", font_bold12);
        }
    }

    float bx = 64, X = 28;
    Pose p = pose(EYES_OPEN);
    if (t < TAP) {
        p.eyes = (t / 600) % 2 ? EYES_OPEN : (m ? EYES_LEFT : EYES_RIGHT);
    } else if (t < RUN) {
        p.feet = m ? FEET_RIGHT : FEET_LEFT;
        p.eyes = m ? EYES_LEFT : EYES_RIGHT;
    } else {
        uint32_t u = t - RUN;
        X = 28 + u * 0.075f;
        p.feet = (u / 90) % 2 ? FEET_RIGHT : FEET_LEFT;
        p.eyes = m ? EYES_LEFT : EYES_RIGHT;
    }
    if (t >= TAP) bx = 64 + (t - TAP) * 0.09f;
    int sx = m ? 128 - SPRITE_W - iround(X) : iround(X);
    c.fill_rect(sx, Y, sx + SPRITE_W - 1, Y + 19, false);   // stands in front of the lines
    clawd(c, sx, Y, p);
    if (t >= TAP && t < TAP + 90) kick_leg(c, sx, Y, !m);
    if (bx < 140) ball(c, mx(bx), 50, 3);
}

// ---------------------------------------------------------------- half time
// sitting by a cup of tea, HT on the board. loops

static void cup(Canvas& c, int x, int y) {
    c.fill_rect(x, y, x + 6, y + 7);
    c.arc(x + 4, y + 1, x + 10, y + 6, 270, 90, 2);
    c.hline(x - 1, x + 7, y - 1);
}

static void a_half_time(Canvas& c, uint32_t t, const Ctx& x) {
    const uint32_t P = 6000;
    uint32_t in = t % P;
    const int X = 26, Y = 47;
    char s[12];

    // board
    c.rect(86, 4, 124, 42);
    c.text_center(105, 8, "HT", font_bold16);
    int a = x.done < 0 ? 0 : (x.done > 9 ? 9 : x.done), b = x.bugs < 0 ? 0 : (x.bugs > 9 ? 9 : x.bugs);
    snprintf(s, sizeof(s), "%d-%d", a, b);
    c.text_center(105, 27, s, font_bold12);
    c.hline(0, 127, 63);

    // a sip every loop: lifts the cup, eyes closed, puts it back
    float lift = ease(seg(in, 2600, 3000)) * (1 - ease(seg(in, 4200, 4600)));
    bool sipping = in >= 2900 && in < 4300;
    Eyes e = sipping ? EYES_CLOSED : EYES_OPEN;
    uint32_t bl = in % 3000;
    if (!sipping && bl >= 1500 && bl < 1650) e = EYES_CLOSED;   // blink
    if ((hash32(t / P + x.seed) & 1) && in >= 300 && in < 1300) e = EYES_RIGHT;   // glances at the board
    int bob = iround(breathe(in, 2000));
    clawd(c, X, Y + bob, pose(e, lift > 0.3f ? ARMS_ONE_UP : ARMS_DOWN, FEET_NONE));

    int cx = iround(lerp(66, X + 37, lift)), cy = iround(lerp(55, 40, lift));
    cup(c, cx, cy);
    // steam
    for (int k = 0; k < 2; k++) {
        float ph = fmodf(in / 1400.0f + k * 0.5f, 1.0f);
        int sx = cx + 1 + k * 3;
        for (int j = 0; j < 4; j++) {
            float yy = cy - 3 - ph * 12 - j * 2;
            float xx = sx + 1.5f * sinf(yy / 2.5f + k);
            if (ph < 0.8f) c.px(iround(xx), iround(yy));
        }
    }
    if (sipping && in > 3300 && in < 4100) c.text(X + 6, Y - 9, "ahh", font_4x6);
}

static const Anim A_SCORE = {"scoreboard", a_scoreboard, 3200, SCENE};
static const Anim A_YELLOW = {"yellow_card", a_yellow_card, 3150, CALM};
static const Anim A_RED = {"red_card", a_red_card, 3150, CALM};
static const Anim A_VAR = {"var_check", a_var_check, 4000, LOOP | SCENE};
static const Anim A_SUB = {"substitution", a_substitution, 2700, SCENE};
static const Anim A_STOPPAGE = {"stoppage_time", a_stoppage_time, 2950, CALM};
static const Anim A_FULL = {"full_time", a_full_time, 2600, CALM};
static const Anim A_KICKOFF = {"kickoff", a_kickoff, 3000, SCENE};
static const Anim A_HALF = {"half_time", a_half_time, 6000, LOOP | SCENE};

ANIM_LIST(anims_match, &A_SCORE, &A_YELLOW, &A_RED, &A_VAR, &A_SUB, &A_STOPPAGE, &A_FULL, &A_KICKOFF, &A_HALF);
