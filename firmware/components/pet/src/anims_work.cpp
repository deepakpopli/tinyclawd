// what Clawd does while Claude Code works: one look per tool, plus done / needs you
#include <stdio.h>

#include "anim.h"

static const float PI = (float)M_PI;

// ------------------------------------------------------------------- helpers

// two eyes at any spot, right one mirrored
static void eyes_at(Canvas& c, float lx, float rx, float y, const Eye& e) {
    Eye r = e;
    r.mirror = true;
    draw_eye(c, lx, y, e);
    draw_eye(c, rx, y, r);
}

// blink k for a blink starting at `at`, or -1
static float blink_k(uint32_t t, uint32_t at) {
    return t >= at && t < at + BLINK_MS ? (t - at) / (float)BLINK_MS : -1;
}

static Eye maybe_blink(const Eye& e, float k) { return k >= 0 ? blink_eye(e, k) : e; }

// --------------------------------------------------------------------- working
// two gears turning beside his eyes, he watches them like he is fixing something

// a gear: solid body, square teeth, a hole. only one sin/cos per gear,
// the teeth are found by rotating a step at a time
static void gear(Canvas& c, float cx, float cy, float rin, float rout, int teeth, float ang) {
    float ca = cosf(ang), sa = sinf(ang);
    float step = 2 * PI / teeth;
    float cs = cosf(step), ss = sinf(step);
    float body[24];
    for (int k = 0; k < teeth; k++) {
        body[2 * k] = cx + rin * ca;
        body[2 * k + 1] = cy + rin * sa;
        c.line(cx + (rin - 2) * ca, cy + (rin - 2) * sa, cx + rout * ca, cy + rout * sa, 3);
        float na = ca * cs - sa * ss;
        sa = sa * cs + ca * ss;
        ca = na;
    }
    c.fill_poly(body, teeth);
    c.fill_circle(cx, cy, 2, false);
}

static void a_working(Canvas& c, uint32_t t, const Ctx& x) {
    // gears turn in little pushes, like someone cranking them
    float turns = t / 1400.0f;
    float push = turns - floorf(turns);
    float ang = (floorf(turns) + ease(push * 1.6f)) * (2 * PI / 10);
    gear(c, 97, 24, 11, 15, 10, ang);
    gear(c, 115, 48, 6, 9, 6, -ang * 10 / 6 + 0.3f);

    // eyes on the left, focused on the gears
    Eye e;
    e.w = 12;
    e.h = 18;
    e.dx = 4;
    e.dy = -1 + (push < 0.3f ? 1 : 0);   // a small nod with each push
    uint32_t slot = t / 3100, in = t % 3100;
    uint32_t h = hash32(slot * 17 + x.seed);
    uint32_t at = 400 + h % 1800;
    int ev = (h >> 12) % 4;
    float k = -1;
    if (ev == 0 || ev == 2) k = blink_k(in, at);
    if (ev == 1 && in >= at && in < at + 900) {   // glance at you, then back to work
        float u = sinf(PI * seg(in, at, at + 900));
        e.dx = lerp(e.dx, -2, u);
        e.h = lerp(18, EYE_H, u);
    }
    eyes_at(c, 24, 58, 33, maybe_blink(e, k));
    if (ev == 3 && in >= at && in < at + 1400)   // hard work: a sweat drop slides down
        sweat(c, 10, 12 + 10 * seg(in, at, at + 1400));
}

// ------------------------------------------------------------------- searching

struct SearchCycle {
    int legs;        // sweeps
    uint32_t leg;    // ms per sweep
    bool found;
    float row;       // which row it searches (dy)
    uint32_t len;
};

static SearchCycle search_cycle(uint32_t i, uint32_t seed) {
    uint32_t h = hash32(i * 2654435761u + seed);
    SearchCycle s;
    s.legs = 2 + h % 3;
    s.leg = 420 + (h >> 4) % 380;
    s.found = (h >> 12) % 3 != 0;
    s.row = (float)((int)((h >> 16) % 9) - 4);
    s.len = s.legs * s.leg + (s.found ? 1500 : 900);
    return s;
}

