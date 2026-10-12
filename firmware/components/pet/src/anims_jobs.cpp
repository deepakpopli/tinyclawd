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

// -------------------------------------------------------------- checklist run
// a test run: names with dots growing, a tick at the end. one fails with an x,
// Clawd sweats, it runs again and passes. then the summary and a hop

static void tick(Canvas& c, int x, int y) {
    float p[] = {(float)x, (float)y + 3, (float)x + 2, (float)y + 5, (float)x + 6, (float)y - 1};
    c.polyline(p, 3, 2);
}

static void cross(Canvas& c, int x, int y) {
    c.line(x, y, x + 5, y + 5, 2);
    c.line(x, y + 5, x + 5, y, 2);
}

static const uint32_t CL_ROW = 700, CL_N = 8, CL_RETRY = 1000, CL_SUM = 1600, CL_OUT = 500;
static const uint32_t CL_CYC = CL_ROW * CL_N + CL_RETRY + CL_SUM + CL_OUT;   // 8700
static const int CL_PITCH = 9, CL_VIS = 6, CL_DOTS = 70;
static const char* const CL_NAMES[] = {"parse", "render", "blink", "touch", "wifi", "usb", "fonts", "menu",
                                       "games", "sleep", "eyes", "anims", "proto", "brain", "clock", "keys"};

static void a_checklist_run(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cyc = t / CL_CYC, in = t % CL_CYC;
    uint32_t h = hash32(cyc * 2111 + x.seed);
    int bad = 1 + h % (CL_N - 2);
    // start time of row i: the bad one takes CL_RETRY longer
    uint32_t start[CL_N + 1];
    for (int i = 0; i <= (int)CL_N; i++) start[i] = i * CL_ROW + (i > bad ? CL_RETRY : 0);
    uint32_t sum_at = start[CL_N], out_at = sum_at + CL_SUM;
    int cur = 0;
    while (cur < (int)CL_N && in >= start[cur + 1]) cur++;
    int shown = cur + 1 > (int)CL_N ? CL_N : cur + 1;

    // scroll so the newest row stays on screen, eased when a new one shows up
    float scroll = 0;
    int extra = shown - CL_VIS;
    if (extra > 0) {
        scroll = (extra - 1 + ease(seg(in, start[shown - 1], start[shown - 1] + 200))) * CL_PITCH;
    }
    if (in >= out_at) scroll += 70 * ease_in(seg(in, out_at, CL_CYC));

    for (int i = 0; i < shown; i++) {
        int y = 2 + i * CL_PITCH - iround(scroll);
        if (y < -8 || y > 63) continue;
        const char* name = CL_NAMES[((h >> 3) + i * (1 + 2 * ((h >> 8) % 4))) % 16];   // odd step, no repeats
        int nx = c.text(2, y, name, font_4x6);
        uint32_t u = in - start[i];
        // dots grow along, then the mark
        float k = seg(u, 0, CL_ROW - 150);
        bool failed = i == bad && u >= CL_ROW - 150 && u < CL_ROW + CL_RETRY - 150;
        if (i == bad && u >= CL_ROW + 400) k = seg(u, CL_ROW + 400, CL_ROW + CL_RETRY - 150);   // second go
        int dx1 = nx + 2 + iround((CL_DOTS - nx - 2) * k);
        for (int xx = nx + 2; xx < dx1; xx += 2) c.px(xx, y + 4);
        if (failed) {
            if (u < CL_ROW + 400 || ((u / 100) % 2)) cross(c, 74, y);
        } else if (u >= CL_ROW - 150) {
            tick(c, 73, y + 1);
        }
    }
    // the summary bar at the bottom
    if (in >= sum_at && in < out_at + 200) {
        c.fill_rect(0, 54, 86, 63);
        char s[16];
        snprintf(s, sizeof(s), "%d passed", (int)CL_N);
        c.text(4, 56, s, font_4x6, false);
    }

    // Clawd on the right with the count over his head
    char n[24];
    int passed = 0;
    for (int i = 0; i < shown; i++)
        if (in - start[i] >= CL_ROW - 150 && !(i == bad && in - start[i] < CL_ROW + CL_RETRY - 150)) passed++;
    snprintf(n, sizeof(n), "%d/%d", passed, (int)CL_N);
    c.text_center(109, 6, n, font_5x7);
    Pose p = pose(EYES_LEFT);
    int cy = 38;
    uint32_t fail_at = start[bad] + CL_ROW - 150;
    if (in >= fail_at && in < fail_at + 450) {
        p.eyes = EYES_CLOSED;
        sweat(c, 122, 22 + 8 * seg(in, fail_at, fail_at + 450));
    } else if (in >= fail_at + 450 && in < start[bad + 1]) {
        p.arms = ARMS_ONE_UP;   // fixing it
        cy += ((in / 120) % 2);
    }
    if (in >= sum_at && in < out_at) {
        p = pose(EYES_CLOSED, ARMS_UP);
        if (in < sum_at + 900) cy -= iround(hop(in - sum_at, 5));
        if (((in - sum_at) / 150) % 2) sparkle(c, 96, 22, 2);
        else sparkle(c, 123, 24, 2);
    }
    clawd(c, 91, cy, p);
}

