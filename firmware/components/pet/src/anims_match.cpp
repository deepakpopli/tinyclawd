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

static const Anim A_SCORE = {"scoreboard", a_scoreboard, 3200, SCENE};
static const Anim A_YELLOW = {"yellow_card", a_yellow_card, 3150, CALM};
static const Anim A_RED = {"red_card", a_red_card, 3150, CALM};
static const Anim A_FULL = {"full_time", a_full_time, 2600, CALM};

ANIM_LIST(anims_match, &A_SCORE, &A_YELLOW, &A_RED, &A_FULL);
