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

// -------------------------------------------------------------- penalty save

static void goal_centre(Canvas& c) {
    c.hline(30, 98, 16);
    c.vline(30, 16, 62);
    c.vline(98, 16, 62);
    for (int gy = 20; gy < 62; gy += 5)
        for (int gx = 34; gx < 96; gx += 5) c.px(gx, gy);
    pitch(c);
}

static void a_penalty_save(Canvas& c, uint32_t t, const Ctx& x) {
    Side s = side_of(x.seed);
    uint32_t h = hash32(x.seed + 404);
    bool low = h & 2;           // low dive along the ground
    bool parry = h & 4;         // pushes it away instead of holding it
    const int KX = 46, KY = 38;
    float tx = 38, ty = low ? 46 : 26;          // where the shot goes
    goal_centre(c);
    if (t < 600) {                               // ready, bouncing on its feet
        int i = t / 120;
        clawd(c, s.sx(KX), KY - (i % 2), pose(EYES_OPEN, ARMS_DOWN, i % 2 ? FEET_RIGHT : FEET_LEFT));
        ball(c, 64, 58, 6);
        return;
    }
    if (t < 1100) {                              // shot + dive
        float u = (t - 600) / 500.0f;
        float kx = KX - 22 * ease(u);
        float cy = low ? lerp(KY + 12, 46, ease(u)) : KY + 12 - 10 * sinf(u * (float)M_PI * 0.9f);
        float ang = (low ? 32 : 25) * ease(seg(t, 800, 966));
        Pose p = pose(EYES_OPEN, u > 0.3f ? ARMS_UP : ARMS_DOWN);
        if (ang > 0) clawd_rotated(c, s.x(kx + 18), cy, s.deg(ang), p);
        else clawd(c, s.sx(kx), iround(cy - 12), p);
        ball(c, s.x(lerp(64, tx, u)), lerp(58, ty, u), 6 - 3 * u);
        return;
    }
    uint32_t u = t - 1100;
    if (low && u < 400) {                        // down on the grass, then back up
        float k = ease(seg(u, 200, 400));
        clawd_rotated(c, s.x(24 + 18), lerp(46, 50, k), s.deg(32 * (1 - k)), pose(EYES_OPEN, ARMS_UP));
        if (!parry) ball(c, s.x(lerp(30, 40, k)), lerp(46, 35, k), 3);
    } else {
        float y = low ? 38 : 30 + fminf(8, u / 22.5f);
        clawd(c, s.sx(24), iround(y), pose(u > 600 ? EYES_WINK : EYES_OPEN, ARMS_UP));
        if (!parry) ball(c, s.x(40), y - 3, 3);
    }
    if (parry) {                                 // ball flies off over the bar
        float k = seg(u, 0, 500);
        if (k < 1) ball(c, s.x(tx - 30 * k), ty - 40 * k, 3);
    }
    if (u % 360 < 270) c.text_center(iround(s.x(78)), 0, "SAVE!", font_bold16);
}

// -------------------------------------------------------------------- header

