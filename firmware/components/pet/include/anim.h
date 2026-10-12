#pragma once
#include <stdint.h>

#include "canvas.h"
#include "clawd.h"
#include "face.h"
#include "motion.h"

// what an animation can know about the world
struct Ctx {
    uint32_t seed = 1;      // new random value every play, use it for variations
    int subagents = 0;      // subagents running right now
    int pct5h = -1;         // usage, -1 = unknown
    int pctwk = -1;
    int ctx = -1;           // context window used %
    int reset_min = -1;     // minutes until the 5h window resets
    int done = 0;           // tasks finished this session (scoreboard)
    int bugs = 0;           // failures this session
    const char* tool = "";  // last tool name
    const char* track = ""; // song playing on the laptop ("artist - title"), "" if none
    const char* who = "";   // project waiting for you ("tinyclawd", "2 waiting"), "" if unknown
};

// an animation is a pure function of time: draw(canvas, ms since start, ctx)
typedef void (*DrawFn)(Canvas& c, uint32_t t, const Ctx& x);

enum AnimFlags : uint8_t {
    LOOP = 1,       // fine to keep playing past ms (draw wraps t itself)
    CALM_IN = 2,    // first frame is the plain face
    CALM_OUT = 4,   // last frame is the plain face
    SCENE = 8,      // full screen scene (sprite, board), not the big eyes
};

struct Anim {
    const char* name;
    DrawFn draw;
    uint16_t ms;     // length of one play
    uint8_t flags;
};

// each anims_*.cpp file exports one list, anims.cpp joins them
struct AnimList {
    const Anim* const* items;
    int n;
};
#define ANIM_LIST(name, ...)                                   \
    static const Anim* const name##_items[] = {__VA_ARGS__};  \
    extern const AnimList name;                                \
    const AnimList name = {name##_items, sizeof(name##_items) / sizeof(name##_items[0])}

int anim_count();
const Anim* anim_at(int i);
const Anim* find_anim(const char* name);

#define CALM (CALM_IN | CALM_OUT)
