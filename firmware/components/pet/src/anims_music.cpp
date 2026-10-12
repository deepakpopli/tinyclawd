// music from the laptop: the song title, an equalizer, head banging
#include "anim.h"

static const uint32_t BEAT = 470;   // about 128 bpm

// 0 at the start of a beat, 1 just before the next one
static float beat_phase(uint32_t t) { return (t % BEAT) / (float)BEAT; }

// equalizer bars along the bottom edge, k scales them (0 = gone)
static void eq_bars(Canvas& c, uint32_t t, uint32_t seed, float k) {
    if (k <= 0) return;
    uint32_t beat = t / BEAT;
    float p = beat_phase(t);
    for (int i = 0; i < 14; i++) {
        int x = 2 + i * 9;
        // a new peak on every beat, falls until the next one
        float peak = 2 + hash32(beat * 31 + i * 7 + seed) % 9;
        // the middle bars hit harder, like a real spectrum
        if (i > 3 && i < 10) peak += 1;
        int h = iround(peak * (1 - 0.6f * p) * k);
        if (h > 0) c.fill_rect(x, 63 - h + 1, x + 5, 63);
        // the cap falls slower than the bar
        int cap = iround(peak * (1 - 0.25f * p) * k) + 2;
        if (k > 0.5f) c.hline(x, x + 5, 63 - cap);
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
    eq_bars(c, t, x.seed, k);

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

static const Anim A_NOW_PLAYING = {"now_playing", a_now_playing, 5600, CALM};
static const Anim A_HEADBANG = {"headbang", a_headbang, 3900, CALM};

ANIM_LIST(anims_music, &A_NOW_PLAYING, &A_HEADBANG);