static void a_searching(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t i = 0, start = 0;
    SearchCycle s = search_cycle(0, x.seed);
    while (t >= start + s.len) {
        start += s.len;
        s = search_cycle(++i, x.seed);
    }
    uint32_t in = t - start, sweep = s.legs * s.leg;
    float dir = (hash32(i + x.seed) & 1) ? 1 : -1;
    Eye e;
    e.w = 8;
    e.h = 8;
    if (in < sweep) {
        // legs: 0 -> -22 -> +22 -> ... -> 0, the first and last are half legs
        float pos = in / (float)s.leg;
        int leg = (int)pos;
        float f = pos - leg;
        float a = leg == 0 ? 0 : (leg % 2 ? -22 : 22) * dir;
        float b = leg == s.legs - 1 ? 0 : (leg % 2 ? 22 : -22) * dir;
        e.dx = lerp(a, b, ease(f));
        e.dy = s.row * sinf(PI * in / sweep);
        face(c, e);
        float ux = 64 + e.dx * 2;
        c.hline(iround(ux - 10), iround(ux + 10), 56);
        return;
    }
    uint32_t q = in - sweep;
    if (s.found) {
        const Key g[] = {{0, 0}, {180, 1}, {1200, 1}, {1500, 0}};
        float k = KEYS(g, q);
        e.w = lerp(8, 14, k);
        e.h = lerp(8, 26, k);
        e.dy = -2 * k;
        if (q > 120 && q < 1000) c.text(61, 0, "!", font_bold16);
        face(c, maybe_blink(e, blink_k(q, 950)));
        return;
    }
    // not found: squint, a little "?" and back to it
    const Key l[] = {{0, 0}, {200, 0.35f}, {650, 0.35f}, {900, 0}};
    e.w = 10;
    e.h = 10;
    e.lid = KEYS(l, q);
    e.dy = 2 * e.lid;
    face(c, e);
    if (q > 150 && q < 750) c.text(104, 8, "?", font_6x10);
}

// ------------------------------------------------------------------- needs you

static const float NY[] = {0, -3, -4, -3, 0, 0, 0, 0};

static void a_needs_you(Canvas& c, uint32_t t, const Ctx& x) {
    Eye e;
    e.w = 15;
    e.h = 26;
    e.dy = pickf(NY, 8, t / 90.0f) + 3;
    // looks around for you now and then
    uint32_t slot = t / 2400, in = t % 2400;
    uint32_t h = hash32(slot * 41 + x.seed);
    if ((h & 3) == 0 && in > 900 && in < 1900) {
        float u = sinf(PI * seg(in, 900, 1900));
        e.dx = ((h >> 4) & 1 ? 8 : -8) * u;
    }
    face(c, maybe_blink(e, (h & 3) == 1 ? blink_k(in, 1300) : -1));
    // big ! on both sides, blinking
    if ((t / 360) % 2 == 0) {
        c.text(6, 20, "!", font_bold24);
        c.text(115, 20, "!", font_bold24);
    }
    if (*x.who) c.text_center(64, 57, x.who, font_5x7);   // which project
    // a hard flash every few seconds so it is seen from across the room
    uint32_t f = t % 2880;
    if (f < 90 || (f >= 180 && f < 270)) c.invert();
}

// ------------------------------------------------------------------------ done

static const float HOP[] = {0, -4, -6, -4, 0, 0, 0};

static void sparkles4(Canvas& c, bool alt) {
    static const int A[][2] = {{14, 10}, {112, 14}, {20, 50}, {108, 48}};
    static const int B[][2] = {{10, 20}, {116, 34}, {26, 56}, {102, 56}};
    for (int k = 0; k < 4; k++) sparkle(c, alt ? B[k][0] : A[k][0], alt ? B[k][1] : A[k][1]);
}

static void a_done(Canvas& c, uint32_t t, const Ctx&) {
    if (t < 300 || t >= 1780) {
        face(c);
        return;
    }
    if (t < 420) {
        face(c, blink_eye(Eye(), 0.5f * seg(t, 300, 420)));
        return;
    }
    if (t >= 1660) {
        face(c, blink_eye(Eye(), 0.5f + 0.5f * seg(t, 1660, 1780)));
        return;
    }
    Eye e;
    e.shape = HAPPY;
    e.dy = pickf(HOP, 7, (t - 420) / 90.0f);
    face(c, e);
    sparkles4(c, ((t - 420) / 180) % 2);
}

// ----------------------------------------------------------------------- party

static void confetti(Canvas& c, uint32_t t, uint32_t seed) {
    for (uint32_t k = 0; k < 50; k++) {
        uint32_t h = hash32(k * 97 + seed);
        uint32_t born = (h >> 8) % 1600;
        if (t < born) continue;
        float v = 0.05f + ((h >> 16) % 100) / 100.0f * 0.03f;
        float y = -4 + v * (t - born);
        if (y > 64) continue;
        float x0 = (h & 0x7f);
        float xx = x0 + 4 * sinf(t / 210.0f + x0);
        int kind = (h >> 24) % 3;
        if (kind == 0) c.fill_rect(iround(xx), iround(y), iround(xx) + 1, iround(y) + 1);
        else if (kind == 1) c.line(xx, y, xx + 1, y + 2);
        else c.line(xx, y + 2, xx + 2, y);
    }
}

