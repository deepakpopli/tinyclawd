// football: the mini Clawd on a pitch, and a few big-eye ones with a ball.
// most scenes can play towards a goal on the right or (mirrored) on the left.
#include "anim.h"

// ------------------------------------------------------------------ helpers

// which way the scene faces. m = goal on the left, everything mirrored
struct Side {
    bool m;
    float x(float v) const { return m ? 127 - v : v; }
    // left edge of something w wide that sits at v in the unmirrored scene
    int sx(float v, int w = SPRITE_W) const { return m ? 128 - w - iround(v) : iround(v); }
    float deg(float d) const { return m ? -d : d; }
    Feet plant() const { return m ? FEET_RIGHT : FEET_LEFT; }   // standing foot when kicking
    Eyes look(bool fwd) const { return fwd != m ? EYES_RIGHT : EYES_LEFT; }
};

static Side side_of(uint32_t seed) { return Side{(hash32(seed ^ 0x51de) & 1) != 0}; }

static void pitch(Canvas& c) { c.hline(0, 127, 63); }

// small goal on the far side, net bulges when the ball goes in
static void goal_net(Canvas& c, Side s, float bulge = 0, int ox = 0, int oy = 0) {
    c.line(s.x(104) + ox, 30 + oy, s.x(127) + ox, 30 + oy);
    c.vline(iround(s.x(104)) + ox, 30 + oy, 62);
    for (int gy = 33; gy < 62; gy += 4)
        for (int gx = 107; gx < 127; gx += 4)
            c.px(iround(s.x(gx + (gy > 38 && gy < 56 ? bulge : 0))) + ox, gy + oy);
    pitch(c);
}

static void kick(Canvas& c, Side s, float x, int y) { kick_leg(c, s.sx(x), y, !s.m); }

// flashing word, 240 on / 80 off
static void shout(Canvas& c, const char* w, int cx, int y, uint32_t u) {
    if (u % 320 < 240) c.text_center(cx, y, w, font_bold16);
}

static const char* goal_word(uint32_t seed) {
    static const char* const W[] = {"GOAL!", "GOAL!", "GOAL!", "GOLAZO", "GET IN"};
    return W[hash32(seed * 13 + 5) % 5];
}

// a ball dropping into the net after the shot, then lying there
static void ball_in_net(Canvas& c, Side s, float y0, uint32_t u) {
    float k = ease_in(seg(u, 0, 260));
    ball(c, s.x(120), lerp(y0, 59, k), 3);
}

static float hop(uint32_t u) {
    static const float J[] = {0, 3, 5, 3, 0};
    return pickf(J, 5, u / 90.0f);
}

// arms up hopping in front of the goal, ball drops in the net
static void celebrate(Canvas& c, Side s, float X, uint32_t u, const char* word, float ball_y) {
    goal_net(c, s, u < 270 ? 2 : 0);
    ball_in_net(c, s, ball_y, u);
    clawd(c, s.sx(X), 38 - iround(hop(u)), pose(EYES_OPEN, ARMS_UP));
    shout(c, word, 64, 1, u);
}

static void running(Canvas& c, Side s, float X, int y, uint32_t t, Eyes e = EYES_OPEN, uint32_t step = 90) {
    clawd(c, s.sx(X), y, pose(e, ARMS_DOWN, (t / step) % 2 ? FEET_RIGHT : FEET_LEFT));
}

// ------------------------------------------------------------- keepy uppy

