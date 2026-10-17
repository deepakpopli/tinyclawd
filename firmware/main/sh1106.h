#pragma once
#include <stdint.h>

#include "canvas.h"

// SH1106 128x64 over I2C. only pages that changed get sent.
// never blocks for long and never crashes: if the screen is missing or
// the bus glitches it logs once and tries again a bit later.
namespace sh1106 {

struct Conf {
    uint8_t offset = 2;           // ram is 132 wide, the panel sits at +2
    bool flip = false;            // turn the picture 180 degrees
    bool mirror = false;          // swap left and right
    uint8_t contrast = 0xCF;
    uint32_t hz = 400000;         // i2c clock
};

void configure(const Conf& c);    // call before init, or any time later (applies live)
bool init();                      // false if the screen did not answer (will retry in flush)
int flush(const Canvas& c);       // returns how many pages went out
bool ok();                        // last transfer worked
int scan(uint8_t* found, int max);  // addresses that answer on the bus

}