static void a_header(Canvas& c, uint32_t t, const Ctx& x) {
    Side s = side_of(x.seed);
    uint32_t h = hash32(x.seed + 505);
    bool dive = h & 8;
    const char* word = goal_word(x.seed);
    const int X = 44;
    if (t < 825) {                               // the cross comes in
        float u = t / 825.0f;
        float bx = -6 + 62 * u, by = 6 + 22 * u + 10 * sinf(u * (float)M_PI);
        goal_net(c, s);
        if (!dive) {
            float jump = u > 0.5f ? 10 * sinf((u - 0.5f) * (float)M_PI) : 0;
            clawd(c, s.sx(X), iround(38 - jump), pose(u < 0.6f ? s.look(false) : EYES_OPEN));
        } else {
            // launches forward head first
            float k = ease(seg(t, 450, 825));
            float cx = lerp(X + 18 - 14, X + 18 - 6, k), cy = lerp(50, 38, k);
            Pose p = pose(u < 0.6f ? s.look(false) : EYES_OPEN, ARMS_DOWN, k > 0 ? FEET_NONE : FEET_BOTH);
            if (k > 0) clawd_rotated(c, s.x(cx), cy, s.deg(-35 * k), p);
            else clawd(c, s.sx(X - 14), 38, p);
        }
        ball(c, s.x(bx), by, 3);
        return;
    }
    if (t < 1320) {                              // heads it into the net
        float u = (t - 825) / 495.0f;
        goal_net(c, s);
        if (!dive) {
            clawd(c, s.sx(X), iround(28 + 10 * u), pose(t < 905 ? EYES_CLOSED : EYES_OPEN));
        } else {
            float k = ease(u);
            float cx = X + 18 - 6 + 6 * k, cy = lerp(38, 54, ease_in(u));
            Pose p = pose(t < 905 ? EYES_CLOSED : EYES_OPEN, ARMS_DOWN, FEET_NONE);
            clawd_rotated(c, s.x(cx), cy, s.deg(-35 * (1 - k)), p);
        }
        ball(c, s.x(58 + 62 * u), 24 + 22 * u, 3);
        return;
    }
    uint32_t u = t - 1320;
    if (!dive) {
        celebrate(c, s, X, u, word, 46);
        return;
    }
    // belly flop slide, then arms up on the grass
    goal_net(c, s, u < 270 ? 2 : 0);
    ball_in_net(c, s, 46, u);
    clawd(c, s.sx(X), 47, pose(EYES_OPEN, u > 200 ? ARMS_UP : ARMS_DOWN, FEET_NONE));
    shout(c, word, 64, 1, u);
}

// ------------------------------------------------------------- bicycle kick

static void a_bicycle_kick(Canvas& c, uint32_t t, const Ctx& x) {
    Side s = side_of(x.seed);
    uint32_t h = hash32(x.seed + 606);
    float endy = 44 + 12 * hashf(h);
    const char* word = goal_word(x.seed);
    const int X = 40, Y = 38;
    const float CX = X + 18;
    if (t >= 1335) {
        celebrate(c, s, X, t - 1335, word, endy);
        return;
    }
    goal_net(c, s);
    if (t < 420) {                               // ball drops from the sky
        float u = t / 420.0f;
        clawd(c, s.sx(X), Y, pose(EYES_OPEN));
        ball(c, s.x(CX), -4 + 22 * u * u, 3);
        return;
    }
    if (t < 840) {                               // jump and flip backwards
        uint32_t u = t - 420;
        const Key A[] = {{0, 0}, {240, 180}};
        const Key L[] = {{0, 0}, {240, 20}};
        float ang = KEYS(A, u), lift = KEYS(L, u);
        bool up = ang > 150;
        clawd_rotated(c, s.x(CX), Y + 12 - lift, s.deg(ang), pose(up ? EYES_CLOSED : EYES_OPEN, ARMS_DOWN, up ? FEET_LEFT : FEET_BOTH));
        ball(c, s.x(CX), 18 + 4 * seg(u, 0, 240), 3);
        return;
    }
    {                                            // ball flies in, Clawd comes back down
        float u = (t - 840) / 495.0f;
        float k = ease(u);
        float ang = 180 * (1 - k);
        if (ang < 2) clawd(c, s.sx(X), Y, pose());
        else clawd_rotated(c, s.x(CX), Y + 12 - 20 * (1 - k), s.deg(ang), pose(EYES_OPEN, ARMS_DOWN, FEET_BOTH));
        ball(c, s.x(CX + 62 * u), lerp(22, endy, u), 3);
    }
}

// ------------------------------------------------------------------ dribble