static void a_keepy_uppy(Canvas& c, uint32_t t, const Ctx& x) {
    const int X = 40, Y = 38;
    uint32_t h = hash32(x.seed + 101);
    int n = 3 + h % 4;                       // touches before the big one
    bool lefty = h & 0x10;                   // which foot starts
    bool balance = ((h >> 5) % 3) == 0;      // sometimes it stays on the head
    float roll = (h & 0x100) ? 1 : -1;       // where the ball rolls off to
    const uint32_t JUG = 2600, hop = JUG / n;

    if (t < JUG) {
        int k = t / hop;
        float u = (t % hop) / (float)hop;
        bool right = (k % 2 == 0) != lefty;
        float fx = right ? X + 34 : X + 2, nx = right ? X + 2 : X + 34;
        float top = 28 + 10 * hashf(x.seed * 7 + k);
        float bx = lerp(fx, nx, u), by = 52 - top * sinf(u * (float)M_PI);
        bool kicking = u < 0.14f;
        Eyes e = bx < X + 12 ? EYES_LEFT : (bx > X + 24 ? EYES_RIGHT : EYES_OPEN);
        clawd(c, X, Y, pose(e, ARMS_DOWN, kicking ? (right ? FEET_LEFT : FEET_RIGHT) : FEET_BOTH));
        if (kicking) kick_leg(c, X, Y, right);
        ball(c, bx, by, 3);
        return;
    }
    bool right = (n % 2 == 0) != lefty;
    float fx = right ? X + 34 : X + 2;
    uint32_t u = t - JUG;
    if (u < 500) {                           // big kick off the top
        float k = u / 500.0f;
        bool kicking = u < 70;
        clawd(c, X, Y, pose(EYES_OPEN, u > 200 ? ARMS_UP : ARMS_DOWN,
                            kicking ? (right ? FEET_LEFT : FEET_RIGHT) : FEET_BOTH));
        if (kicking) kick_leg(c, X, Y, right);
        ball(c, lerp(fx, X + 18, k), 52 - 72 * ease_out(k), 3);
        return;
    }
    u -= 500;
    if (u < 400) {                           // waiting, ball off the top
        clawd(c, X, Y - iround(breathe(u, 400)), pose(EYES_OPEN, ARMS_UP));
        return;
    }
    u -= 400;
    if (u < 350) {                           // comes down
        float k = u / 350.0f;
        clawd(c, X, Y, pose(EYES_OPEN, ARMS_UP));
        ball(c, X + 18, -6 + 40 * k * k, 3);
        return;
    }
    u -= 350;
    if (balance) {
        // lands on the head and stays there, a little wobble to keep it up
        float d = expf(-u / 500.0f);
        float wob = 3 * sinf(u / 90.0f) * d;
        bool wink = u > 900 && u < 1200;
        clawd(c, X + iround(wob * 0.4f), Y, pose(wink ? EYES_WINK : EYES_OPEN, u > 400 ? ARMS_UP : ARMS_DOWN));
        ball(c, X + 18 + wob, 34, 3);
        if (u > 900) sparkle(c, X + 44, Y - 6, u % 300 < 150 ? 3 : 2);
        return;
    }
    if (u < 780) {                           // bonk, stars, ball rolls off
        float k = u / 780.0f;
        clawd(c, X, Y + (u < 90 ? 2 : 0), pose(EYES_CLOSED));
        stars(c, X + 18, Y - 6, k * 2);
        ball(c, X + 18 + roll * 50 * k, 34 - 20 * sinf(k * (float)M_PI) + 26 * k, 3);
        return;
    }
    clawd(c, X, Y);
}

// --------------------------------------------------------------------- goal

static void a_goal(Canvas& c, uint32_t t, const Ctx& x) {
    Side s = side_of(x.seed);
    uint32_t h = hash32(x.seed + 202);
    const int Y = 38;
    const float BX = 54, BY = 59;
    float endy = 38 + 18 * hashf(h);              // where it goes in
    float arc = 4 + 26 * hashf(h >> 9);           // low drive .. lob
    // keep it under the bar at the goal line (u ~ 0.76)
    float bar = lerp(BY, endy, 0.76f) - 34;
    if (arc * 0.68f > bar) arc = bar / 0.68f;
    bool turn = h & 0x40000;
    const char* word = goal_word(x.seed);

    if (t < 630) {                                // run-up
        goal_net(c, s);
        ball(c, s.x(BX), BY, 3);
        running(c, s, 4 + 20 * (t / 630.0f), Y, t);
        return;
    }
    if (t < 780) {
        goal_net(c, s);
        ball(c, s.x(BX), BY, 3);
        clawd(c, s.sx(24), Y, pose(EYES_OPEN, ARMS_DOWN, s.plant()));
        return;
    }
    if (t < 1230) {                               // kick, ball flies in
        float u = (t - 780) / 450.0f;
        goal_net(c, s);
        bool k = t < 860;
        clawd(c, s.sx(24), Y, pose(s.look(true), ARMS_DOWN, k ? s.plant() : FEET_BOTH));
        if (k) kick(c, s, 24, Y);
        ball(c, s.x(BX + 66 * u), lerp(BY, endy, u) - arc * sinf(u * (float)M_PI), 3);
        return;
    }
    uint32_t u = t - 1230;
    if (u < 1040) {                               // GOAL!
        celebrate(c, s, 24, u, word, endy);
        return;
    }
    u -= 1040;
    goal_net(c, s);
    ball(c, s.x(120), 59, 3);
    if (u < 910) {
        int k = u / 70;
        if (turn) clawd_turn(c, s.sx(24), Y, s.m ? TURN_STEPS - 1 - k : k);
        else {
            static const float J[] = {0, 4, 0, 4, 0};   // arm pumps
            clawd(c, s.sx(24), Y - iround(pickf(J, 5, u / 110.0f)),
                  pose(EYES_OPEN, (u / 180) % 2 ? ARMS_UP : ARMS_ONE_UP));
        }
        return;
    }
    clawd(c, s.sx(24), Y, pose(EYES_WINK));
}

