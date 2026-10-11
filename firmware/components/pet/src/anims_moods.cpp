// moods: surprised, sad, angry, dizzy, love and the > < family
#include "anim.h"

static const float PI = (float)M_PI;

// ------------------------------------------------------------------- helpers

// clear the top part of an eye: a lid that works for every shape, k 0 open .. 1 shut
static void lid_mask(Canvas& c, float cx, const Eye& e, float k) {
    if (k <= 0) return;
    float top = EYE_Y + e.dy - e.h / 2 - 3, bot = EYE_Y + e.dy + e.h / 2 + 2;
    c.fill_rect(iround(cx + e.dx - e.w / 2 - 5), iround(top) - 6, iround(cx + e.dx + e.w / 2 + 5),
                iround(lerp(top, bot - 2, k)), false);
}

// a mood face with a lid on both eyes
static void mood(Canvas& c, const Eye& l, const Eye& r, float k = 0) {
    face(c, l, r);
    lid_mask(c, LEFT_X, l, k);
    lid_mask(c, RIGHT_X, r, k);
}

static void mood(Canvas& c, const Eye& e, float k = 0) {
    Eye r = e;
    r.mirror = true;
    mood(c, e, r, k);
}

// plain face -> mood: plain eyes close over [a, a+120), mood opens over [a+120, a+240).
// mood -> plain: same backwards starting at b. returns false when the plain face was drawn.
static const uint32_t SW = 120;

static bool mood_window(Canvas& c, uint32_t t, uint32_t a, uint32_t b, float& lid) {
    lid = 0;
    if (t < a || t >= b + 2 * SW) {
        face(c);
        return false;
    }
    if (t < a + SW) {
        face(c, blink_eye(Eye(), 0.5f * seg(t, a, a + SW)));
        return false;
    }
    if (t >= b + SW) {
        face(c, blink_eye(Eye(), 0.5f + 0.5f * seg(t, b + SW, b + 2 * SW)));
        return false;
    }
    if (t < a + 2 * SW) lid = 1 - seg(t, a + SW, a + 2 * SW);
    if (t >= b) lid = seg(t, b, b + SW);
    return true;
}

static Eye squint_eye(float dx, float dy, bool right) {
    Eye e;
    e.shape = SQUINT;
    e.dx = right ? -dx : dx;
    e.dy = dy;
    e.mirror = right;
    return e;
}

static void squints(Canvas& c, float dx, float dy, float lid = 0) {
    mood(c, squint_eye(dx, dy, false), squint_eye(dx, dy, true), lid);
}

// ----------------------------------------------------------------- surprised

static void a_surprised(Canvas& c, uint32_t t, const Ctx& x) {
    const Key w[] = {{0, 1}, {500, 1}, {560, 1.4f}, {620, 1.6f}, {680, 1.5f}, {1380, 1.5f}, {1630, 1}};
    const Key hk[] = {{0, 1}, {500, 1}, {560, 1.25f}, {1380, 1.25f}, {1630, 1}};
    const Key dy[] = {{0, 0}, {500, 0}, {560, -2}, {1380, -2}, {1630, 0}};
    float side = (x.seed & 1) ? ((x.seed & 2) ? 5.f : -5.f) : 0;
    const Key dx[] = {{0, 0}, {700, 0}, {800, side}, {1300, side}, {1500, 0}};
    Eye e;
    e.w = EYE_W * KEYS(w, t);
    e.h = EYE_H * KEYS(hk, t);
    e.dy = KEYS(dy, t);
    e.dx = KEYS(dx, t);
    face_blink(c, t, 1800, e);
    if (t >= 600 && t < 1380) {
        int jump = t < 680 ? 2 : 0;
        c.text(61, 0 - jump, "!", font_bold16);
    }
}

// ----------------------------------------------------------------------- sad

static void a_sad(Canvas& c, uint32_t t, const Ctx& x) {
    float lid;
    if (!mood_window(c, t, 100, 3000, lid)) return;
    Eye e;
    e.shape = SAD;
    e.dy = 4 + breathe(t, 1600);
    mood(c, e, lid);
    bool right = x.seed & 1;
    float tx = right ? RIGHT_X - 3 : LEFT_X + 3;
    if (t >= 600 && t < 2600) tear(c, tx, 50 + 16 * ease_in(seg(t, 600, 2600)));
    if ((x.seed & 2) && t >= 1400 && t < 2900) tear(c, right ? LEFT_X + 3 : RIGHT_X - 3, 50 + 16 * ease_in(seg(t, 1400, 2900)));
}

// --------------------------------------------------------------------- angry

