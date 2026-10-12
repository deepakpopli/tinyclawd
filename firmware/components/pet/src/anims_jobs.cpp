// looks for the bigger jobs: builds, tests, git, installs and downloads.
// cheap on purpose: rects, lines, small circles, sprites, a few sinf per frame.
#include <stdio.h>

#include "anim.h"

static const float PI = (float)M_PI;

// ------------------------------------------------------------------- helpers

static float blink_k(uint32_t t, uint32_t at) {
    return t >= at && t < at + BLINK_MS ? (t - at) / (float)BLINK_MS : -1;
}

static Eye maybe_blink(const Eye& e, float k) { return k >= 0 ? blink_eye(e, k) : e; }

static void eyes_at(Canvas& c, float lx, float rx, float y, const Eye& e) {
    Eye r = e;
    r.mirror = true;
    draw_eye(c, lx, y, e);
    draw_eye(c, rx, y, r);
}

// height of a little hop, u ms into it, 300 ms per hop
static float hop(uint32_t u, float h = 4) {
    float k = (u % 300) / 300.0f;
    return h * 4 * k * (1 - k);
}

// ---------------------------------------------------------------------- crane
// a tower crane lifts floors onto a building, Clawd drives it from the cab

static const uint32_t CR_LIFT = 2650, CR_FLOORS = 5, CR_DONE = 1800;
static const uint32_t CR_CYC = CR_LIFT * CR_FLOORS + CR_DONE;   // 15050
static const int CR_PICK = 71, CR_BLD = 107, CR_FW = 26, CR_FH = 7, CR_UP = 14;

// one floor of the building: a slab with windows, the ground floor gets a door
static void cr_floor(Canvas& c, int cx, int top, uint32_t key, bool ground) {
    int x0 = cx - CR_FW / 2;
    c.fill_rect(x0, top, x0 + CR_FW - 1, top + CR_FH - 2);
    uint32_t h = hash32(key);
    for (int i = 0; i < 4; i++) {
        int wx = x0 + 3 + i * 6;
        if (ground && i == 2) {
            c.fill_rect(wx, top + 2, wx + 3, top + CR_FH - 2, false);
            continue;
        }
        if ((h >> i) & 1 || i == 0) c.fill_rect(wx, top + 2, wx + 2, top + 3, false);
        else c.px(wx + 1, top + 3, false);
    }
}

