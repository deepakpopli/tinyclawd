#pragma once
#include <math.h>
#include <stdint.h>

// small helpers so animations stay a function of time

static inline float clamp01(float t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float ease(float t) { return 0.5f - 0.5f * cosf((float)M_PI * clamp01(t)); }
static inline float ease_out(float t) { t = clamp01(t); return 1 - (1 - t) * (1 - t); }
static inline float ease_in(float t) { t = clamp01(t); return t * t; }
static inline int iround(float v) { return (int)floorf(v + 0.5f); }

// 0..1 progress of t inside [a, b)
static inline float seg(uint32_t t, uint32_t a, uint32_t b) {
    if (t <= a) return 0;
    if (t >= b) return 1;
    return (float)(t - a) / (float)(b - a);
}

// like eyes.py pick(): a looping list of numbers, interpolated
static inline float pickf(const float* seq, int n, float i) {
    float fl = floorf(i);
    int i0 = ((int)fl % n + n) % n;
    float f = i - fl;
    return seq[i0] + (seq[(i0 + 1) % n] - seq[i0]) * f;
}

// keyframes: list of {time_ms, value}, eased between them, holds the last
struct Key {
    uint32_t t;
    float v;
};
static inline float keys(const Key* k, int n, uint32_t t) {
    if (t <= k[0].t) return k[0].v;
    for (int i = 1; i < n; i++)
        if (t < k[i].t) return lerp(k[i - 1].v, k[i].v, ease(seg(t, k[i - 1].t, k[i].t)));
    return k[n - 1].v;
}
#define KEYS(arr, t) keys(arr, sizeof(arr) / sizeof(arr[0]), t)

// cheap deterministic noise for "random" things that must be pure functions of time
static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}
static inline float hashf(uint32_t x) { return (hash32(x) & 0xffff) / 65535.0f; }  // 0..1