static void a_dribble(Canvas& c, uint32_t t, const Ctx& x) {
    Side s = side_of(x.seed);
    uint32_t h = hash32(x.seed + 707);
    int nc = 2 + h % 3;                          // cones
    bool roulette = (h >> 3) % 3 == 0;           // spins past the middle one
    const uint32_t TOTAL = 3000, SPIN = 13 * 60;
    float cones[4];
    for (int i = 0; i < nc; i++) cones[i] = 128.0f * (i + 1) / (nc + 1);

    // where Clawd is: walks the whole time except during the spin
    uint32_t walk = roulette ? TOTAL - SPIN : TOTAL;
    uint32_t spin_at = walk / 2;
    uint32_t tw = t;
    int k = -1;
    if (roulette && t >= spin_at) {
        if (t < spin_at + SPIN) {
            tw = spin_at;
            k = (t - spin_at) / 60;
        } else tw = t - SPIN;
    }
    float X = -36 + 168 * (tw / (float)walk);
    float bx = X + 38 + 2 * sinf(tw / 50.0f);

    pitch(c);
    for (int i = 0; i < nc; i++) {
        float cx = s.x(cones[i]);
        float p[] = {cx - 3, 62, cx + 3, 62, cx, 54};
        c.fill_poly(p, 3);
    }
    // little hop of the ball over each cone, and of Clawd too
    float hopb = 0, hopc = 0;
    for (int i = 0; i < nc; i++) {
        float d = fabsf(bx - cones[i]);
        if (d < 9) hopb = fmaxf(hopb, 8 * sinf((1 - d / 9) * (float)M_PI / 2));
        float dc = fabsf(X + 18 - cones[i]);
        if (dc < 12) hopc = fmaxf(hopc, 4 * (1 - dc / 12));
    }
    if (k >= 0) {
        clawd_turn(c, s.sx(X), 37, s.m ? TURN_STEPS - 1 - k : k);
        ball(c, s.x(X + 18 + 20 * cosf(k / 12.0f * 2 * (float)M_PI)), 59, 3);
        return;
    }
    clawd(c, s.sx(X), iround(37 - hopc), pose(EYES_OPEN, ARMS_DOWN, (tw / 60) % 2 ? FEET_RIGHT : FEET_LEFT));
    ball(c, s.x(bx), 59 - hopb, 3);
}

// --------------------------------------------------------------- knee slide

static void a_knee_slide(Canvas& c, uint32_t t, const Ctx& x) {
    Side s = side_of(x.seed);
    const char* word = goal_word(x.seed);
    const uint32_t SLIDE = 1260;
    float u = seg(t, 0, SLIDE);
    float X = -30 + 110 * ease_out(u);
    pitch(c);
    bool wink = t > SLIDE + 200 && t < SLIDE + 500;
    clawd(c, s.sx(X), 47, pose(wink ? EYES_WINK : EYES_OPEN, ARMS_UP, FEET_NONE));
    if (u < 0.9f) {                              // grass flying up behind
        int i = t / 30;
        for (int k = 0; k < 5; k++)
            c.px(iround(s.x(X - 4 - k * 5 - (i * 3 + k * 7) % 4)), 60 - (k * 3 + i) % 6);
    }
    if (u > 0.6f) shout(c, word, 64, 6, t);
}

// ------------------------------------------------------------------- trophy

static void trophy(Canvas& c, float x, float y) {
    c.pieslice(x - 8, y - 6, x + 8, y + 10, 0, 180);
    c.fill_rect(iround(x - 8), iround(y - 2), iround(x + 8), iround(y + 2));
    c.arc(x - 13, y - 2, x - 5, y + 6, 90, 270, 2);
    c.arc(x + 5, y - 2, x + 13, y + 6, 270, 90, 2);
    c.fill_rect(iround(x - 1), iround(y + 10), iround(x + 1), iround(y + 14));
    c.fill_rect(iround(x - 6), iround(y + 14), iround(x + 6), iround(y + 16));
}