static void a_angry(Canvas& c, uint32_t t, const Ctx&) {
    float lid;
    if (!mood_window(c, t, 100, 2200, lid)) return;
    uint32_t q = t - 100;
    float sh = (q % 560) < 280 ? ((q / 35) % 2 ? 1.2f : -1.2f) : 0;
    Eye e;
    e.shape = ANGRY;
    e.dx = sh;
    e.dy = 1;
    mood(c, e, lid);
    // pulsing vein in the corner
    float k = (q % 560) < 280 ? 1 : 0;
    int x0 = 110 - (int)k, y0 = 4 - (int)k, s = 4 + (int)k;
    c.line(x0, y0, x0 + s, y0 + s);
    c.line(x0 + 2 * s, y0, x0 + s, y0 + s);
    c.line(x0, y0 + 2 * s + 2, x0 + s, y0 + s + 2);
    c.line(x0 + 2 * s, y0 + 2 * s + 2, x0 + s, y0 + s + 2);
}

// --------------------------------------------------------------------- dizzy

static void a_dizzy(Canvas& c, uint32_t t, const Ctx& x) {
    float amp = 0.7f + 0.3f * sinf(t / 3100.0f);
    Eye e;
    e.shape = SPIRAL;
    e.phase = 2 * PI * t / 1440.0f;
    e.dx = 3 * amp * sinf(t / 240.0f);
    e.dy = 1.5f * sinf(t / 410.0f);
    face(c, e);
    // stars circle over the head now and then
    uint32_t slot = t / 3000, h = hash32(slot * 3 + x.seed);
    if (h % 3 != 0) stars(c, 64 + e.dx, 6, t / 900.0f);
}

// ---------------------------------------------------------------------- love

static float bump(float p, float at) {
    float d = (p - at) / 0.08f;
    return expf(-d * d);
}

static void a_love(Canvas& c, uint32_t t, const Ctx& x) {
    float p = (t % 900) / 900.0f;   // heartbeat: lub dub
    float k = 1 + 0.22f * (bump(p, 0.1f) + 0.7f * bump(p, 0.32f));
    Eye e;
    e.shape = HEART;
    e.w = EYE_W * k;
    e.dx = 2 * sinf(t / 700.0f);
    e.dy = -2;
    face(c, e);
    // little hearts float up from the sides
    for (int j = 0; j < 2; j++) {
        float q = t / 1700.0f + j * 0.5f;
        uint32_t n = (uint32_t)q;
        float f = q - n;
        uint32_t h = hash32(n * 5 + j + x.seed);
        float hx = (h & 1) ? 112 + (h >> 3) % 8 : 10 + (h >> 3) % 8;
        heart(c, hx + 3 * sinf(f * 7), 60 - 56 * f, 0.35f + 0.15f * f);
    }
}

// -------------------------------------------------------------------- petted

static void a_petted(Canvas& c, uint32_t t, const Ctx& x) {
    float lid;
    if (!mood_window(c, t, 150, 1700, lid)) return;
    float rub = 2 * sinf((t - 150) / 160.0f);   // leans into your finger
    squints(c, rub, 2, lid);
    float p = seg(t, 300, 1650);
    if (t >= 300 && t < 1650) {
        float side = (x.seed & 1) ? 1 : -1;
        heart(c, 64 + side * 4 * sinf(p * 6), 46 - 36 * p, 0.9f + 0.3f * p);
    }
}

// -------------------------------------------------------------------- giggle

static const float GB[] = {0, -2, -3, -2};

static void a_giggle(Canvas& c, uint32_t t, const Ctx&) {
    float lid;
    if (!mood_window(c, t, 150, 1600, lid)) return;
    uint32_t q = t - 150;
    float sh = 2 * sinf(q / 60.0f * PI / 2);
    float b = pickf(GB, 4, q / 60.0f);
    squints(c, sh, b + 2, lid);
    if ((q / 120) % 2 == 0) {
        static const int T[][4] = {{20, 22, 26, 20}, {20, 32, 27, 32}, {20, 42, 26, 44},
                                   {108, 20, 102, 22}, {108, 32, 101, 32}, {108, 44, 102, 42}};
        for (auto& l : T) c.line(l[0], l[1], l[2], l[3]);
    }
}

// ----------------------------------------------------------------------- shy

static void blush(Canvas& c, float x, float y) {
    for (int k = 0; k < 3; k++) c.line(x + k * 4, y + 3, x + k * 4 + 3, y);
}

