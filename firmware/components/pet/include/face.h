#pragma once
#include "canvas.h"

// Clawd's eyes: solid pixel slits, no pupils
const int EYE_W = 12, EYE_H = 22;
const int LEFT_X = 42, RIGHT_X = 86, EYE_Y = 32;

enum Shape : uint8_t { RECT, HALF, HAPPY, SQUINT, CROSS, HEART, SAD, ANGRY, SPIRAL };

struct Eye {
    float dx = 0, dy = 0;
    float w = EYE_W, h = EYE_H;
    Shape shape = RECT;
    float lid = 0;      // 0 open .. 1 closed (top lid comes down)
    bool mirror = false;  // right eye
    float phase = 0;    // spiral rotation
};

void draw_eye(Canvas& c, float cx, float cy, const Eye& e);

// both eyes at the normal spots
void face(Canvas& c, const Eye& l, const Eye& r);
// same pose on both eyes (right one mirrored)
void face(Canvas& c, const Eye& e);
static inline void face(Canvas& c) { face(c, Eye()); }

// eye with fields set, for one-liners: eye(dx, dy) / eye(dx, dy, w, h)
static inline Eye eye(float dx = 0, float dy = 0, float w = EYE_W, float h = EYE_H, Shape s = RECT, float lid = 0) {
    Eye e;
    e.dx = dx; e.dy = dy; e.w = w; e.h = h; e.shape = s; e.lid = lid;
    return e;
}

// blink with squash (eyes get wider as they close), k = 0..1 of the blink.
// returns the eye to draw. base is the pose being blinked from.
Eye blink_eye(const Eye& base, float k);
// a full blink lasts this long
const uint32_t BLINK_MS = 245;
// draw a neutral face that blinks once at t in [at, at + BLINK_MS)
void face_blink(Canvas& c, uint32_t t, uint32_t at, const Eye& base = Eye());

// a slow 1 px "breathing" bob so held poses never look frozen
float breathe(uint32_t t, uint32_t period = 1400);

// little shapes that show up in many animations
void heart(Canvas& c, float x, float y, float k = 1);
void sparkle(Canvas& c, int x, int y, int r = 3);
void sweat(Canvas& c, float x, float y);
void tear(Canvas& c, float x, float y);
void ball(Canvas& c, float x, float y, float r = 3);
void zzz(Canvas& c, float t);          // rising z z z, t loops 0..1
void stars(Canvas& c, float x, float y, float t);  // dizzy stars circling