static const float PB[] = {0, -3, -4, -3, 0, 0};

static void a_party(Canvas& c, uint32_t t, const Ctx& x) {
    if (t < 200 || t >= 3020) face(c);
    else if (t < 320) face(c, blink_eye(Eye(), 0.5f * seg(t, 200, 320)));
    else if (t >= 2900) face(c, blink_eye(Eye(), 0.5f + 0.5f * seg(t, 2900, 3020)));
    else {
        Eye e;
        e.shape = HAPPY;
        e.dy = pickf(PB, 6, (t - 320) / 70.0f);
        face(c, e);
    }
    if (t >= 200) confetti(c, t - 200, x.seed);
}

// --------------------------------------------------------------------- reading

struct Line {
    int x0, len, nw;
    uint8_t w[10];   // word widths
};

static Line text_line(uint32_t n, uint32_t seed) {
    Line l;
    uint32_t h = hash32(n * 7349 + seed);
    l.x0 = (h & 7) == 0 ? 28 : 20;
    int maxlen = (h >> 3) % 5 == 0 ? 30 + (h >> 6) % 30 : 70 + (h >> 6) % 18;
    l.len = 0;
    l.nw = 0;
    while (l.nw < 10) {
        int w = 3 + hash32(h + l.nw * 13) % 9;
        if (l.len + w > maxlen && l.nw) break;
        l.w[l.nw++] = w;
        l.len += w + 3;
    }
    l.len -= 3;
    return l;
}

// words left of `done` are already read: drawn as a dotted checker
static void draw_line(Canvas& c, const Line& l, int y, int done) {
    int xx = l.x0;
    for (int k = 0; k < l.nw; k++) {
        for (int i = xx; i < xx + l.w[k]; i++) {
            bool read = i < done;
            c.px(i, y, !read || (i + y) % 2 == 0);
            c.px(i, y + 1, !read || (i + y) % 2 == 1);
        }
        xx += l.w[k] + 3;
    }
}

static const int RD_MS_PX = 16, RD_BACK = 260, RD_PAUSE = 1000;

static uint32_t line_ms(const Line& l, uint32_t n, uint32_t seed, bool& pause) {
    pause = hash32(n * 31 + seed + 5) % 7 == 0;
    return l.len * RD_MS_PX + RD_BACK + (pause ? RD_PAUSE : 0);
}

static void a_reading(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t n = 0, start = 0;
    bool pause;
    Line cur = text_line(0, x.seed);
    uint32_t len = line_ms(cur, 0, x.seed, pause);
    while (t >= start + len) {
        start += len;
        cur = text_line(++n, x.seed);
        len = line_ms(cur, n, x.seed, pause);
    }
    uint32_t in = t - start, read = cur.len * RD_MS_PX;
    // eyes hop word by word along the line
    uint32_t px = in / RD_MS_PX, ws = 0, prev = 0;
    for (int k = 0; k < cur.nw - 1; k++) {
        uint32_t next = ws + cur.w[k] + 3;
        if (px < next) break;
        prev = ws;
        ws = next;
    }
    float rx = cur.x0 + lerp(prev, ws, ease(clamp01((px - (float)ws) / 4.0f)));
    float rx_end = cur.x0 + ws;
    float scroll = 0;   // 0..1 while the lines move up
    Eye e;
    e.w = 8;
    e.h = 10;
    e.dy = -12;
    if (in >= read) {
        uint32_t q = in - read;
        if (pause && q < RD_PAUSE) {   // stops to think, looks up at you
            float u = sinf(PI * seg(q, 0, RD_PAUSE));
            rx = rx_end;
            e.dy -= 6 * u;
            e.h = lerp(10, 14, u);
            e = maybe_blink(e, blink_k(q, 450));
        } else {
            uint32_t b = q - (pause ? RD_PAUSE : 0);
            float k = ease(seg(b, 0, RD_BACK - 60));
            rx = lerp(rx_end, text_line(n + 1, x.seed).x0, k);
            scroll = k;
        }
    }
    e.dx = (rx - 64) / 3.5f;
    if (hash32(n + x.seed * 3) % 4 == 0) e = maybe_blink(e, blink_k(in, read / 2));
    Eye l = e, r = e;
    r.mirror = true;
    draw_eye(c, LEFT_X + 6, EYE_Y, l);
    draw_eye(c, RIGHT_X - 6, EYE_Y, r);
    // text: lines already read above, the current one, the next ones below
    for (int j = -2; j <= 4; j++) {
        if ((int)n + j < 0) continue;
        int y = 40 + j * 6 - iround(scroll * 6);
        if (y < 34 || y > 62) continue;
        int done = j < 0 ? 999 : (j == 0 ? (scroll > 0 ? 999 : iround(rx) + 1) : 0);
        draw_line(c, text_line(n + j, x.seed), y, done);
    }
}

