#pragma once
#include <stdint.h>

#include "brain.h"

#define PROTO_VERSION 1

// handle one line from the laptop (no newline). writes the answer into
// reply ("ok", "ok tinyclawd 1", "err ...") and returns true if it knew the line.
bool proto_line(Brain& b, char* line, uint32_t now, char* reply, int reply_len);
