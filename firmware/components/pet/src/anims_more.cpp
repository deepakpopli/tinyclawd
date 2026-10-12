// more small moments: coffee, a rubber duck, stretching, juggling, a bug,
// a git branch, a paper plane and a night sky
#include <string.h>

#include "anim.h"

static const float PI = (float)M_PI;

// ------------------------------------------------------------------- helpers

// tiny pixel art from strings, '#' = on. flip mirrors it (rows must be the same width)
static void art(Canvas& c, int x, int y, const char* const* rows, int n, bool flip = false, bool on = true) {
    for (int r = 0; r < n; r++) {
        int w = strlen(rows[r]);
        for (int i = 0; i < w; i++)
            if (rows[r][i] == '#') c.px(flip ? x + w - 1 - i : x + i, y + r, on);
    }
}

// which way a scene faces, m = mirrored
struct Side {
    bool m;
    int px(int v) const { return m ? 127 - v : v; }
    int box(int v, int w) const { return m ? 128 - w - v : v; }   // left edge of something w wide
    Eyes look(bool fwd) const { return fwd != m ? EYES_RIGHT : EYES_LEFT; }
};

// ------------------------------------------------------------------- coffee

static const int8_t WIG[] = {0, 1, 1, 0, -1, -1};

// two wisps of steam, the wiggle travels up
static void steam(Canvas& c, int x, int y, uint32_t t, uint32_t seed, float blow) {
    for (int k = 0; k < 2; k++) {
        int ph = t / 110 + k * 3 + (seed & 3);
        int len = 5 + (hash32((t / 1300) * 3 + k + seed) & 3);
        for (int i = 0; i < len; i++) {
            int xx = x + k * 7 + WIG[((i - ph) % 6 + 6) % 6] + iround(blow * i);
            int yy = y - i * 2;
            if (i == len - 1 && (t / 110) % 2) continue;   // the tip flickers
            c.px(xx, yy);
            c.px(xx, yy - 1);
        }
    }
}

static const char* const MUG_HEART[] = {"##.##", "#####", ".###.", "..#.."};

// mug, top-left at x, top; sh tilts the top sideways
static void mug(Canvas& c, int x, int top, float sh) {
    const int w = 18, h = 14;
    float p[] = {x + sh, (float)top, x + w - 1 + sh, (float)top, (float)(x + w - 1), (float)(top + h - 1),
                 (float)x, (float)(top + h - 1)};
    c.fill_poly(p, 4);
    int s = iround(sh * 0.5f);
    c.hline(x + 1 + iround(sh), x + w - 2 + iround(sh), top + 2, false);
    c.rect(x + w + s, top + 3, x + w + 3 + s, top + 9);
    c.vline(x + w + 1 + s, top + 3, top + 9);
    art(c, x + 7 + s, top + 6, MUG_HEART, 4, false, false);
}

static void a_coffee(Canvas& c, uint32_t t, const Ctx& x) {
    bool blow = x.seed & 1, twice = x.seed & 2;
    uint32_t end = twice ? 5300 : 4300;
    // mug height: below the screen -> resting -> up for a sip
    const Key in[] = {{300, 64}, {800, 46}};
    const Key out[] = {{end, 46}, {end + 450, 66}};
    float top = t < end ? KEYS(in, t) : KEYS(out, t);
    float sh = 0, lid = 0;
    bool happy = false;
    uint32_t sips[2] = {1800u, twice ? 3900u : 99999u};
    for (uint32_t a : sips) {
        if (t < a || t >= a + 1100) continue;
        const Key up[] = {{a, 46}, {a + 260, 36}, {a + 820, 36}, {a + 1100, 46}};
        const Key tilt[] = {{a + 150, 0}, {a + 350, -4}, {a + 700, -4}, {a + 900, 0}};
        const Key cl[] = {{a + 100, 0}, {a + 250, 1}, {a + 850, 1}, {a + 1000, 0.3f}};
        top = KEYS(up, t);
        sh = KEYS(tilt, t);
        lid = KEYS(cl, t);
    }
    if ((t >= 2900 && t < 3800) || (twice && t >= 5000 && t < 5300)) happy = true;
    Eye e;
    float look = clamp01((64 - top) / 18);
    e.dy = 3 * look;
    e.h = EYE_H - 2 * look;
    e.lid = lid;
    if (blow && t >= 1100 && t < 1650) e.lid = 0.35f * sinf(PI * seg(t, 1100, 1650));
    if (happy) {
        e.shape = HAPPY;
        e.dy = 2 + breathe(t, 600);
        face(c, e);
        if (t >= 2950 && t < 3650) c.text_center(64, 1, "ahh", font_5x7);
    } else if (t >= 3800 && t < 3900) {
        face(c, blink_eye(e, 0.5f + 0.5f * seg(t, 3800, 3900)));
    } else {
        face_blink(c, t, twice ? 3500 : 99999, e);
    }
    if (top < 64) {
        float b = blow && t >= 1100 && t < 1650 ? 0.9f * sinf(PI * seg(t, 1100, 1650)) : 0;
        if (fabsf(sh) < 1) steam(c, 59 + iround(b * 2), iround(top) - 3, t, x.seed, b);
        mug(c, 55, iround(top), sh);
    }
}

static const Anim A_COFFEE = {"coffee", a_coffee, 5800, CALM};

ANIM_LIST(anims_more, &A_COFFEE);