static void a_shy(Canvas& c, uint32_t t, const Ctx&) {
    float lid;
    if (!mood_window(c, t, 150, 2700, lid)) return;
    float sway = 2 * sinf(2 * PI * (t - 150) / 2200.0f);
    float dy = 4 * ease(seg(t, 270, 600));
    squints(c, sway, dy, lid);
    if (t > 400 && t < 2700) {
        blush(c, LEFT_X - 8 + sway, 48);
        blush(c, RIGHT_X - 4 + sway, 48);
    }
}

// --------------------------------------------------------------------- laugh

static const float LB[] = {0, -4, -5, -4, 0, 1};

static void a_laugh(Canvas& c, uint32_t t, const Ctx&) {
    float lid;
    if (!mood_window(c, t, 100, 2100, lid)) return;
    uint32_t q = t - 100;
    float b = pickf(LB, 6, q / 60.0f);
    squints(c, 0, b, lid);
    if (q > 240 && t < 2100) {
        float p = ((q - 240) % 720) / 720.0f;
        float y = 30 + b - 14 * sinf(p * PI) + 12 * p;
        tear(c, LEFT_X - 12 - 18 * p, y);
        tear(c, RIGHT_X + 12 + 18 * p, y);
    }
}

// ------------------------------------------------------------------- content

static void a_content(Canvas& c, uint32_t t, const Ctx& x) {
    float lid;
    if (!mood_window(c, t, 200, 3800, lid)) return;
    Eye e;
    e.shape = HAPPY;
    e.dx = 3 * sinf(2 * PI * (t - 200) / 2400.0f);
    e.dy = 2 + sinf(2 * PI * (t - 200) / 1200.0f);
    // purr: a soft buzz in bursts
    uint32_t pq = (t + (x.seed & 0xff)) % 1500;
    if (pq < 600) e.dy += ((t / 45) % 2) * 0.8f;
    mood(c, e, lid);
    for (int j = 0; j < 2; j++) {
        if (t < 400u + j * 800 || t >= 3800) continue;
        float f = fmodf((t - 400 - j * 800) / 1600.0f, 1);
        float side = ((uint32_t)((t - 400 - j * 800) / 1600) + j + x.seed) % 2 ? 1 : -1;
        float mx = side > 0 ? 106 : 14;
        c.text(iround(mx + 3 * sinf(f * 6)), iround(42 - 30 * f), "~", font_6x10);
    }
}

// ------------------------------------------------------------------ startled

static void a_startled(Canvas& c, uint32_t t, const Ctx&) {
    const Key s[] = {{0, 0}, {150, 0}, {230, 1}, {560, 1}, {780, 0}};
    float k = KEYS(s, t);
    if (t >= 150 && t < 230) k = ease_out(seg(t, 150, 230));   // fast up, slower down
    Eye e;
    e.w = lerp(EYE_W, 8, k);
    e.h = lerp(EYE_H, 8, k);
    e.dy = -6 * k;
    if (t > 230 && t < 560) e.dx = 0.7f * sinf(t / 18.0f);
    face_blink(c, t, 880, e);
    if (t > 200 && t < 560) {   // shock lines over each eye
        for (int i = -1; i <= 1; i++) {
            for (int ex = LEFT_X; ex <= RIGHT_X; ex += RIGHT_X - LEFT_X) {
                float bx = ex + i * 6, by = EYE_Y - 6 - 8;
                c.line(bx + i * 1.5f, by - 3, bx + i * 3.f, by - 7);
            }
        }
    }
}

static const Anim A_SURPRISED = {"surprised", a_surprised, 2200, CALM};
static const Anim A_SAD = {"sad", a_sad, 3700, CALM};
static const Anim A_ANGRY = {"angry", a_angry, 2900, CALM};
static const Anim A_DIZZY = {"dizzy", a_dizzy, 6000, LOOP};
static const Anim A_LOVE = {"love", a_love, 6300, LOOP};
static const Anim A_PETTED = {"petted", a_petted, 2700, CALM};
static const Anim A_GIGGLE = {"giggle", a_giggle, 2400, CALM};
static const Anim A_SHY = {"shy", a_shy, 3400, CALM};
static const Anim A_LAUGH = {"laugh", a_laugh, 2700, CALM};
static const Anim A_CONTENT = {"content", a_content, 4400, CALM};
static const Anim A_STARTLED = {"startled", a_startled, 1300, CALM};

ANIM_LIST(anims_moods, &A_SURPRISED, &A_SAD, &A_ANGRY, &A_DIZZY, &A_LOVE, &A_PETTED, &A_GIGGLE,
          &A_SHY, &A_LAUGH, &A_CONTENT, &A_STARTLED);
