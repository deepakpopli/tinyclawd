// music from the laptop: the song title, head banging and full screen visualizers
#include "anim.h"

static const uint32_t BEAT = 470;   // about 128 bpm

// 0 at the start of a beat, 1 just before the next one
static float beat_phase(uint32_t t) { return (t % BEAT) / (float)BEAT; }

// equalizer bars along the bottom edge, k scales them (0 = gone)
static void eq_bars(Canvas& c, uint32_t t, uint32_t seed, float k, const uint8_t* live) {
    if (k <= 0) return;
    uint32_t beat = t / BEAT;
    float p = beat_phase(t);
    for (int i = 0; i < 14; i++) {
        int x = 2 + i * 9;
        // a new peak on every beat, falls until the next one
        float peak = 2 + hash32(beat * 31 + i * 7 + seed) % 9;
        // the middle bars hit harder, like a real spectrum
        if (i > 3 && i < 10) peak += 1;
        float fall = 0.6f * p;
        if (live) {   // real levels from the laptop, 14 bars share 8 bands
            peak = 2 + live[i * 8 / 14];
            fall = 0;
        }
        int h = iround(peak * (1 - fall) * k);
        if (h > 0) c.fill_rect(x, 63 - h + 1, x + 5, 63);
    }
}

// note + title in a strip at y. long titles scroll, short ones are centred.
static void title_strip(Canvas& c, int y, uint32_t t, const char* s) {
    const Font& f = font_6x10;
    if (!s || !*s) s = "music";
    int tw = c.text_width(s, f);
    const int NOTE_W = 12;
    if (tw + NOTE_W <= 124) {
        int x = (128 - tw - NOTE_W) / 2;
        c.glyph(x, y, 0x266b, font_sym12);
        c.text(x + NOTE_W, y, s, f);
        return;
    }
    // marquee: wait a moment, then scroll left forever with a gap between copies
    const int X0 = 16, GAP = 30;
    int shift = t < 900 ? 0 : (int)((t - 900) * 40 / 1000);
    shift %= tw + GAP;
    int adv = c.text_width("a", f);
    for (int copy = 0; copy < 2; copy++) {
        int x = X0 - shift + copy * (tw + GAP);
        for (const char* p = s; *p; p++, x += adv) {
            if (x <= X0 - adv || x >= 128) continue;   // off the strip, skip the work
            char one[2] = {*p, 0};
            c.text(x, y, one, f);
        }
    }
    // the note sits on a clean patch so letters slide under it
    c.fill_rect(0, y, X0 - 2, y + f.h, false);
    c.glyph(3, y, 0x266b, font_sym12);
}

// ---------------------------------------------------------------- now playing

static void a_now_playing(Canvas& c, uint32_t t, const Ctx& x) {
    const uint32_t IN0 = 250, IN1 = 650, OUT0 = 4950, OUT1 = 5350;
    // strip and bars come in, then go away again
    float k = ease(seg(t, IN0, IN1)) * (1 - ease(seg(t, OUT0, OUT1)));
    int y = iround(lerp(-12, 1, k));
    if (k > 0) title_strip(c, y, t - IN0, x.track);
    eq_bars(c, t, x.seed, k, x.eq_live ? x.eq : nullptr);

    // eyes look up at the title, then bob to the beat
    float look = ease(seg(t, IN0, 700)) * (1 - ease(seg(t, 1200, 1600)));
    float env = ease(seg(t, 1300, 1700)) * (1 - ease(seg(t, OUT0 - 200, OUT0 + 200)));
    float p = beat_phase(t);
    float hit = (1 - p) * (1 - p) * env;
    float side = ((t / BEAT + x.seed) & 1) ? 1 : -1;
    float sway = side * 2 * env * (1 - fabsf(2 * p - 1));
    Eye e;
    e.dy = -3 * look + 3 * hit;
    e.dx = sway;
    e.w = EYE_W + 2 * hit;
    e.h = EYE_H - 3 * hit;
    face_blink(c, t, 3100 + x.seed % 700, e);
}

// ------------------------------------------------------------------ headbang

