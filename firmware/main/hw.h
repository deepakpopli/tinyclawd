#pragma once
#include <stdint.h>

#include "canvas.h"

// bring-up and tuning over serial: "hw ..." lines (see PROTOCOL.md).
// screen offset, flip, contrast, i2c speed and touch timings change live,
// `hw save` keeps them in nvs so the next boot uses them.
namespace hw {

void load();      // nvs -> settings, applies them. call before sh1106::init
bool line(char* line, char* reply, int len);   // false if it was not a hw line

int test();       // 0 = normal pet, else the test pattern to draw
void draw_test(Canvas& c, uint32_t now);

// the render task reports every frame, `hw stats` prints the averages
void frame(uint32_t draw_us, uint32_t flush_us, int pages);

// game high scores: loaded at boot, saved when one changes
void scores_tick(uint32_t now);

}