static void a_trophy(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t h = hash32(x.seed + 808);
    bool confetti = h & 1;
    static const float L[] = {0, -2, -4, -2};
    float lift = pickf(L, 4, t / 90.0f);
    float sway = (h & 2) ? 2 * sinf(t / 260.0f) : 0;
    trophy(c, 64 + sway, 6 + lift);
    clawd(c, 46 + iround(sway * 0.5f), 38 + iround(lift / 2),
          pose(t > 1000 && t < 1300 ? EYES_WINK : EYES_OPEN, ARMS_UP));
    if (confetti) {
        for (int k = 0; k < 14; k++) {
            uint32_t r = hash32(k * 977 + x.seed);
            float sp = 0.02f + 0.02f * hashf(r);
            float y = fmodf((r >> 8) % 64 + t * sp, 70) - 4;
            int px = (r >> 16) % 128 + iround(2 * sinf(t / 200.0f + k));
            if (px > 36 && px < 92 && y > 20) continue;   // not over Clawd
            c.px(px, iround(y));
            c.px(px + 1, iround(y) + ((t / 100 + k) % 2));
        }
    } else {
        static const int A[][2] = {{20, 10}, {108, 14}, {14, 40}, {112, 44}};
        static const int B[][2] = {{26, 20}, {102, 8}, {20, 52}, {106, 34}};
        const int (*p)[2] = (t / 90) % 2 ? A : B;
        for (int k = 0; k < 4; k++) sparkle(c, p[k][0], p[k][1], 2);
    }
}

// ---------------------------------------------------------------- free kick
// a wall of three half size Clawds, the ball curls over them into the top corner


static void a_free_kick(Canvas& c, uint32_t t, const Ctx& x) {
    Side s = side_of(x.seed);
    uint32_t h = hash32(x.seed + 909);
    bool jump = (h & 3) != 0;                    // the wall jumps (most of the time)
    float corner = 34 + 6 * hashf(h >> 4);       // top corner height
    const char* word = goal_word(x.seed);
    const int Y = 38;
    const float BX = 40, BY = 59;
    const uint32_t RUN = 900, HIT = 1000, IN = 1800;

    goal_net(c, s, t > IN && t < IN + 270 ? 2 : 0);
    // the ball
    float bx = BX, by = BY;
    if (t >= HIT && t < IN) {
        float u = (t - HIT) / (float)(IN - HIT);
        bx = lerp(BX, 116, u);
        // up over the wall then dipping down under the bar
        by = lerp(BY, corner, u) - 34 * sinf(u * (float)M_PI * 0.92f);
    }
    // the wall
    for (int i = 0; i < 3; i++) {
        float wy = 50;
        Pose p = pose(EYES_OPEN);
        if (t < HIT) p.eyes = s.look(false);
        else if (t < IN + 100) {
            p.eyes = bx > 56 + i * 18 ? s.look(true) : s.look(false);
            if (jump) {
                float j = seg(t, HIT + 60 + i * 30, HIT + 560 + i * 30);
                wy -= 7 * sinf(j * (float)M_PI);
                if (j > 0 && j < 1) p.arms = ARMS_UP;
            }
        } else p.eyes = EYES_CLOSED;
        // arms linked, a little shuffle while they wait
        clawd(c, s.sx(47 + i * 18, 18), iround(wy - ((t / 300 + i) % 2 && t < RUN ? 1 : 0)), p, 0.5f);
    }
    // the taker
    if (t < 400) clawd(c, s.sx(-4), Y, pose(s.look(true)));
    else if (t < RUN) running(c, s, -4 + 6 * ease(seg(t, 400, RUN)), Y, t, s.look(true), 120);
    else if (t < IN) {
        bool k = t >= HIT && t < HIT + 90;
        clawd(c, s.sx(2), Y, pose(s.look(true), ARMS_DOWN, t < HIT || k ? s.plant() : FEET_BOTH));
        if (k) kick(c, s, 2, Y);
    } else {
        clawd(c, s.sx(2), Y - iround(hop(t - IN)), pose(EYES_OPEN, ARMS_UP));
        shout(c, word, 64, 1, t - IN);
    }
    if (t < IN) ball(c, s.x(bx), by, 3);
    else ball_in_net(c, s, corner, t - IN);
}

// -------------------------------------------------------------- head juggle
// big eyes: a ball bounces on top of the head, the eyes nudge it up each touch