// ---------------------------------------------------------------------- typing

static const char* const NAMES[] = {"x", "i", "n", "ok", "buf", "len", "eye", "pet", "tick", "draw", "t", "acc"};
static const char* const KW[] = {"fn", "let", "int", "auto", "void"};
static const char* const OPS[] = {" = ", " += ", " < ", " == ", " - "};

static void code_line(uint32_t n, uint32_t seed, char* out, int cap) {
    uint32_t h = hash32(n * 3301 + seed);
    const char* a = NAMES[(h >> 4) % 12];
    const char* b = NAMES[(h >> 8) % 12];
    const char* op = OPS[(h >> 12) % 5];
    switch (h % 8) {
    case 0: snprintf(out, cap, "%s %s(%s) {", KW[(h >> 16) % 5], a, b); break;
    case 1: snprintf(out, cap, "  %s%s%s;", a, op, b); break;
    case 2: snprintf(out, cap, "  %s(%s);", a, b); break;
    case 3: snprintf(out, cap, "}"); break;
    case 4: snprintf(out, cap, "  if (%s%s%d) {", a, op, (int)((h >> 20) % 10)); break;
    case 5: snprintf(out, cap, "  return %s;", a); break;
    case 6: snprintf(out, cap, "// %s %s", a, b); break;
    default: snprintf(out, cap, "  %s++;", a); break;
    }
}

static const uint32_t TY_CH = 85, TY_HOLD = 450, TY_SCROLL = 140;

static uint32_t typing_ms(uint32_t n, uint32_t seed) {
    char s[32];
    code_line(n, seed, s, sizeof(s));
    return strlen(s) * TY_CH + TY_HOLD + TY_SCROLL;
}

static void a_typing(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t n = 0, start = 0, len = typing_ms(0, x.seed);
    while (t >= start + len) {
        start += len;
        len = typing_ms(++n, x.seed);
    }
    uint32_t in = t - start;
    char cur[32];
    code_line(n, x.seed, cur, sizeof(cur));
    int nch = strlen(cur);
    // a little thinking pause somewhere in the line
    uint32_t h = hash32(n * 7 + x.seed);
    int stop = (h & 1) ? 2 + (h >> 3) % (nch > 3 ? nch - 2 : 1) : 999;
    uint32_t typed_ms = in;
    if ((int)(in / TY_CH) > stop) typed_ms = in < (uint32_t)stop * TY_CH + 400 ? stop * TY_CH : in - 400;
    int shown = typed_ms / TY_CH;
    if (shown > nch) shown = nch;
    uint32_t end_type = nch * TY_CH + (stop < nch ? 400 : 0);
    float scroll = in > end_type + TY_HOLD ? ease(seg(in, end_type + TY_HOLD, len)) : 0;
    int base = 56 - iround(scroll * 8);
    char buf[32];
    for (int j = 3; j >= 1; j--) {
        if ((int)n - j < 0) continue;
        int y = base - j * 8;
        if (y < 36) continue;
        code_line(n - j, x.seed, buf, sizeof(buf));
        c.text(2, y, buf, font_5x7);
    }
    buf[0] = 0;
    strncpy(buf, cur, shown);
    buf[shown] = 0;
    int cx = c.text(2, base, buf, font_5x7);
    bool typing = in < end_type;
    if (scroll == 0 && (typing || (t / 265) % 2 == 0)) c.fill_rect(cx, base, cx + 4, base + 6);
    // eyes look down at the cursor
    Eye e;
    e.h = 16;
    e.w = 11;
    e.dy = -8;
    e.dx = (cx - 64) / 6.0f;
    if (h % 3 == 0) e = maybe_blink(e, blink_k(in, end_type + 60));
    if (typing && shown > 0 && (h >> 5) % 4 == 0) e.dy += 0.6f * ((in / 90) % 2);   // typing hard
    face(c, e);
}

// -------------------------------------------------------------------- terminal
// a full screen terminal: title bar, real commands typing out, output scrolling,
// and a big check or x when a command finishes

struct Cmd {
    const char* cmd;
    const char* out[3];
    int nout;
    bool ok;    // false = it failed, shows the x
    bool bar;   // a build: a progress bar before the output
};

