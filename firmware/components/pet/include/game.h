#pragma once
#include <stdint.h>

#include "canvas.h"

// one button games. the button is the touch pad on the head.
// react to BTN_DOWN for actions: a tap only arrives after the double tap
// window (~280 ms), too late for a jump.
enum Btn : uint8_t { BTN_DOWN, BTN_UP, BTN_TAP, BTN_DOUBLE };

// holding the pad ~1.5 s leaves the game, the menu layer handles that, games never see it.
// all state lives in the game's own file (statics), only one game runs at a time.
struct Game {
    const char* id;      // "run", short, used by `./sim game run` and the menu
    const char* name;    // "bug run", shown in the menu
    void (*start)(uint32_t now, uint32_t seed, int best);
    void (*input)(Btn b, uint32_t now);
    // draw one frame. return false once the game over screen has been up a while
    // with no input, the menu then takes over again.
    bool (*draw)(Canvas& c, uint32_t now);
    int (*score)();      // current / final score, the menu keeps the best
    // a little bot: true while it would hold the button. the sim uses it for the demo gifs
    bool (*bot)(uint32_t now);
    // the logo in the games menu, fits 52 x 26 px centred on cx, cy (see icons.h).
    // nullptr = the plain play icon
    void (*icon)(Canvas& c, int cx, int cy, uint32_t now);
};

int game_count();
const Game* game_at(int i);
const Game* find_game(const char* id);