static void a_head_juggle(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t h = hash32(x.seed + 1010);
    int n = 4 + h % 4;
    const uint32_t IN = 450, JUG = 3000, hop = JUG / n, BONK = 1050;
    const float LOW = 7;                           // eyes sit this much lower while juggling

    if (t < IN) {                                  // ball drops in, eyes go down a bit and look up
        float u = t / (float)IN;
        float k = ease(u);
        face(c, eye(0, LOW * k, EYE_W, EYE_H - 4 * k));
        ball(c, 64, -6 + 28 * u * u, 4);
        return;
    }
    if (t < IN + JUG) {
        // touches go eye to eye, the eye that hits it pops up
        uint32_t tt = t - IN;
        int k = tt / hop;
        float u = (tt % hop) / (float)hop;
        int from = k == 0 ? 0 : (hash32(x.seed * 5 + k) & 1 ? 1 : -1);
        int to = hash32(x.seed * 5 + k + 1) & 1 ? 1 : -1;
        float top = 2 + 7 * hashf(x.seed * 3 + k);
        float x0 = 64 + from * 26, x1 = 64 + to * 26;
        float bx = lerp(x0, x1, u);
        float by = 22 - (22 - top) * sinf(u * (float)M_PI);
        float pop = u < 0.18f ? sinf(u / 0.18f * (float)M_PI) : 0;
        float catch_ = u > 0.85f ? (u - 0.85f) / 0.15f : 0;   // next eye gets ready
        float dx = (bx - 64) / 6, dy = LOW - (22 - by) / 7;
        Eye l = eye(dx, dy, EYE_W, EYE_H - 4), r = eye(dx, dy, EYE_W, EYE_H - 4);
        r.mirror = true;
        Eye& hit = from < 0 ? l : r;
        if (from != 0) {
            hit.dy -= 5 * pop;
            hit.h += 2 * pop;
        } else {
            l.dy -= 4 * pop;
            r.dy -= 4 * pop;
        }
        Eye& next = to < 0 ? l : r;
        next.dy += 1.5f * catch_;
        face(c, l, r);
        ball(c, bx, by, 4);
        return;
    }
    uint32_t u = t - IN - JUG;
    if (u < BONK) {
        // one last high one, the eyes are late and it lands on one of them
        float last = (hash32(x.seed * 5 + n) & 1) ? 1 : -1;
        float side = -last;
        float ex = 64 + side * 26;
        if (u < 450) {
            float q = u / 450.0f;
            float bx = lerp(64 + last * 26, ex, q), by = lerp(22, 26, q) - 26 * sinf(q * (float)M_PI);
            float dx = (bx - 64) / 6 * (1 - q);
            Eye l = eye(dx, LOW - (22 - fminf(by, 22)) / 7, EYE_W, EYE_H - 4), r = l;
            r.mirror = true;
            face(c, l, r);
            ball(c, bx, by, 4);
            return;
        }
        float q = (u - 450) / 600.0f;
        Eye hit = eye(0, LOW + 4, EYE_W + 2, EYE_H - 10, SQUINT);
        Eye other = eye(0, LOW, EYE_W, EYE_H - 4);
        if (side > 0) {
            hit.mirror = true;
            face(c, other, hit);
        } else {
            other.mirror = true;
            face(c, hit, other);
        }
        stars(c, ex, 12, q * 2);
        ball(c, ex + side * 40 * q, 26 - 20 * q + 64 * q * q, 4);
        return;
    }
    u -= BONK;
    if (u < 300) {                                 // back up to the normal face
        float k = ease(u / 300.0f);
        face(c, eye(0, LOW * (1 - k), EYE_W, EYE_H - 4 * (1 - k)));
        return;
    }
    u -= 300;
    if (u < BLINK_MS) face(c, blink_eye(Eye(), u / (float)BLINK_MS));
    else face(c);
}

// ----------------------------------------------------------------- crossbar
// shoots, DONK off the bar, hands on head, ball rolls away