// ------------------------------------------------------------------ git graph
// a git log graph scrolling up: branches split off, get commits, merge back.
// the newest commit pops in at the bottom, Clawd cheers the merges

static const uint32_t GG_ROW = 800, GG_P = 18;
static const int GG_PITCH = 9, GG_LANE = 10, GG_X0 = 6, GG_BOT = 56;

struct GRow {
    uint8_t before, after;   // lanes alive (bit per lane) going in and out of the row
    uint8_t lane;            // where the dot is
    uint8_t kind;            // 0 commit, 1 branch off, 2 merge
    uint8_t from;            // merge: lane it came from
};

static void gg_rows(uint32_t seed, GRow* r) {
    uint8_t m = 1;
    int age[3] = {0, 0, 0};   // commits on each branch so far
    for (int i = 0; i < (int)GG_P; i++) {
        uint32_t h = hash32(i * 733 + seed);
        GRow g;
        g.before = m;
        g.kind = 0;
        g.lane = 0;
        g.from = 0;
        int open = (m & 2 ? 1 : 0) + (m & 4 ? 1 : 0);
        int ripe = (m & 4) && age[2] >= 2 ? 2 : ((m & 2) && age[1] >= 2 ? 1 : 0);
        bool late = i >= (int)GG_P - 3;   // close everything so it loops clean
        if (late && open) {
            g.kind = 2;
            g.from = m & 4 ? 2 : 1;
        } else if (!late && i > 0 && open < 2 && h % 3 == 0) {
            g.kind = 1;
            g.lane = m & 2 ? 2 : 1;
            age[g.lane] = 1;
        } else if (!late && ripe && h % 3 == 1) {
            g.kind = 2;
            g.from = ripe;
        } else {
            // most commits land on a branch when there is one
            int lanes[3], n = 0;
            for (int l = 1; l < 3; l++)
                if ((m >> l) & 1) lanes[n++] = l;
            if (n == 0 || (h >> 8) % 4 == 0) lanes[n++] = 0;
            g.lane = lanes[(h >> 12) % n];
            age[g.lane]++;
        }
        if (g.kind == 1) m |= 1 << g.lane;
        if (g.kind == 2) m &= ~(1 << g.from);
        g.after = m;
        r[i] = g;
    }
}