// ---------------------------------------------------------------- ball watch
// big eyes: a ball bounces past, the eyes follow it, it comes back and bonks an eye

static float track_dx(float bx) { return clampf((bx - 64) / 5, -12, 12); }
static float track_dy(float by) { return clampf((by - 32) / 4, -6, 6); }

static void a_ball_watch(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t h = hash32(x.seed + 303);
    float m = (h & 1) ? -1 : 1;                    // -1: comes from the right
    int nb = 2 + (h >> 1) % 3;                     // bounces
    float hop = 34 + 8 * hashf(h >> 4);
    auto mx = [m](float v) { return m > 0 ? v : 128 - v; };
    const uint32_t T0 = 300, T1 = T0 + 1485, T2 = T1 + 300, T3 = T2 + 495, T4 = T3 + 900;

    if (t < T0) {
        face(c);
        return;
    }
    if (t < T1) {                                  // bounces across
        float u = (t - T0) / (float)(T1 - T0);
        float bx = -8 + 144 * u, by = 56 - hop * fabsf(sinf(u * nb * (float)M_PI));
        float w = ease(seg(t, T0, T0 + 350));
        face(c, eye(m * track_dx(bx) * w, track_dy(by) * w));
        ball(c, mx(bx), by, 5);
        return;
    }
    if (t < T2) {                                  // it is gone, eyes stay on that side
        face(c, eye(m * 12, 6 - 2 * ease(seg(t, T1, T2)) + breathe(t - T1, 300)));
        return;
    }
    if (t < T3) {                                  // comes back in the air, aimed at an eye
        float u = (t - T2) / (float)(T3 - T2);
        float bx = 136 - 50 * u, by = lerp(40, 16, u) - 20 * sinf(u * (float)M_PI);
        float dy = lerp(4, -3, ease(seg(t, T2, T2 + 200)));
        face(c, eye(m * track_dx(bx), dy));
        ball(c, mx(bx), by, 5);
        return;
    }
    if (t < T4) {                                  // bonk
        float u = (t - T3) / (float)(T4 - T3);
        Eye l = eye(-2, 0, EYE_W, EYE_H, SQUINT), r = eye(2, 0, EYE_W, EYE_H, SQUINT);
        if (t - T3 < 60) l.dy = r.dy = 2;
        r.mirror = true;
        face(c, l, r);
        stars(c, mx(86), 8, u * 2);
        ball(c, mx(86 + 38 * u), 16 - 30 * u + 80 * u * u, 5);
        return;
    }
    if (t < T4 + BLINK_MS) {
        face(c, blink_eye(Eye(), (t - T4) / (float)BLINK_MS));
        return;
    }
    face(c);
}

static const Anim A_KEEPY = {"keepy_uppy", a_keepy_uppy, 5130, SCENE};
static const Anim A_GOAL = {"goal", a_goal, 3880, SCENE};
static const Anim A_WATCH = {"ball_watch", a_ball_watch, 4130, CALM};

ANIM_LIST(anims_football, &A_KEEPY, &A_GOAL, &A_WATCH);