static void a_crossbar(Canvas& c, uint32_t t, const Ctx& x) {
    Side s = side_of(x.seed);
    uint32_t h = hash32(x.seed + 1111);
    bool post = (h & 3) == 0;                    // sometimes the post instead
    const char* word = (h & 4) ? "DOINK" : "DONK";
    const int Y = 38;
    const float BX = 54, BY = 59;
    const uint32_t RUN = 630, HIT = 780, BAR = 1230, END = 3600;
    float hx = post ? 104 : 108, hy = post ? 44 : 30;   // contact point

    // the goal shakes after the hit
    float shake = 0;
    if (t >= BAR && t < BAR + 600) shake = 2 * sinf((t - BAR) / 18.0f) * (1 - seg(t, BAR, BAR + 600));
    if (post) goal_net(c, s, 0, iround(s.m ? -shake : shake), 0);
    else goal_net(c, s, 0, 0, iround(shake));

    // ball path
    float bx = BX, by = BY;
    if (t >= HIT && t < BAR) {
        float u = (t - HIT) / (float)(BAR - HIT);
        bx = lerp(BX, hx - 3, u);
        by = lerp(BY, hy + (post ? 0 : 3), u) - (post ? 14 : 18) * sinf(u * (float)M_PI);
    } else if (t >= BAR) {
        // bounces back down and rolls past Clawd, off the screen
        float u = (t - BAR) / 1600.0f;
        bx = hx - 3 - 150 * u;
        float fall = fminf(1, u * 3);
        by = lerp(hy + 3, 59, fall * fall) - (u > 0.33f ? 8 * fabsf(sinf((u - 0.33f) * 3 * (float)M_PI)) * (1 - u) : 0);
    }
    if (bx > -5) ball(c, s.x(bx), by, 3);

    // Clawd
    if (t < RUN) running(c, s, 4 + 20 * (t / (float)RUN), Y, t);
    else if (t < HIT) clawd(c, s.sx(24), Y, pose(EYES_OPEN, ARMS_DOWN, s.plant()));
    else if (t < BAR + 80) {
        bool k = t < HIT + 80;
        clawd(c, s.sx(24), Y, pose(s.look(true), ARMS_DOWN, k ? s.plant() : FEET_BOTH));
        if (k) kick(c, s, 24, Y);
    } else if (t < BAR + 1100) {
        // hands on head, shaking it
        int shake_x = ((t - BAR) / 130) % 2 ? 1 : -1;
        clawd(c, s.sx(24) + shake_x, Y + 1, pose(EYES_CLOSED, ARMS_UP));
    } else {
        // slumps, sweat drop slides down
        clawd(c, s.sx(24), Y + 1, pose(EYES_CLOSED, ARMS_DOWN));
        float k = seg(t, BAR + 1200, END - 400);
        if (k > 0 && k < 1) sweat(c, s.x(64), 32 + 8 * k);
    }
    // the noise
    if (t >= BAR && t < BAR + 900) {
        int wx = iround(s.x(post ? 80 : 98));
        int jitter = (t / 40) % 2;
        c.text_center(wx, 8 + jitter, word, font_bold12);
        // little vibration ticks around the hit point
        if ((t / 60) % 2 == 0) {
            float px = s.x(hx), py = hy;
            c.line(px - 4, py - 4, px - 7, py - 7);
            c.line(px + 4, py - 4, px + 7, py - 7);
        }
    }
}

// ------------------------------------------------------------ rainbow flick
// running with the ball, flicks it over its own head from behind, volleys it in