static void a_git_graph(Canvas& c, uint32_t t, const Ctx& x) {
    GRow rows[GG_P];
    gg_rows(x.seed, rows);
    uint32_t n = t / GG_ROW, u = t % GG_ROW;   // n = newest row
    float slide = 1 - ease(seg(u, 0, 220));    // everything moves up one row as it arrives

    for (int back = 0; back < 8; back++) {
        int idx = (((int)n - back) % (int)GG_P + GG_P) % GG_P;
        const GRow& g = rows[idx];
        int y = GG_BOT - back * GG_PITCH + iround(slide * GG_PITCH);
        int yp = y - GG_PITCH;   // the row above
        if (y < -GG_PITCH) break;
        // lines from the row above into this one
        for (int l = 0; l < 3; l++) {
            int lx = GG_X0 + l * GG_LANE;
            if ((g.before >> l) & 1) {
                if (g.kind == 2 && l == g.from) c.line(lx, yp, GG_X0, y);
                else c.vline(lx, yp, y);
            }
        }
        if (g.kind == 1) c.line(GG_X0, yp, GG_X0 + g.lane * GG_LANE, y);
        // the dot
        int dx = GG_X0 + (g.kind == 2 ? 0 : g.lane * GG_LANE);
        bool fresh = back == 0;
        if (g.kind == 2) {
            int r = fresh && u < 150 ? 4 : 3;
            c.fill_circle(dx, y, r);
            c.fill_circle(dx, y, r - 1.5f, false);
        } else {
            c.fill_circle(dx, y, fresh && u < 150 ? 3 : 2);
        }
        // hash and a message, the new one types in
        char hs[8];
        uint32_t hh = hash32(idx * 97 + x.seed);
        for (int i = 0; i < 7; i++) hs[i] = "0123456789abcdef"[(hh >> (i * 4)) & 15];
        hs[7] = 0;
        int chars = fresh ? (int)clampf((u - 150) / 30.0f, 0, 7) : 7;
        if (fresh && u < 150) chars = 0;
        hs[chars] = 0;
        int tx = c.text(36, y - 3, hs, font_4x6);
        if (chars == 7) {
            int len = 6 + hash32(hh) % 14;
            if (fresh) len = iround(len * seg(u, 360, 560));
            if (len > 0) c.hline(tx + 3, tx + 3 + len, y);
        }
        if (fresh && g.kind == 2 && u >= 100 && u < 450) {   // merge pops
            int d = 5 + iround(5 * ease_out(seg(u, 100, 450)));
            c.px(dx + d, y);
            c.px(dx, y - d);
            c.px(dx, y + d);
            c.px(dx + d - 2, y - d + 2);
            c.px(dx + d - 2, y + d - 2);
        }
    }
    c.fill_rect(88, 0, 127, 63, false);

    // Clawd watches the bottom, branches make him look, merges make him hop
    const GRow& g = rows[n % GG_P];
    Pose p = pose(EYES_LEFT);
    int cy = 38;
    if (g.kind == 2 && u < 900) {
        p = pose(EYES_CLOSED, ARMS_UP);
        cy -= iround(hop(u, 4));
    } else if (g.kind == 1 && u < 500) {
        p.arms = ARMS_ONE_UP;
    } else if (hash32(n + x.seed) % 4 == 0 && u >= 400 && u < 400 + BLINK_MS) {
        p.eyes = EYES_CLOSED;
    }
    clawd(c, 91, cy, p);
    const char* tag = g.kind == 2 ? "merge" : (g.kind == 1 ? "branch" : "commit");
    c.text_center(109, 10, tag, font_4x6);
}

// -------------------------------------------------------------------- package
// little boxes drop into a big one, the flaps close, Clawd tapes it,
// a label gets stamped on and it slides away

static const uint32_t PK_IN = 500, PK_ITEM = 600, PK_ITEMS = 4, PK_CLOSE = 400, PK_TAPE = 700;
static const uint32_t PK_STAMP = 500, PK_YAY = 700, PK_OUT = 600;
static const uint32_t PK_CYC = PK_IN + PK_ITEM * PK_ITEMS + PK_CLOSE + PK_TAPE + PK_STAMP + PK_YAY + PK_OUT;   // 5800
static const int PK_X0 = 54, PK_X1 = 100, PK_TOP = 38;
static const char* const PK_LABELS[] = {"npm", "pip", "apt", "cargo", "brew", "uv"};

// things that go in the box: a small box, a code tile, a ball
static void pk_small(Canvas& c, int x, int y, int kind) {
    if (kind == 0) {
        c.rect(x, y, x + 9, y + 8);
        c.hline(x, x + 9, y + 2);
        c.vline(x + 4, y, y + 2);
    } else if (kind == 1) {
        c.fill_rect(x, y, x + 11, y + 7);
        c.text(x + 1, y + 1, "{}", font_4x6, false);
    } else {
        c.fill_circle(x + 4, y + 4, 4);
        c.fill_circle(x + 4, y + 4, 2, false);
    }
}