static void a_headbang(Canvas& c, uint32_t t, const Ctx& x) {
    const uint32_t T0 = 300, NB = 7;
    if (t < T0 || t >= T0 + BEAT * NB) {
        face_blink(c, t, T0 + BEAT * NB + 50, Eye());
        return;
    }
    uint32_t b = (t - T0) / BEAT;
    float p = ((t - T0) % BEAT) / (float)BEAT;
    float env = b == 0 ? ease(p * 3) : (b == NB - 1 ? 1 - ease(p) : 1);
    // down fast on the beat, back up slowly
    float down = p < 0.18f ? ease_out(p / 0.18f) : 1 - ease((p - 0.18f) / 0.82f);
    float side = ((b + (x.seed >> 3)) & 1) ? 1 : -1;
    Eye e;
    e.lid = 0.4f * env;   // eyes half shut, really into it
    e.dy = (-4 + 10 * down) * env;
    e.dx = side * 3 * env;
    if (p < 0.18f) {   // squash on the hit
        float q = down * env;
        e.w = EYE_W + 3 * q;
        e.h = EYE_H - 3 * q;
    }
    face(c, e);
    // speed lines above the eyes on the way down
    if (p < 0.25f && env > 0.5f) {
        int top = EYE_Y - EYE_H / 2 + iround(e.dy) - 6;
        for (int i = 0; i < 2; i++) {
            int cx = (i ? RIGHT_X : LEFT_X) + iround(e.dx);
            for (int k = -4; k <= 4; k += 4) c.vline(cx + k, top - 4, top);
        }
    }
    // a note flies off on every other beat
    if (b < NB - 1) {
        uint32_t nb = b & ~1u;
        float q = ((t - T0) - nb * BEAT) / (2.0f * BEAT);
        float nx = ((nb / 2 + x.seed) & 1) ? 110 : 8;
        c.glyph(iround(nx), iround(44 - 36 * q), (nb / 2) & 1 ? 0x266a : 0x266b, font_sym12);
    }
}

// ---------------------------------------------------------------- visualizers
// full screen, no eyes, only the music. ints only: the C3 has no FPU

// levels are kept in 1/64 of a step (0..9 steps)
static const int Q = 64;
static const int TOP = 9 * Q;
static const int NB = 8;      // bands from the laptop
static const int MAXB = 32;   // most bars a visualizer draws

// made up bars when the laptop sends no levels: a kick on the beat,
// a hat on the off beat, the middle wanders. target level 0..9
static int fake_level(int i, uint32_t t, uint32_t seed) {
    uint32_t step = t / (BEAT / 4);   // sixteenth notes
    int q = step % 4;
    uint32_t h = hash32(step * 8 + i + seed);
    // some bars are louder than others
    int phrase = hash32(t / (BEAT * 8) + seed) % 3;
    int v;
    if (i < 2) v = (q == 0 ? 8 : 6 - 2 * q) + h % 2 - i;
    else if (i < 6) v = 2 + h % 5 + (q == 0 || q == 2) - (i == 5);
    else v = (q == 2 ? 5 : 1) + h % 3 - (i - 6);
    v += phrase - 2;
    return v < 0 ? 0 : (v > 9 ? 9 : v);
}

// n bars from the 8 bands, bass first. a bar between two bands blends them
// and wiggles a little, so the bars do not look like a ramp
static void targets(int* out, int n, uint32_t t, const Ctx& x) {
    int band[NB];
    for (int i = 0; i < NB; i++) band[i] = (x.eq_live ? x.eq[i] : fake_level(i, t, x.seed)) * Q;
    uint32_t step = t / (BEAT / 4);
    for (int j = 0; j < n; j++) {
        int pos = n > 1 ? j * (NB - 1) * 256 / (n - 1) : 0;
        int i = pos >> 8, f = pos & 255;
        int v = i < NB - 1 ? band[i] + (band[i + 1] - band[i]) * f / 256 : band[NB - 1];
        v += ((int)(hash32(step * 41 + j * 7 + x.seed) % 33) - 16) * v / 128;
        out[j] = v < 0 ? 0 : (v > TOP ? TOP : v);
    }
}

// smoothed bars: rise fast, fall slow
struct EqState {
    uint32_t last = 0;
    bool ok = false;
    int lvl[MAXB];

    void step(uint32_t t, const Ctx& x, int n) {
        int want[MAXB];
        targets(want, n, t, x);
        int dt = ok && t >= last ? (int)(t - last) : -1;
        if (dt > 400) dt = -1;   // a jump in time (a new play): start fresh
        last = t;
        for (int i = 0; i < n; i++) {
            if (dt < 0) lvl[i] = want[i];
            else if (want[i] > lvl[i]) lvl[i] += (want[i] - lvl[i]) * (dt > 40 ? 40 : dt) / 40;
            else {
                lvl[i] -= dt * 14 * Q / 1000;   // 14 steps a second
                if (lvl[i] < want[i]) lvl[i] = want[i];
            }
        }
        ok = true;
    }
};