static const Cmd CMDS[] = {
    {"npm test", {"> jest", "PASS 12 tests", "done in 1.4s"}, 3, true, false},
    {"ls", {"src  docs  test", "README.md"}, 2, true, false},
    {"make", {"cc main.c", "cc pet.c", "ld tinyclawd"}, 3, true, true},
    {"git status", {"on branch main", "2 files changed"}, 2, true, false},
    {"cargo build", {"Compiling pet", "Finished"}, 2, true, true},
    {"pytest -q", {"..F.....", "1 failed"}, 2, false, false},
    {"git diff", {"+ eyes.y += 1", "- eyes.y -= 1"}, 2, true, false},
    {"idf.py build", {"Linking...", "build ok"}, 2, true, true},
    {"npm run lint", {"pet.ts:12", "error: no-var"}, 2, false, false},
    {"grep -rn TODO", {"pet.c:12 TODO", "face.c:40 TODO"}, 2, true, false},
};
static const int NCMD = sizeof(CMDS) / sizeof(CMDS[0]);
static const uint32_t TM_CH = 90, TM_WAIT = 400, TM_BAR = 1400, TM_OUT = 300, TM_MARK = 700, TM_IDLE = 500;
static const int TM_ROWS = 5;

static int block_cmd(uint32_t b, uint32_t seed) {
    int k = hash32(b * 613 + seed) % NCMD;
    if (b > 0 && k == (int)(hash32((b - 1) * 613 + seed) % NCMD)) k = (k + 3) % NCMD;
    return k;
}

static uint32_t wait_ms(const Cmd& m) { return m.bar ? TM_BAR : TM_WAIT; }

static uint32_t block_ms(int k) {
    const Cmd& m = CMDS[k];
    return strlen(m.cmd) * TM_CH + wait_ms(m) + m.nout * TM_OUT + TM_MARK + TM_IDLE;
}

static void big_check(Canvas& c, int x, int y) {
    float p[] = {(float)x + 5, (float)y + 14, (float)x + 11, (float)y + 20, (float)x + 23, (float)y + 6};
    c.polyline(p, 3, 4, false);
}

static void big_x(Canvas& c, int x, int y) {
    c.line(x + 7, y + 7, x + 20, y + 20, 4, false);
    c.line(x + 7, y + 20, x + 20, y + 7, 4, false);
}

static void a_terminal(Canvas& c, uint32_t t, const Ctx& x) {
    // title bar
    c.fill_rect(0, 0, 127, 9);
    for (int k = 0; k < 3; k++) c.fill_rect(3 + k * 6, 3, 5 + k * 6, 5, false);
    c.text_center(64, 1, "bash", font_5x7, false);

    uint32_t b = 0, start = 0;
    int k = block_cmd(0, x.seed);
    while (t >= start + block_ms(k)) {
        start += block_ms(k);
        k = block_cmd(++b, x.seed);
    }
    uint32_t in = t - start;
    const Cmd& cm = CMDS[k];
    uint32_t typed = strlen(cm.cmd) * TM_CH;
    uint32_t ran = typed + wait_ms(cm);
    uint32_t done_at = ran + cm.nout * TM_OUT;

    // lines bottom up: 0 is the newest
    const char* lines[TM_ROWS];
    int chars[TM_ROWS];
    bool prompt[TM_ROWS];
    int n = 0;
    auto push = [&](const char* s, int ch, bool p) {
        if (n < TM_ROWS) { lines[n] = s; chars[n] = ch; prompt[n] = p; n++; }
    };
    int outs = in < ran ? 0 : 1 + (in - ran) / TM_OUT;
    if (outs > cm.nout) outs = cm.nout;
    bool done = in >= done_at;
    bool barring = cm.bar && in >= typed && in < ran;
    if (done) push("", 0, true);   // the next prompt
    for (int o = outs - 1; o >= 0; o--) push(cm.out[o], 99, false);
    if (barring) push("", 0, false);   // room for the bar
    push(cm.cmd, in / TM_CH, true);
    for (int bb = (int)b - 1; bb >= 0 && n < TM_ROWS; bb--) {
        const Cmd& p = CMDS[block_cmd(bb, x.seed)];
        for (int o = p.nout - 1; o >= 0; o--) push(p.out[o], 99, false);
        push(p.cmd, 99, true);
    }

    int cur_x = -1, cur_y = 0;
    for (int i = 0; i < n; i++) {
        int y = 13 + (n - 1 - i) * 10;
        int xx = 2;
        if (prompt[i]) {
            c.text(xx, y, "$", font_6x10);   // bold: drawn twice
            c.text(xx + 1, y, "$", font_6x10);
            xx += 10;
        }
        char buf[22];
        int m = chars[i], sl = strlen(lines[i]);
        if (m > sl) m = sl;
        if (m > 20) m = 20;
        memcpy(buf, lines[i], m);
        buf[m] = 0;
        int ex = c.text(xx, y, buf, font_6x10);
        if (i == 0 && prompt[i]) { cur_x = ex; cur_y = y; }
        if (barring && i == 0) {   // build progress
            float f = seg(in, typed, ran);
            c.rect(xx, y + 1, xx + 80, y + 7);
            c.fill_rect(xx + 2, y + 3, xx + 2 + iround(76 * f), y + 5);
        }
    }
    // fat block cursor: solid while typing, blinks while idle, gone while running
    bool running = !done && in >= typed;
    if (cur_x >= 0 && !running && (in < typed || (t / 400) % 2 == 0))
        c.fill_rect(cur_x + 1, cur_y, cur_x + 6, cur_y + 9);

    // the result, big, for a moment
    if (in >= done_at && in < done_at + TM_MARK) {
        int bx = 92, by = 20;
        c.fill_rect(bx - 2, by - 2, bx + 29, by + 29, false);
        c.fill_rect(bx, by, bx + 27, by + 27);
        if (cm.ok) big_check(c, bx, by);
        else big_x(c, bx, by);
    }
}

