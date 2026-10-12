// line protocol from the laptop, see firmware/PROTOCOL.md
#include "proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int arg_int(const char* s, const char* key, int def) {
    // finds key=value in "5h=42 wk=18 ..."
    size_t n = strlen(key);
    for (const char* p = s; (p = strstr(p, key)); p += n) {
        if ((p == s || p[-1] == ' ') && p[n] == '=') return atoi(p + n + 1);
    }
    return def;
}

struct EvName {
    const char* name;
    Ev ev;
};

static const EvName EVENTS[] = {
    {"start", EV_START}, {"end", EV_END}, {"prompt", EV_PROMPT}, {"tool", EV_TOOL},
    {"toolfail", EV_TOOL_FAIL}, {"perm", EV_PERM}, {"done", EV_DONE}, {"sub", EV_SUB},
    {"test", EV_TEST_PASS}, {"commit", EV_COMMIT}, {"interrupt", EV_INTERRUPT},
    {"compact", EV_COMPACT},
};

bool proto_line(Brain& b, char* line, uint32_t now, char* reply, int len) {
    // trim
    while (*line == ' ' || *line == '\t') line++;
    char* end = line + strlen(line);
    while (end > line && (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\n')) *--end = 0;
    if (!*line) {
        snprintf(reply, len, "ok");
        return true;
    }

    char* rest = strchr(line, ' ');
    if (rest) *rest++ = 0;
    else rest = end;
    while (*rest == ' ') rest++;

    if (!strcmp(line, "hello") || !strcmp(line, "ping")) {
        snprintf(reply, len, "ok tinyclawd %d", PROTO_VERSION);
        return true;
    }
    if (!strcmp(line, "ev")) {
        char* arg = strchr(rest, ' ');
        if (arg) *arg++ = 0;
        else arg = rest + strlen(rest);
        for (const EvName& e : EVENTS) {
            if (strcmp(rest, e.name)) continue;
            Ev ev = e.ev;
            if (ev == EV_TEST_PASS && !strncmp(arg, "fail", 4)) ev = EV_TEST_FAIL;
            b.event(ev, arg, now);
            snprintf(reply, len, "ok");
            return true;
        }
        snprintf(reply, len, "err unknown event");
        return false;
    }
    if (!strcmp(line, "usage")) {
        b.usage(arg_int(rest, "5h", -1), arg_int(rest, "wk", -1), arg_int(rest, "ctx", -1),
                arg_int(rest, "reset", -1), now);
        snprintf(reply, len, "ok");
        return true;
    }
    if (!strcmp(line, "time")) {
        int h = -1, m = 0;
        if (sscanf(rest, "%d:%d", &h, &m) >= 1 && h >= 0 && h < 24) {
            b.clock(h, m, now);
            snprintf(reply, len, "ok");
            return true;
        }
        snprintf(reply, len, "err bad time");
        return false;
    }
    if (!strcmp(line, "music")) {
        if (*rest != '0' && *rest != '1') {
            snprintf(reply, len, "err want music 0 or music 1 <title>");
            return false;
        }
        bool on = *rest == '1';
        const char* title = rest + 1;
        while (*title == ' ') title++;
        b.music(on, title, now);
        snprintf(reply, len, "ok");
        return true;
    }
    if (!strcmp(line, "touch")) {
        Touch t = TOUCH_TAP;
        if (!strcmp(rest, "double")) t = TOUCH_DOUBLE;
        else if (!strcmp(rest, "long")) t = TOUCH_LONG;
        else if (!strcmp(rest, "down")) t = TOUCH_DOWN;
        else if (!strcmp(rest, "up")) t = TOUCH_UP;
        b.touch(t, now);
        snprintf(reply, len, "ok");
        return true;
    }
    if (!strcmp(line, "play")) {
        if (!find_anim(rest)) {
            snprintf(reply, len, "err no anim");
            return false;
        }
        b.play(rest, now);
        snprintf(reply, len, "ok");
        return true;
    }
    if (!strcmp(line, "tune")) {
        b.tune(!strcmp(rest, "fast"));
        snprintf(reply, len, "ok");
        return true;
    }
    if (!strcmp(line, "now")) {
        snprintf(reply, len, "ok %s", b.current());
        return true;
    }
    snprintf(reply, len, "err unknown");
    return false;
}