static void a_package(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cyc = t / PK_CYC, in = t % PK_CYC;
    uint32_t h = hash32(cyc * 1409 + x.seed);
    uint32_t items_at = PK_IN, close_at = items_at + PK_ITEM * PK_ITEMS, tape_at = close_at + PK_CLOSE;
    uint32_t stamp_at = tape_at + PK_TAPE, yay_at = stamp_at + PK_STAMP, out_at = yay_at + PK_YAY;

    // the big box drops in, and slides out at the end
    int ox = 0, oy = 0;
    if (in < PK_IN) oy = iround(-50 * (1 - ease_in(seg(in, 0, 350))));
    if (in >= out_at) ox = iround(80 * ease_in(seg(in, out_at, PK_CYC)));
    int bump = 0;

    // little boxes falling in
    for (int i = 0; i < (int)PK_ITEMS; i++) {
        uint32_t at = items_at + i * PK_ITEM;
        if (in < at || in >= at + 500) continue;
        uint32_t q = in - at;
        int kind = hash32(h + i) % 3;
        int bx = PK_X0 + 8 + (hash32(h + i * 3) % 24);
        int by = iround(lerp(-10, PK_TOP + 6, ease_in(seg(q, 0, 380))));
        if (q < 380) pk_small(c, bx, by, kind);
        else if (q < 460) bump = 1;
    }

    // flaps: hinge at the top corners, swing from open (out and up) to shut
    float shut = ease(seg(in, close_at, close_at + PK_CLOSE));
    if (in >= out_at) shut = 1;
    float a = lerp(-2.2f, 0, shut);   // left flap angle, the right one is mirrored
    float fl = (PK_X1 - PK_X0) / 2.0f;
    float ca = cosf(a), sa = sinf(a);
    int top = PK_TOP + oy + bump;
    c.line(PK_X0 + ox, top, PK_X0 + ox - fl * ca * -1, top + fl * sa, 2);
    c.line(PK_X1 + ox, top, PK_X1 + ox - fl * ca, top + fl * sa, 2);

    // box body
    int bot = 62 + oy;
    c.fill_rect(PK_X0 + ox, top + 1, PK_X1 + ox, bot, false);
    c.rect(PK_X0 + ox, top, PK_X1 + ox, bot);
    c.rect(PK_X0 + ox + 1, top + 1, PK_X1 + ox - 1, bot - 1);
    int mid = (PK_X0 + PK_X1) / 2 + ox;

    // tape: along the top from left to right, then down the front a bit
    float tk = seg(in, tape_at, tape_at + PK_TAPE - 150);
    if (in >= tape_at) {
        int tx = iround(lerp(PK_X0 - 2, PK_X1 + 2, tk)) + ox;
        c.fill_rect(PK_X0 + ox - 2, top - 1, tx, top + 2);
        c.hline(PK_X0 + ox - 2, tx, top + 1, false);
        if (in < stamp_at) {   // the tape roll on a stick Clawd holds
            c.line(45, PK_TOP, tx, top - 3);
            c.fill_circle(tx, top - 3, 4);
            c.fill_circle(tx, top - 3, 1.5f, false);
        }
    }
    // the label gets stamped on
    if (in >= stamp_at) {
        const char* label = PK_LABELS[h % 6];
        uint32_t q = in - stamp_at;
        if (q < 260) {   // the stamp: comes down, presses, goes up
            int sy = top + 8 - iround(30 * (1 - ease_in(seg(q, 0, 150))));
            if (q >= 190) sy = top + 8 - iround(30 * ease_in(seg(q, 190, 260)));
            if (q >= 150) {
                c.fill_rect(mid - 15, top + 8, mid + 15, top + 18);
                c.text_center(mid, top + 10, label, font_5x7, false);
            }
            c.fill_rect(mid - 15, sy - 4, mid + 15, sy, false);
            c.fill_rect(mid - 14, sy - 3, mid + 14, sy);
            c.fill_rect(mid - 2, sy - 12, mid + 2, sy - 4);
            c.fill_rect(mid - 5, sy - 16, mid + 5, sy - 12);
        } else {
            c.fill_rect(mid - 15, top + 8, mid + 15, top + 18);
            c.text_center(mid, top + 10, label, font_5x7, false);
        }
    } else {   // "this way up" arrows so it reads as a box
        c.vline(mid - 4, top + 10, top + 16);
        c.vline(mid + 4, top + 10, top + 16);
        c.line(mid - 6, top + 12, mid - 4, top + 10);
        c.line(mid - 2, top + 12, mid - 4, top + 10);
        c.line(mid + 2, top + 12, mid + 4, top + 10);
        c.line(mid + 6, top + 12, mid + 4, top + 10);
    }
    c.hline(0, 127, 63);

    // Clawd: watches things fall in, pulls the tape, cheers
    Pose p = pose(EYES_RIGHT);
    int cy = 39;
    if (in >= items_at && in < close_at) {
        uint32_t q = (in - items_at) % PK_ITEM;
        if (q >= 380 && q < 380 + BLINK_MS && (hash32(h + (in - items_at) / PK_ITEM) & 1)) p.eyes = EYES_CLOSED;
    }
    if (in >= tape_at && in < stamp_at) {
        p.arms = ARMS_ONE_UP;
        cy += (in / 110) % 2;
    }
    if (in >= yay_at && in < out_at) {
        p = pose(EYES_CLOSED, ARMS_UP);
        cy -= iround(hop(in - yay_at, 5));
    }
    if (in >= out_at) p.arms = ARMS_UP;   // waves it goodbye
    clawd(c, 10, cy, p);
}