// ------------------------------------------------------------------------- web

static void globe(Canvas& c, float gx, float gy, float r, float rot) {
    c.fill_circle(gx, gy, r + 1, false);   // in front of whatever is behind
    c.ellipse(gx - r, gy - r, gx + r, gy + r);
    c.hline(iround(gx - r), iround(gx + r), iround(gy));
    float hw = sqrtf(r * r - (r * 0.5f) * (r * 0.5f));
    c.hline(iround(gx - hw), iround(gx + hw), iround(gy - r * 0.5f));
    c.hline(iround(gx - hw), iround(gx + hw), iround(gy + r * 0.5f));
    for (int k = 0; k < 3; k++) {
        float a = rot + k * PI / 3;
        float w = r * cosf(a);
        if (fabsf(w) < 1) c.vline(iround(gx), iround(gy - r), iround(gy + r));
        else c.ellipse(gx - fabsf(w), gy - r, gx + fabsf(w), gy + r);
    }
}

static void a_web(Canvas& c, uint32_t t, const Ctx& x) {
    float ph = (x.seed % 628) / 100.0f;
    float gx = 64 + 46 * sinf(t / 2300.0f + ph);
    float gy = 30 + 17 * sinf(t / 1700.0f + 1 + ph);
    float rot = t / 480.0f;
    // the eyes follow it, sometimes they look around on their own
    uint32_t slot = t / 2600, in = t % 2600;
    uint32_t h = hash32(slot * 59 + x.seed);
    Eye e;
    e.w = 10;
    e.h = 18;
    e.dx = clampf((gx - 64) / 4, -11, 11);
    e.dy = clampf((gy - 32) / 4, -5, 5);
    if ((h & 3) == 0 && in > 600 && in < 1800) {
        float u = sinf(PI * seg(in, 600, 1800));
        e.dx = lerp(e.dx, ((h >> 3) & 1 ? 12 : -12), u);
        e.dy = lerp(e.dy, -4, u);
    }
    face(c, maybe_blink(e, (h & 3) == 1 ? blink_k(in, 1100) : -1));
    // data coming in from the globe
    for (int k = 0; k < 3; k++) {
        float p = fmodf(t / 900.0f + k / 3.0f, 1);
        float tx = k == 1 ? RIGHT_X : LEFT_X;
        c.fill_rect(iround(lerp(gx, tx, p)), iround(lerp(gy, EYE_Y, p)), iround(lerp(gx, tx, p)) + 1,
                    iround(lerp(gy, EYE_Y, p)) + 1);
    }
    globe(c, gx, gy, 11, rot);
    // a ping now and then
    uint32_t pq = t % 3700;
    if (pq < 500) {
        float r = 13 + 14 * pq / 500.0f;
        for (int a = 0; a < 16; a++) c.px(iround(gx + r * cosf(a * PI / 8)), iround(gy + r * sinf(a * PI / 8)));
    }
}

// ------------------------------------------------------------------------ todo

