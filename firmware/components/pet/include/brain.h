#pragma once
#include <stdint.h>

#include "anim.h"

// things that happen in a claude code session (sent by the bridge)
enum Ev : uint8_t {
    EV_START,      // session started
    EV_END,        // session ended
    EV_PROMPT,     // you sent a prompt, claude is thinking
    EV_TOOL,       // a tool started, arg = tool name (Bash, Read, Grep...)
    EV_TOOL_FAIL,  // a tool came back with an error
    EV_PERM,       // claude is waiting for you (permission / question)
    EV_DONE,       // claude finished its turn
    EV_SUB,        // subagents running, arg = count
    EV_TEST_PASS,  // a test command passed
    EV_TEST_FAIL,  // a test command failed
    EV_COMMIT,     // git commit went through
    EV_INTERRUPT,  // you pressed esc
    EV_COMPACT,    // context got compacted
};

// down / up are the raw pad edges, the brain times holds from them
enum Touch : uint8_t { TOUCH_TAP, TOUCH_DOUBLE, TOUCH_LONG, TOUCH_DOWN, TOUCH_UP };

// picks what to play and draws it. not thread safe: call everything from one task.
class Brain {
public:
    void begin(uint32_t now, uint32_t seed);
    void event(Ev e, const char* arg, uint32_t now);
    void touch(Touch t, uint32_t now);
    void usage(int pct5h, int pctwk, int ctx, int reset_min, uint32_t now);
    void music(bool on, const char* title, uint32_t now);   // title = "artist - title", may be empty
    void typing(int rate, uint32_t now);         // you type on the laptop, keys per second, 0 = stopped
    void eq(const uint8_t* levels, uint32_t now);   // 8 music levels 0..9, bass first
    void clock(int hour, int minute, uint32_t now);
    void sessions(int n);                        // claude code sessions busy right now
    void play(const char* name, uint32_t now);   // debug: play one animation now
    void tune(bool fast);                        // debug: short idle timings (bored in 30 s)
    void render(Canvas& c, uint32_t now);
    const char* current() const;                 // name of what is playing

    struct Impl;

private:
    Impl* p = nullptr;
};