static void a_crane(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cyc = t / CR_CYC, in = t % CR_CYC;
    uint32_t seed = cyc * 3301 + x.seed;
    bool done = in >= CR_LIFT * CR_FLOORS;
    int lift = done ? CR_FLOORS : in / CR_LIFT;
    uint32_t u = done ? 0 : in % CR_LIFT;

    // trolley x, hook y, and whether a floor hangs from it
    float tx = CR_PICK, hy = CR_UP, sway = 0;
    bool carry = false;
    int stack = lift;   // floors already standing
    float land = 63 - (lift + 1) * CR_FH + 1;   // top of the floor being placed
    if (!done) {
        if (u < 450) hy = lerp(CR_UP, 56, ease(seg(u, 0, 450)));
        else if (u < 550) hy = 56;
        else if (u < 1000) hy = lerp(56, CR_UP, ease(seg(u, 550, 1000))), carry = true;
        else if (u < 1450) tx = lerp(CR_PICK, CR_BLD, ease(seg(u, 1000, 1450))), hy = CR_UP, carry = true;
        else if (u < 1850) {
            tx = CR_BLD;
            hy = lerp(CR_UP, land, ease(seg(u, 1450, 1850)));
            carry = true;
            float k = seg(u, 1450, 1850);
            sway = 3 * (1 - k) * sinf(k * 3 * PI);   // swings a bit after the ride
        } else if (u < 1950) tx = CR_BLD, hy = land;
        else if (u < 2200) tx = CR_BLD, hy = lerp(land, CR_UP, ease(seg(u, 1950, 2200)));
        else tx = lerp(CR_BLD, CR_PICK, ease(seg(u, 2200, CR_LIFT)));
        if (u >= 1850) stack = lift + 1;
    }
    // when it is finished the building sinks away for the next one
    int sink = done ? iround(40 * ease_in(seg(in - CR_LIFT * CR_FLOORS, 1300, CR_DONE))) : 0;

    // ground and the building
    c.hline(0, 127, 63);
    for (int f = 0; f < stack; f++) cr_floor(c, CR_BLD, 63 - (f + 1) * CR_FH + 1 + sink, seed + f * 7, f == 0);
    if (done && sink < 30) {   // a flag on the roof
        int ry = 63 - (int)CR_FLOORS * CR_FH + sink;
        c.vline(CR_BLD, ry - 9, ry);
        c.fill_rect(CR_BLD + 1, ry - 9, CR_BLD + 6, ry - 6);
    }
    c.fill_rect(CR_BLD - 18, 63, CR_BLD + 18, 63, false);
    c.hline(0, 127, 63);

    // next floor waits at the pick up spot, it pops up out of the ground
    bool waiting = !done && (u < 550 || (u >= 1450 && lift + 1 < (int)CR_FLOORS));
    if (waiting) {
        int top = 56;
        if (u >= 1450) top = iround(lerp(64, 56, ease_out(seg(u, 1450, 1700))));
        cr_floor(c, CR_PICK, top, seed + (lift + (u >= 1450 ? 1 : 0)) * 7, false);
        c.fill_rect(CR_PICK - 14, 63, CR_PICK + 14, 63, false);
        c.hline(0, 127, 63);
    }

    // tower: mast, jib, tie lines
    c.vline(8, 4, 62);
    c.vline(14, 4, 62);
    for (int y = 8; y < 62; y += 6) c.line(8, y, 14, y + 6);
    c.hline(2, 124, 4);
    c.hline(2, 124, 8);
    for (int xx = 16; xx < 124; xx += 6) c.line(xx, 8, xx + 6, 4);
    c.line(11, 0, 2, 4);
    c.line(11, 0, 70, 4);
    c.fill_rect(2, 9, 7, 14);   // counterweight

    // trolley, cable, hook, the hanging floor
    int ti = iround(tx);
    c.fill_rect(ti - 3, 9, ti + 3, 11);
    float hx = tx + sway;
    c.line(tx, 12, hx, hy);
    c.vline(iround(hx), iround(hy), iround(hy) + 2);
    c.px(iround(hx) + 1, iround(hy) + 2);
    if (carry) {
        c.line(hx, hy + 1, hx - 10, hy + 6);
        c.line(hx, hy + 1, hx + 10, hy + 6);
        int top = iround(hy) + 7;
        if (u >= 450 && u < 1000) top = iround(hy) + (u < 550 ? 0 : iround(7 * seg(u, 550, 650)));
        cr_floor(c, iround(hx), top, seed + lift * 7, lift == 0);
    }

    // a little cab, Clawd works the crane from the ground with a remote
    c.rect(15, 9, 25, 17);
    c.fill_rect(17, 11, 23, 13);
    uint32_t q = done ? in - CR_LIFT * CR_FLOORS : 0;
    Pose p = pose(EYES_RIGHT, ARMS_ONE_UP);
    int cy = 39;
    if (done && q < 1000) {
        p = pose(EYES_CLOSED, ARMS_UP);
        cy -= iround(hop(q, 5));
    } else if (!done && u >= 1850 && u < 2100) {
        p.eyes = EYES_CLOSED;   // a happy squint, it fits
    } else if (!done && hash32(seed + lift) % 3 == 0 && u >= 2300 && u < 2300 + BLINK_MS) {
        p.eyes = EYES_CLOSED;
    }
    clawd(c, 17, cy, p);
    if (p.arms == ARMS_ONE_UP) {   // the remote, with its antenna
        bool press = !done && ((u >= 450 && u < 600) || (u >= 1850 && u < 1950));
        int ry = cy - 5 + press;
        c.rect(47, ry, 53, ry + 7);
        c.fill_rect(49, ry + 2, 51, ry + 4);
        c.vline(52, ry - 5, ry - 1);
        c.fill_rect(52, ry - 6, 53, ry - 5);
        if (press) {   // signal going up to the crane
            c.line(56, ry - 6, 58, ry - 8);
            c.line(58, ry - 3, 61, ry - 6);
        }
    }
    if (done && q < 1200) {
        bool alt = (q / 150) % 2;
        sparkle(c, CR_BLD - 19, alt ? 22 : 24, 3);
        sparkle(c, CR_BLD + 18, alt ? 30 : 28, 2);
    }
}

// ----------------------------------------------------------------- test tubes
// test tubes bubble one after another, a lamp over each one says pass or fail.
// a failed one gets run again and passes, then they all light up

static const uint32_t TT_RUN = 1200, TT_N = 4, TT_FIX = 1500, TT_YAY = 1300, TT_DRAIN = 600;
static const uint32_t TT_CYC = TT_RUN * TT_N + TT_FIX + TT_YAY + TT_DRAIN;   // 8200
static const int TT_TOP = 22, TT_BOT = 56, TT_LO = 46, TT_HI = 30;

static int tt_x(int i) { return 68 + i * 16; }

static void tt_tube(Canvas& c, int x) {
    c.hline(x - 5, x + 5, TT_TOP);
    c.vline(x - 4, TT_TOP, TT_BOT - 2);
    c.vline(x + 4, TT_TOP, TT_BOT - 2);
    c.px(x - 3, TT_BOT - 1);
    c.px(x + 3, TT_BOT - 1);
    c.hline(x - 2, x + 2, TT_BOT);
}

// liquid up to level, with a little wobbly surface when it boils
static void tt_liquid(Canvas& c, int x, int level, bool boil, uint32_t t) {
    c.fill_rect(x - 2, level, x + 2, TT_BOT - 1);
    if (boil) {
        int k = (t / 90) % 3;
        c.px(x - 2 + k, level - 1);
        c.px(x + 2 - k, level - 1);
    }
}

