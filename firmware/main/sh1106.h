#pragma once
#include <stdint.h>

#include "canvas.h"

// SH1106 128x64 over I2C. only pages that changed get sent.
// never blocks for long and never crashes: if the screen is missing or
// the bus glitches it logs once and tries again a bit later.
namespace sh1106 {

bool init();                      // false if the screen did not answer (will retry in flush)
void flush(const Canvas& c);
void set_contrast(uint8_t v);     // 0..255, 0xCF at boot
void set_on(bool on);
bool ok();                        // last transfer worked

}
