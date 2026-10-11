// poses that come from Claude Code itself: blink, looks, wink, the 360 turn,
// the thinking spinner and the mini Clawd from the banner
#include "anim.h"

// ---------------------------------------------------------------- idle face
// the face Clawd shows between everything else. never frozen: it breathes,
// blinks at random and glances around. pure function of t so it is smooth.

static const uint32_t SLOT = 2100;

static void gaze_at(uint32_t slot, uint32_t seed, float& dx, float& dy) {
    uint32_t h = hash32(slot * 2654435761u ^ seed);
    float r = (h & 0xff) / 255.0f;
    if (r < 0.5f || slot == 0) {   // look straight most of the time, and at the start
        dx = 0;
        dy = 0;
        return;
    }
    dx = ((h >> 8) & 0xff) / 255.0f * 20 - 10;
    dy = ((h >> 16) & 0xff) / 255.0f * 7 - 4;
    if (r > 0.9f) dx = dx > 0 ? 12 : -12;   // a proper side look now and then
}

static void idle_face_parts(uint32_t t, uint32_t seed, Eye& e, float& blink, bool& moving) {
    uint32_t slot = t / SLOT, in = t % SLOT;
    float x0, y0, x1, y1;
    gaze_at(slot ? slot - 1 : 0, seed, x0, y0);
    gaze_at(slot, seed, x1, y1);
    uint32_t h = hash32(slot ^ (seed * 31));
    uint32_t move_at = 200 + (h & 0x1ff);   // when in this slot the eyes move
    float k = ease(seg(in, move_at, move_at + 170));
    e = Eye();
    e.dx = lerp(x0, x1, k);
    e.dy = lerp(y0, y1, k) + breathe(t, 2800);
    moving = in >= move_at && in < move_at + 170;

    // blinks: often right when the eyes move, sometimes a double blink
    blink = -1;
    float r = ((h >> 12) & 0xff) / 255.0f;
    uint32_t b1 = r < 0.35f ? move_at - 60 : 900 + ((h >> 20) & 0x3ff);
    if (r < 0.75f && in >= b1 && in < b1 + BLINK_MS) blink = (in - b1) / (float)BLINK_MS;
    if (r < 0.12f && in >= b1 + BLINK_MS + 90 && in < b1 + 2 * BLINK_MS + 90)
        blink = (in - b1 - BLINK_MS - 90) / (float)BLINK_MS;
}

static void a_idle(Canvas& c, uint32_t t, const Ctx& x) {
    Eye e;
    float blink;
    bool moving;
    idle_face_parts(t, x.seed, e, blink, moving);
    face(c, blink >= 0 ? blink_eye(e, blink) : e);
}

// the brain uses this to start the next thing on a calm frame
bool idle_at_rest(uint32_t t, uint32_t seed) {
    Eye e;
    float blink;
    bool moving;
    idle_face_parts(t, seed, e, blink, moving);
    return blink < 0 && !moving && e.dx == 0 && e.dy == 0;
}

// --------------------------------------------------------------------- blink

static void a_blink(Canvas& c, uint32_t t, const Ctx&) {
    Eye e;
    e.dy = breathe(t);
    if (t >= 1400 && t < 1400 + BLINK_MS) e = blink_eye(Eye(), (t - 1400) / (float)BLINK_MS);
    // double blink at the end
    if (t >= 2500 && t < 2500 + BLINK_MS) e = blink_eye(Eye(), (t - 2500) / (float)BLINK_MS);
    face(c, e);
}

// --------------------------------------------------------------- look around

static void a_look_around(Canvas& c, uint32_t t, const Ctx& x) {
    float a = 10 + (x.seed % 5);          // how far
    float s = (x.seed & 0x10) ? -1 : 1;   // which side first
    const Key k[] = {{0, 0}, {500, 0}, {680, -a * s}, {1280, -a * s}, {1460, 0}, {1660, 0},
                     {1840, a * s}, {2440, a * s}, {2620, 0}};
    Eye e;
    e.dx = KEYS(k, t);
    // a little up on one side so it is not a flat sweep
    e.dy = (x.seed & 0x20) ? -2 * fabsf(e.dx) / a : 0;
    face_blink(c, t, 2900, e);
}

// ---------------------------------------------------------------------- wink

static void a_wink(Canvas& c, uint32_t t, const Ctx& x) {
    bool right = !(x.seed & 4);
    const Key k[] = {{0, 0}, {700, 0}, {820, 1}, {1320, 1}, {1440, 0}};
    float v = KEYS(k, t);
    Eye open, shut = blink_eye(Eye(), v * 0.5f);
    shut.mirror = right;
    if (right) face(c, open, shut);
    else {
        Eye r;
        r.mirror = true;
        face(c, shut, r);
    }
    if (v > 0.95f) c.glyph(right ? 104 : 14, 4, 0x2726, font_sym14);   // little star
}