static void a_todo(Canvas& c, uint32_t t, const Ctx& x) {
    int n = 3 + (x.seed >> 3) % 2;
    const uint32_t IN = 300, T0 = 700, STEP = 500;
    uint32_t t_happy = T0 + n * STEP, t_out = t_happy + 600, T_END = t_out + 400;
    float m = t < IN ? ease(seg(t, 0, IN)) : (t >= t_out ? 1 - ease(seg(t, t_out, T_END)) : 1);
    float lx = lerp(LEFT_X, 18, m), rx = lerp(RIGHT_X, 42, m);
    int listx = iround(lerp(130, 60, m));
    int top = 32 - n * 11 / 2;
    int cur = t < T0 ? 0 : (t - T0) / STEP;   // item being ticked
    for (int i = 0; i < n; i++) {
        int y = top + i * 11;
        c.rect(listx, y, listx + 6, y + 6);
        int len = 18 + hash32(i * 11 + x.seed) % 32;
        bool ticked = t >= T0 + i * STEP + 200;
        if (ticked) {
            float pts[] = {(float)listx + 1, (float)y + 3, (float)listx + 3, (float)y + 5, (float)listx + 7, (float)y - 1};
            c.polyline(pts, 3, 2);
            for (int xx = listx + 11; xx < listx + 11 + len; xx += 2) c.px(xx, y + 3);
        } else c.fill_rect(listx + 11, y + 2, listx + 11 + len, y + 3);
    }
    Eye e;
    e.w = lerp(EYE_W, 10, m);
    e.h = lerp(EYE_H, 18, m);
    if (t >= t_happy && t < t_out) {
        e.shape = HAPPY;
        e.w = 8;
        e.dy = -2 * sinf(PI * seg(t, t_happy, t_out));
    } else if (t >= T0 && cur < n) {
        int y = top + cur * 11 + 3;
        uint32_t in = (t - T0) % STEP;
        e.dy = (y - 32) / 3.0f + 3 * sinf(PI * seg(in, 200, 380));   // nod on the tick
        e.dx = 3;
    }
    if (m > 0 && m < 1) e.dy = 0;
    eyes_at(c, lx, rx, EYE_Y, e);
}

// ------------------------------------------------------------------------ plug

struct PlugCycle {
    bool miss;
    uint32_t hold, len;
};

static PlugCycle plug_cycle(uint32_t i, uint32_t seed) {
    uint32_t h = hash32(i * 1931 + seed);
    PlugCycle p;
    p.miss = i > 0 && h % 3 == 0;
    p.hold = 1400 + (h >> 4) % 1600;
    p.len = 900 + (p.miss ? 800 : 0) + 350 + p.hold + 600;
    return p;
}

static void a_plug(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t i = 0, start = 0;
    PlugCycle p = plug_cycle(0, x.seed);
    while (t >= start + p.len) {
        start += p.len;
        p = plug_cycle(++i, x.seed);
    }
    uint32_t in = t - start;
    const float OUT = 30, TOUCH = 89, IN = 95;   // plug front x
    float px = OUT, py = 0;
    uint32_t a = 900, miss = p.miss ? 800 : 0;
    if (in < a) {
        px = lerp(OUT, TOUCH - 2, ease(seg(in, 0, a)));
    } else if (in < a + miss) {   // bumps the socket, backs off, lines up, tries again
        const Key mx[] = {{0, TOUCH - 2}, {120, TOUCH - 1}, {300, TOUCH - 8}, {650, TOUCH - 8}, {800, TOUCH - 2}};
        const Key my[] = {{120, 4}, {300, 3}, {520, 0}};
        px = KEYS(mx, in - a);
        py = KEYS(my, in - a);
    } else {
        uint32_t q = in - a - miss;
        if (q < 350) px = lerp(TOUCH - 2, IN, ease_in(seg(q, 0, 350)));
        else if (q < 350 + p.hold) px = IN;
        else px = lerp(IN, OUT, ease(seg(q, 350 + p.hold, 350 + p.hold + 600)));
    }
    bool plugged = px >= IN - 0.5f;
    uint32_t since = 0;
    if (plugged) since = in - a - miss - 350;
    int y0 = 50 + iround(py);
    // cable from the left edge
    float bx = px - 6 - 12;   // back of the plug body
    c.line(0, 58, bx * 0.5f, 60, 2);
    c.line(bx * 0.5f, 60, bx, y0 + 4, 2);
    // plug: body + two prongs
    c.fill_rect(iround(bx), y0, iround(px - 7), y0 + 9);
    c.fill_rect(iround(px - 6), y0 + 2, iround(px), y0 + 2);
    c.fill_rect(iround(px - 6), y0 + 7, iround(px), y0 + 7);
    // socket: plate in front, box behind
    c.fill_rect(96, 46, 98, 63);
    c.rect(99, 48, 112, 61);
    c.line(112, 54, 127, 54);
    if (plugged) {
        if (since < 320) {   // sparks
            float s = since / 320.0f;
            for (int k = 0; k < 7; k++) {
                float an = PI + (k - 3) * 0.42f;
                float r0 = 3 + 9 * s, r1 = r0 + 4;
                c.line(94 + r0 * cosf(an), 55 + r0 * sinf(an), 94 + r1 * cosf(an), 55 + r1 * sinf(an));
            }
        } else {   // data flowing along the cable
            for (int k = 0; k < 3; k++) {
                float q = fmodf(since / 700.0f + k / 3.0f, 1);
                float dx = lerp(bx, 0, q);
                float dy = dx > bx * 0.5f ? lerp(y0 + 4, 60, (bx - dx) / (bx * 0.5f + 0.01f)) : lerp(60, 58, (bx * 0.5f - dx) / (bx * 0.5f + 0.01f));
                c.fill_rect(iround(dx), iround(dy) - 2, iround(dx) + 1, iround(dy) - 1);
            }
        }
    }
    Eye e;
    e.w = 11;
    e.h = 16;
    e.dy = -12;
    e.dx = clampf((px - 70) / 4, -10, 9);
    if (plugged && since < 450) {   // flinch at the sparks, then happy
        e = blink_eye(e, 0.25f + 0.25f * sinf(PI * since / 450.0f));
    } else if (plugged && since < 1200) {
        e.shape = HAPPY;
        e.w = 9;
        e.dy -= 1;
    }
    if (p.miss && in >= a && in < a + 300) e.dx -= 2, e.h = 18;
    face(c, e);
}