static void tt_bubbles(Canvas& c, int x, int level, uint32_t t, uint32_t key) {
    for (int i = 0; i < 3; i++) {
        uint32_t per = 500 + hash32(key + i) % 300;
        float k = ((t + i * 230) % per) / (float)per;
        int bx = x - 1 + (int)(hash32(key + i * 9 + (t + i * 230) / per) % 3);
        int by = iround(lerp(TT_BOT - 2, level - 9, k));
        if (by >= level) c.px(bx, by, false);   // inside the liquid: a dark dot
        else if (by > TT_TOP - 8) c.rect(bx - 1, by - 1, bx, by);   // out of the mouth: a puff
    }
}

static void a_test_tubes(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cyc = t / TT_CYC, in = t % TT_CYC;
    uint32_t h = hash32(cyc * 811 + x.seed);
    int bad = (h % 3 == 0) ? -1 : (int)((h >> 4) % TT_N);   // the one that fails, or none
    uint32_t fix_at = TT_RUN * TT_N, yay_at = fix_at + TT_FIX, drain_at = yay_at + TT_YAY;
    int cur = in < fix_at ? in / TT_RUN : -1;
    uint32_t u = in % TT_RUN;
    bool fixing = in >= fix_at && in < yay_at && bad >= 0;
    uint32_t fu = in - fix_at;

    // rack behind the tubes
    c.fill_rect(58, 40, 125, 41);
    c.vline(59, 40, 63);
    c.vline(124, 40, 63);
    for (int i = 0; i < (int)TT_N; i++) {
        int tx = tt_x(i);
        c.fill_rect(tx - 3, 40, tx + 3, 41, false);
        tt_tube(c, tx);
        // liquid level: low, boils up while running, stays up after
        float lv = TT_LO;
        bool boil = false;
        if (cur == i) lv = lerp(TT_LO, TT_HI, ease(seg(u, 0, 900))), boil = true;
        else if (cur > i || in >= fix_at) lv = TT_HI;
        if (fixing && i == bad) {   // runs again: drops and boils back up
            lv = fu < 300 ? lerp(TT_HI, TT_LO, ease(seg(fu, 0, 300))) : lerp(TT_LO, TT_HI, ease(seg(fu, 300, 1200)));
            boil = fu >= 300 && fu < 1300;
        }
        if (in >= drain_at) lv = lerp(TT_HI, TT_LO, ease(seg(in, drain_at, TT_CYC)));
        tt_liquid(c, tx, iround(lv), boil, t);
        if (boil) tt_bubbles(c, tx, iround(lv), t, h + i * 77);

        // the lamp: empty, pass (solid), fail (x)
        bool ran = cur > i || (cur == i && u >= 950) || in >= fix_at;
        bool fail = i == bad && ran && !(in >= fix_at + 1300 && bad >= 0);
        bool lit = ran && !fail;
        if (in >= drain_at) lit = false;
        if (in >= yay_at && in < drain_at) lit = ((in - yay_at) / 160) % 2 == 0;
        c.ellipse(tx - 4, 6, tx + 4, 14);
        if (lit) c.fill_circle(tx, 10, 3);
        if (fail && !(fixing && fu < 300 && (fu / 100) % 2)) {
            c.line(tx - 2, 8, tx + 2, 12);
            c.line(tx - 2, 12, tx + 2, 8);
        }
    }
    c.hline(0, 127, 63);

    // eyes on the left watch the tube that is running
    Eye e;
    e.w = 10;
    e.h = 18;
    int look = cur >= 0 ? cur : (fixing ? bad : -1);
    if (look >= 0) {
        e.dx = 2 + look * 1.5f;
        e.dy = -2;
    }
    if (cur == bad && cur >= 0 && u >= 950) {   // uh oh
        e.h = 12;
        e.dy = 2;
    }
    if (bad >= 0 && in >= TT_RUN * (bad + 1) && in < fix_at) e.lid = 0.3f;   // still a bit worried
    if (fixing && fu >= 1300) e.shape = HAPPY, e.w = 9;
    if (in >= yay_at && in < drain_at) {
        e.shape = HAPPY;
        e.w = 9;
        e.dy = -2 - 2 * fabsf(sinf(2 * PI * seg(in, yay_at, yay_at + 700)));
    }
    if (e.shape == RECT && cur >= 0 && hash32(h + cur) % 3 == 0) e = maybe_blink(e, blink_k(u, 400));
    eyes_at(c, 16, 40, 31, e);
    if (bad >= 0 && in >= TT_RUN * (bad + 1) - 250 && in < TT_RUN * (bad + 1) + 900)
        sweat(c, 50, 10 + 8 * seg(in, TT_RUN * (bad + 1) - 250, TT_RUN * (bad + 1) + 900));
}

static const Anim A_CRANE = {"crane", a_crane, CR_CYC, LOOP};
static const Anim A_TUBES = {"test_tubes", a_test_tubes, 2 * TT_CYC, LOOP};

ANIM_LIST(anims_jobs, &A_CRANE, &A_TUBES);
