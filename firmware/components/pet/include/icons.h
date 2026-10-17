#pragma once
#include <stdint.h>

#include "canvas.h"

// the menu logos. the menu draws a card (52 x 26 inside) centred on cx, cy
// and the logo fits in it. pixel art is written as text: '#' = on, the rest is left alone.
void art(Canvas& c, int x, int y, const char* const* rows, int n);

// logos of the games, one per game
void icon_run(Canvas& c, int cx, int cy, uint32_t now);
void icon_panic(Canvas& c, int cx, int cy, uint32_t now);
void icon_flappy(Canvas& c, int cx, int cy, uint32_t now);
void icon_keepy(Canvas& c, int cx, int cy, uint32_t now);
void icon_penalty(Canvas& c, int cx, int cy, uint32_t now);
void icon_catch(Canvas& c, int cx, int cy, uint32_t now);
void icon_whack(Canvas& c, int cx, int cy, uint32_t now);

// logos of the top menu
void icon_games(Canvas& c, int cx, int cy, uint32_t now);
void icon_usage(Canvas& c, int cx, int cy, uint32_t now);
void icon_back(Canvas& c, int cx, int cy, uint32_t now);
void icon_play(Canvas& c, int cx, int cy, uint32_t now);