// --------------------------------------------------------------- turn around

static void a_turn_around(Canvas& c, uint32_t t, const Ctx&) {
    // (shift, spacing, width factor, left visible, right visible), 90 ms each
    static const float steps[][5] = {
        {0, 1, 1, 1, 1}, {5, .9f, .9f, 1, 1}, {12, .75f, .8f, 1, 1}, {20, .5f, .6f, 1, 1},
        {28, .25f, .4f, 0, 1}, {34, 0, .25f, 0, 1}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
        {-34, 0, .25f, 1, 0}, {-28, .25f, .4f, 1, 0}, {-20, .5f, .6f, 1, 1}, {-12, .75f, .8f, 1, 1},
        {-5, .9f, .9f, 1, 1}, {0, 1, 1, 1, 1}};
    if (t < 600 || t >= 600 + 14 * 90) {
        face(c);
        return;
    }
    const float* s = steps[(t - 600) / 90];
    float half = (RIGHT_X - LEFT_X) / 2.0f, cx = 64 + s[0];
    Eye e;
    e.w = fmaxf(2, EYE_W * s[2]);
    if (s[3]) draw_eye(c, cx - half * s[1], EYE_Y, e);
    if (s[4]) draw_eye(c, cx + half * s[1], EYE_Y, e);
}

// ------------------------------------------------------------------ thinking

static const uint16_t SPINNER[] = {0x00b7, 0x2722, 0x2733, 0x2736, 0x273b, 0x273d};

void spinner(Canvas& c, int x, int y, uint32_t t) {
    // forward then reversed, like the CLI
    int i = (t / 110) % 12;
    if (i >= 6) i = 11 - i;
    c.glyph(x, y, SPINNER[i], font_sym14);
}

static void a_thinking(Canvas& c, uint32_t t, const Ctx&) {
    float i = t / 110.0f;
    Eye e;
    e.dx = -5 + 3 * sinf(i / 6);
    e.dy = -6;
    // blink now and then while thinking
    face_blink(c, t % 3960, 2600, e);
    spinner(c, 108, 4, t);
}

// ---------------------------------------------------------- mini Clawd poses

static const int SX = (128 - SPRITE_W) / 2, SY = 20;

static void a_spin(Canvas& c, uint32_t t, const Ctx& x) {
    bool back = x.seed & 8;   // sometimes turns the other way
    if (t < 600) clawd(c, SX, SY);
    else if (t < 600 + TURN_STEPS * 85) {
        int k = (t - 600) / 85;
        clawd_turn(c, SX, SY, back ? TURN_STEPS - 1 - k : k);
    } else if (t < 1905 + 200) clawd(c, SX, SY);
    else if (t < 2105 + 500) clawd(c, SX, SY, pose(EYES_WINK));
    else clawd(c, SX, SY);
}

static void a_wave(Canvas& c, uint32_t t, const Ctx&) {
    if (t < 400) {
        clawd(c, SX, SY);
    } else if (t < 1200) {
        int k = (t - 400) / 160;
        clawd(c, SX, SY, pose(EYES_OPEN, k % 2 ? ARMS_DOWN : ARMS_ONE_UP));
    } else if (t < 1920) {
        const Key k[] = {{1200, 0}, {1320, -3}, {1440, -5}, {1820, -5}, {1920, -2}};
        bool wink = t >= 1440 && t < 1820;
        clawd(c, SX, SY + iround(KEYS(k, t)), pose(wink ? EYES_WINK : EYES_OPEN, ARMS_UP));
    } else {
        clawd(c, SX, SY);
    }
}

static const Anim A_IDLE = {"idle", a_idle, 10000, LOOP | CALM};
static const Anim A_BLINK = {"blink", a_blink, 3200, CALM};
static const Anim A_LOOK = {"look_around", a_look_around, 3400, CALM};
static const Anim A_WINK = {"wink", a_wink, 2100, CALM};
static const Anim A_TURN = {"turn_around", a_turn_around, 2460, CALM};
static const Anim A_THINK = {"thinking", a_thinking, 3960, LOOP};
static const Anim A_SPIN = {"spin", a_spin, 3000, SCENE};
static const Anim A_WAVE = {"wave", a_wave, 2400, SCENE};

ANIM_LIST(anims_claude, &A_IDLE, &A_BLINK, &A_LOOK, &A_WINK, &A_TURN, &A_THINK, &A_SPIN, &A_WAVE);
