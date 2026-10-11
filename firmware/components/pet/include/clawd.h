#pragma once
#include "canvas.h"

// mini Clawd, built from the block art Claude Code draws in its banner.
// 9 x 3 terminal cells, each cell 4 x 8 px, so 36 x 24 px (scale 0.5 = 18 x 12).
const int SPRITE_W = 36, SPRITE_H = 24;

enum Arms : uint8_t { ARMS_DOWN, ARMS_UP, ARMS_ONE_UP };
enum Eyes : uint8_t { EYES_OPEN, EYES_LEFT, EYES_RIGHT, EYES_CLOSED, EYES_WINK };
enum Feet : uint8_t { FEET_BOTH, FEET_LEFT, FEET_RIGHT, FEET_NONE };

struct Pose {
    Eyes eyes = EYES_OPEN;
    Arms arms = ARMS_DOWN;
    Feet feet = FEET_BOTH;
};

static inline Pose pose(Eyes e = EYES_OPEN, Arms a = ARMS_DOWN, Feet f = FEET_BOTH) {
    Pose p;
    p.eyes = e; p.arms = a; p.feet = f;
    return p;
}

// draw the sprite with its top-left at x, y
void clawd(Canvas& c, int x, int y, const Pose& p = Pose(), float scale = 1);
// same, rotated by deg (counter clockwise, like PIL rotate(expand=True)) around its centre
void clawd_rotated(Canvas& c, float cx, float cy, float deg, const Pose& p = Pose());

// the real 13 step 360 turn from the CLI, k = 0..12
const int TURN_STEPS = 13;
void clawd_turn(Canvas& c, int x, int y, int k, float scale = 1);

// a kicking leg swung forward
void kick_leg(Canvas& c, int x, int y, bool right = true);

// raw rows (utf-8 block characters), for custom poses
void block_rows(Canvas& c, int x, int y, const char* const* rows, int n, float scale = 1, bool on = true);