// ------------------------------------------------------------------- download
// bits rain from a cloud into a folder, a bar fills up on the side.
// sometimes it stalls for a bit. at 100% the folder gets a tick

static const uint32_t DL_RUN = 5000, DL_DONE = 1400, DL_RESET = 500;
static const uint32_t DL_CYC = DL_RUN + DL_DONE + DL_RESET;   // 6900
static const int DL_CX = 66, DL_FY = 45;

static void dl_cloud(Canvas& c) {
    c.fill_circle(58, 9, 4);
    c.fill_circle(66, 6, 4);
    c.fill_circle(74, 9, 4);
    c.fill_rect(54, 9, 78, 13);
    c.hline(56, 76, 11, false);   // a little shine line
}

static void a_download(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cyc = t / DL_CYC, in = t % DL_CYC;
    uint32_t h = hash32(cyc * 919 + x.seed);
    // uneven progress, with a stall in the middle on some runs
    float a = 15 + (h & 15), b = a + 20 + ((h >> 4) & 15), d = b + 15 + ((h >> 8) & 15);
    bool stall = (h >> 12) % 2;
    Key k[] = {{0, 0}, {1200, a}, {2300, b}, {3200, stall ? b : d}, {3800, d}, {DL_RUN, 100}};
    float p = in < DL_RUN ? KEYS(k, in) : 100;
    if (in >= DL_RUN + DL_DONE) p = 100 * (1 - ease(seg(in, DL_RUN + DL_DONE, DL_CYC)));
    bool stalled = stall && in >= 2300 && in < 3200;
    bool done = in >= DL_RUN && in < DL_RUN + DL_DONE;
    bool running = in < DL_RUN && !stalled;

    dl_cloud(c);

    // bits falling in three lanes: little squares and dashes
    if (in < DL_RUN + 300) {
        for (int i = 0; i < 6; i++) {
            uint32_t per = 560 + (i % 3) * 80;
            uint32_t tt = t + (i / 3) * (per / 2) + (i % 3) * 170;
            uint32_t drop = tt / per;
            float f = (tt % per) / (float)per;
            uint32_t born = in - (uint32_t)(f * per);   // when this bit left the cloud
            if (born > in) continue;
            if (born >= DL_RUN || (stall && born >= 2300 && born < 3200)) continue;
            int lx = DL_CX - 9 + (i % 3) * 9;
            int ly = iround(lerp(15, DL_FY + 2, ease_in(f)));
            if (hash32(drop * 13 + i + h) % 2) c.fill_rect(lx - 1, ly - 1, lx + 1, ly + 1);
            else c.fill_rect(lx, ly - 4, lx + 1, ly);
        }
    }

    // folder: tab, back, front with an arrow (a tick when done)
    int bump = running && (t / 110) % 4 == 0 ? 1 : 0;
    int fy = DL_FY + bump;
    c.fill_rect(DL_CX - 20, fy - 3, DL_CX - 8, fy);
    c.fill_rect(DL_CX - 20, fy, DL_CX + 20, 62);
    c.fill_rect(DL_CX - 18, fy + 3, DL_CX + 18, 60, false);
    if (done) {
        float tk[] = {DL_CX - 6.0f, fy + 9.0f, DL_CX - 1.0f, fy + 14.0f, DL_CX + 7.0f, fy + 5.0f};
        c.polyline(tk, 3, 2);
    } else {
        int ay = fy + 5 + (running ? iround(2 * seg(t % 600, 0, 600)) : 0);
        c.fill_rect(DL_CX - 1, ay, DL_CX + 1, ay + 4);
        float ar[] = {DL_CX - 5.0f, ay + 5.0f, DL_CX + 5.0f, ay + 5.0f, (float)DL_CX, ay + 10.0f};
        c.fill_poly(ar, 3);
    }
    c.hline(0, 127, 63);

    // the bar on the right, fills from the bottom
    const int BX0 = 104, BX1 = 114, BY0 = 4, BY1 = 50;
    c.rect(BX0, BY0, BX1, BY1);
    int fill = iround((BY1 - BY0 - 3) * p / 100);
    for (int yy = BY1 - 2; yy > BY1 - 2 - fill; yy--) {
        if (done || (yy + t / 80) % 4 != 0) c.hline(BX0 + 2, BX1 - 2, yy);
    }
    char s[8];
    snprintf(s, sizeof(s), "%d%%", iround(p));
    c.text_center(109, 55, s, font_4x6);

    // Clawd watches the bits, glares at the bar when it stalls, cheers at the end
    Pose ps = pose(EYES_RIGHT);
    int cy = 39;
    if (stalled) {
        ps.eyes = (in / 500) % 2 ? EYES_RIGHT : EYES_OPEN;
        if (in >= 2700 && in < 3000) ps.arms = ARMS_ONE_UP;   // taps the cable
    }
    if (done) {
        uint32_t q = in - DL_RUN;
        ps = pose(EYES_CLOSED, ARMS_UP);
        if (q < 900) cy -= iround(hop(q, 5));
        bool alt = (q / 150) % 2;
        sparkle(c, DL_CX - 26, alt ? 40 : 42, 2);
        sparkle(c, DL_CX + 26, alt ? 38 : 36, 3);
    } else if (!stalled && in < DL_RUN && (h >> 14) % 2 && blink_k(in, 1700) >= 0) {
        ps.eyes = EYES_CLOSED;
    }
    clawd(c, 4, cy, ps);
    if (stalled) c.text(40, 26, "...", font_5x7);
}

static const Anim A_CRANE = {"crane", a_crane, CR_CYC, LOOP};
static const Anim A_TUBES = {"test_tubes", a_test_tubes, 2 * TT_CYC, LOOP};
static const Anim A_CHECKS = {"checklist_run", a_checklist_run, 2 * CL_CYC, LOOP};
static const Anim A_GRAPH = {"git_graph", a_git_graph, GG_ROW * GG_P, LOOP};
static const Anim A_PACKAGE = {"package", a_package, 2 * PK_CYC, LOOP};
static const Anim A_DOWNLOAD = {"download", a_download, 2 * DL_CYC, LOOP};

ANIM_LIST(anims_jobs, &A_CRANE, &A_TUBES, &A_CHECKS, &A_GRAPH, &A_PACKAGE, &A_DOWNLOAD);