// ---------------------------------------------------------------------- squish

static void wall(Canvas& c, int x0, int x1) {
    if (x1 < x0) return;
    c.fill_rect(x0, 0, x1, 63);
    for (int y = 7; y < 64; y += 8) c.hline(x0, x1, y, false);
    for (int y = 0; y < 64; y += 8) {
        int off = (y / 8) % 2 ? 4 : 0;
        for (int xx = (x0 / 8) * 8 + off; xx <= x1; xx += 8)
            if (xx >= x0) c.vline(xx, y, y + 6, false);
    }
}

static void a_squish(Canvas& c, uint32_t t, const Ctx& x) {
    const Key k[] = {{0, 0}, {300, 0}, {900, 1}, {1800, 1}, {2100, 0}};
    float s = KEYS(k, t);
    float shake = (t > 900 && t < 1800) ? sinf(t / 23.0f) * 0.8f : 0;
    float L = -1 + 42 * s + shake;   // wall inner edge
    wall(c, 0, iround(L));
    wall(c, 127 - iround(L), 127);
    Eye l, r;
    r.mirror = true;
    if (t < 2100) {
        float xo = fmaxf(36, L + 2);   // outer edge pushed by the wall
        float xi = 48 + 3 * s;          // inner edge squeezed a bit too
        float w = fmaxf(4, xi - xo);
        l.w = r.w = w;
        l.h = r.h = fminf(36, EYE_H * EYE_W / w);
        l.dx = (xo + xi) / 2 - LEFT_X;
        r.dx = -l.dx;
        face(c, l, r);
        if (t > 1100 && t < 1800 && (x.seed & 1)) sweat(c, 64, 6 + 10 * seg(t, 1100, 1800));
        return;
    }
    // pop back with a wobble, then blink
    const Key w[] = {{2100, 4}, {2220, 15}, {2340, 11}, {2440, 12}};
    const Key hh[] = {{2100, 36}, {2220, 18}, {2340, 23}, {2440, 22}};
    Eye e;
    e.w = KEYS(w, t);
    e.h = KEYS(hh, t);
    face_blink(c, t, 2600, e);
}

static const Anim A_WORKING = {"working", a_working, 9300, LOOP};
static const Anim A_SEARCH = {"searching", a_searching, 9000, LOOP};
static const Anim A_NEEDS = {"needs_you", a_needs_you, 8640, LOOP};
static const Anim A_DONE = {"done", a_done, 2300, CALM};
static const Anim A_PARTY = {"party", a_party, 3300, CALM};
static const Anim A_READING = {"reading", a_reading, 10000, LOOP};
static const Anim A_TYPING = {"typing", a_typing, 10000, LOOP};
static const Anim A_TERMINAL = {"terminal", a_terminal, 12000, LOOP};
static const Anim A_WEB = {"web", a_web, 11100, LOOP};
static const Anim A_TODO = {"todo", a_todo, 3400, CALM};
static const Anim A_PLUG = {"plug", a_plug, 9000, LOOP};
static const Anim A_SQUISH = {"squish", a_squish, 3100, CALM};

ANIM_LIST(anims_work, &A_WORKING, &A_SEARCH, &A_NEEDS, &A_DONE, &A_PARTY, &A_READING, &A_TYPING,
          &A_TERMINAL, &A_WEB, &A_TODO, &A_PLUG, &A_SQUISH);