static void a_rainbow_flick(Canvas& c, uint32_t t, const Ctx& x) {
    Side s = side_of(x.seed);
    uint32_t h = hash32(x.seed + 1212);
    float peak = 2 + 8 * hashf(h);
    const char* word = goal_word(x.seed);
    const int Y = 38;
    const uint32_t RUN = 900, FLICK = 1150, VOLLEY = 1900, IN = 2250;

    goal_net(c, s, t > IN && t < IN + 270 ? 2 : 0);
    // Clawd keeps running: -30 .. 24
    float X;
    if (t < VOLLEY) X = -30 + 54 * (t / (float)VOLLEY);
    else X = 24;
    float bx, by = 59;
    if (t < RUN) bx = X + 38 + 2 * sinf(t / 50.0f);
    else if (t < FLICK) {                        // ball rolls back between the feet
        float u = (t - RUN) / (float)(FLICK - RUN);
        bx = lerp(X + 38, X + 6, ease(u));
    } else if (t < VOLLEY) {                     // up over the head
        float u = (t - FLICK) / (float)(VOLLEY - FLICK);
        bx = lerp(X + 4, X + 38, u);
        by = lerp(57, 50, u) - (57 - peak) * sinf(u * (float)M_PI);
    } else if (t < IN) {                         // volley into the net
        float u = (t - VOLLEY) / (float)(IN - VOLLEY);
        bx = lerp(X + 38, 118, u);
        by = lerp(50, 40, u) - 6 * sinf(u * (float)M_PI);
    }
    if (t < IN) ball(c, s.x(bx), by, 3);
    else ball_in_net(c, s, 40, t - IN);

    if (t < FLICK) running(c, s, X, Y, t, s.look(true), 100);
    else if (t < FLICK + 100) {                  // the back heel
        clawd(c, s.sx(X), Y, pose(EYES_OPEN, ARMS_DOWN, s.m ? FEET_LEFT : FEET_RIGHT));
        float lx = s.x(X + 6), ly = Y + 16;
        c.line(lx, ly, s.x(X + 1), ly - 4, 2);
    } else if (t < VOLLEY) {
        // eyes follow the ball over, arms out for balance
        Eyes e = bx < X + 18 ? s.look(false) : s.look(true);
        running(c, s, X, Y, t, e, 100);
    } else if (t < IN) {
        bool k = t < VOLLEY + 100;
        clawd(c, s.sx(X), Y, pose(s.look(true), ARMS_DOWN, k ? s.plant() : FEET_BOTH));
        if (k) {
            int sx = s.sx(X);
            if (s.m) c.line(sx + 6, Y + 14, sx - 2, Y + 8, 2);
            else c.line(sx + 29, Y + 14, sx + 37, Y + 8, 2);
        }
    } else {
        clawd(c, s.sx(X), Y - iround(hop(t - IN)), pose(EYES_OPEN, ARMS_UP));
        shout(c, word, 64, 1, t - IN);
    }
}

static const Anim A_KEEPY = {"keepy_uppy", a_keepy_uppy, 5130, SCENE};
static const Anim A_GOAL = {"goal", a_goal, 3880, SCENE};
static const Anim A_WATCH = {"ball_watch", a_ball_watch, 4130, CALM};
static const Anim A_PENALTY = {"penalty_save", a_penalty_save, 2200, SCENE};
static const Anim A_HEADER = {"header", a_header, 2130, SCENE};
static const Anim A_BICYCLE = {"bicycle_kick", a_bicycle_kick, 2150, SCENE};
static const Anim A_DRIBBLE = {"dribble", a_dribble, 3000, SCENE};
static const Anim A_SLIDE = {"knee_slide", a_knee_slide, 1900, SCENE};
static const Anim A_TROPHY = {"trophy", a_trophy, 2400, SCENE};
static const Anim A_FREE_KICK = {"free_kick", a_free_kick, 3200, SCENE};
static const Anim A_HEAD_JUGGLE = {"head_juggle", a_head_juggle, 5250, CALM};
static const Anim A_CROSSBAR = {"crossbar", a_crossbar, 3600, SCENE};
static const Anim A_RAINBOW = {"rainbow_flick", a_rainbow_flick, 3300, SCENE};

ANIM_LIST(anims_football, &A_KEEPY, &A_GOAL, &A_WATCH, &A_PENALTY, &A_HEADER, &A_BICYCLE, &A_DRIBBLE,
          &A_SLIDE, &A_TROPHY, &A_FREE_KICK, &A_HEAD_JUGGLE, &A_CROSSBAR, &A_RAINBOW);