// a level as 0..max steps, rounded
static int scale(int lvl, int max) { return (lvl * max + TOP / 2) / TOP; }

// ------------------------------------------------------------------ equalizer

// solid bars standing on a floor
static void a_equalizer(Canvas& c, uint32_t t, const Ctx& x) {
    static EqState s;
    const int N = 10, FLOOR = 60, MAXH = 52;
    s.step(t, x, N);
    for (int i = 0; i < N; i++) {
        int x0 = i * 13, x1 = x0 + 10;
        int h = 2 + scale(s.lvl[i], MAXH);   // never quite empty
        int top = FLOOR - h;
        c.fill_rect(x0, top, x1, FLOOR - 1);
        // soft corners and a dark line under the top, like a lit lip
        c.px(x0, top, false);
        c.px(x1, top, false);
        if (h > 5) c.hline(x0 + 1, x1 - 1, top + 2, false);
    }
    c.hline(0, 127, FLOOR);
    // the floor goes thick on a kick
    if (s.lvl[0] >= 7 * Q) c.fill_rect(0, FLOOR + 2, 127, 63);
    else for (int i = 0; i < 128; i += 2) c.px(i, 63);
}

// ------------------------------------------------------------------ eq mirror

// solid bars grow up and down from the middle, bass in the centre
static void a_eq_mirror(Canvas& c, uint32_t t, const Ctx& x) {
    static EqState s;
    const int N = 6, MAXH = 29;
    s.step(t, x, N);
    for (int i = 0; i < N; i++) {
        int h = 2 + scale(s.lvl[i], MAXH);
        for (int side = 0; side < 2; side++) {
            int x0 = side ? 65 + i * 11 : 55 - i * 11, x1 = x0 + 7;
            c.fill_rect(x0, 32 - h, x1, 32 + h);
            // soft corners and a dark line in from each end
            c.px(x0, 32 - h, false);
            c.px(x1, 32 - h, false);
            c.px(x0, 32 + h, false);
            c.px(x1, 32 + h, false);
            if (h > 5) {
                c.hline(x0 + 1, x1 - 1, 34 - h, false);
                c.hline(x0 + 1, x1 - 1, 30 + h, false);
            }
        }
    }
    // a dark seam through the middle, a solid line across on a kick
    c.hline(0, 127, 32, s.lvl[0] >= 7 * Q);
}

// ------------------------------------------------------------------- eq hills

// 0..255 eased, so the hills are round between two bands
static int smooth(int f) { return f * f * (768 - 2 * f) >> 16; }

// the height in px at column x, eased between the bars
static int hill(const int* v, int n, int x, int max) {
    int u = x * (n - 1) * 256 / 127;
    int i = u >> 8, f = smooth(u & 255);
    int a = scale(v[i], max), b = i < n - 1 ? scale(v[i + 1], max) : a;
    return a + (b - a) * f / 256;
}

// a filled spectrum across the screen under the song title
static void a_eq_hills(Canvas& c, uint32_t t, const Ctx& x) {
    static EqState s;
    const int N = 12, MAXH = 44;
    s.step(t, x, N);
    title_strip(c, 1, t, x.track);
    for (int i = 0; i < 128; i++) c.vline(i, 61 - hill(s.lvl, N, i, MAXH), 63);
}

static const Anim A_NOW_PLAYING = {"now_playing", a_now_playing, 5600, CALM};
static const Anim A_HEADBANG = {"headbang", a_headbang, 3900, CALM};
static const Anim A_EQUALIZER = {"equalizer", a_equalizer, 9400, LOOP | SCENE};
static const Anim A_EQ_MIRROR = {"eq_mirror", a_eq_mirror, 9400, LOOP | SCENE};
static const Anim A_EQ_HILLS = {"eq_hills", a_eq_hills, 9400, LOOP | SCENE};

ANIM_LIST(anims_music, &A_NOW_PLAYING, &A_HEADBANG, &A_EQUALIZER, &A_EQ_MIRROR, &A_EQ_HILLS);
