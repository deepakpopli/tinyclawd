#pragma once
#include <stdint.h>

#include "brain.h"
#include "canvas.h"

// the one button menu on top of the pet.
// hold the pad: a ring fills. let go early = love, hold till it is full = menu.
// in the menu: tap = next, hold = open, double tap = back to the pet.
namespace menu {

enum Act : uint8_t {
    PASS,    // not ours, the pet handles the touch
    USED,    // the menu took it
    LOVE,    // a short hold on the pet
    USAGE,   // the usage item was picked
};

Act touch(Touch t, uint32_t now);
Act update(uint32_t now);              // holds finish here, call once per frame
bool active();                         // true while the menu or a game owns the screen
void draw(Canvas& c, uint32_t now);    // full screen, only while active()
void ring(Canvas& c, uint32_t now);    // the hold ring, drawn over whatever is on screen
void close();                          // claude needs you: back to the pet
const char* where();                   // "menu usage", "games keepy", "game run" (for `now`)

}
