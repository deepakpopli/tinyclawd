// desktop simulator: runs the pet engine and dumps frames for gif.py
//
//   ./sim list                    all animation names
//   ./sim anim blink [ms]         one animation -> out/blink.frames
//   ./sim all                     every animation
//   ./sim script demo.txt         a fake session through the brain -> out/demo.frames
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "anim.h"
#include "brain.h"
#include "proto.h"

static const int FRAME_MS = 20;   // render at 50 fps, gif.py merges equal frames

struct Writer {
    FILE* f = nullptr;
    bool open(const char* name) {
        char path[256];
        snprintf(path, sizeof(path), "out/%s.frames", name);
        f = fopen(path, "wb");
        if (!f) perror(path);
        return f;
    }
    void frame(const Canvas& c) { fwrite(c.buf, 1, sizeof(c.buf), f); }
    void close() { if (f) fclose(f); f = nullptr; }
};

static void render_anim(const Anim* a, uint32_t ms) {
    Writer w;
    if (!w.open(a->name)) return;
    Canvas c;
    Ctx x;
    x.seed = 12345;
    x.subagents = 5;
    x.pct5h = 42;
    x.pctwk = 18;
    x.reset_min = 135;
    x.done = 3;
    x.bugs = 1;
    for (uint32_t t = 0; t < ms; t += FRAME_MS) {
        c.clear();
        a->draw(c, t, x);
        w.frame(c);
    }
    w.close();
    printf("%s %u ms\n", a->name, ms);
}

// script lines: "<ms> <protocol line>" absolute, or "+<ms> <line>" relative.
// "# ..." comments. runs the real brain and records every frame.
static int run_script(const char* path, const char* name, uint32_t tail) {
    FILE* f = fopen(path, "r");
    if (!f) {
        perror(path);
        return 1;
    }
    struct Line {
        uint32_t t;
        char text[128];
    };
    static Line lines[1024];
    int n = 0;
    uint32_t t = 0;
    char buf[160];
    while (fgets(buf, sizeof(buf), f) && n < 1024) {
        char* s = buf;
        while (*s == ' ') s++;
        if (*s == '#' || *s == '\n' || !*s) continue;
        bool rel = *s == '+';
        if (rel) s++;
        char* rest;
        uint32_t v = strtoul(s, &rest, 10);
        t = rel ? t + v : v;
        while (*rest == ' ') rest++;
        rest[strcspn(rest, "\r\n")] = 0;
        lines[n].t = t;
        snprintf(lines[n].text, sizeof(lines[n].text), "%s", rest);
        n++;
    }
    fclose(f);

    Writer w;
    if (!w.open(name)) return 1;
    // captions for gif.py: "<frame> <e|a> <text>"
    char cap_path[256];
    snprintf(cap_path, sizeof(cap_path), "out/%s.captions", name);
    FILE* cap = fopen(cap_path, "w");
    int frame = 0;
    Brain brain;
    Canvas c;
    brain.begin(0, 42);
    uint32_t end = (n ? lines[n - 1].t : 0) + tail;
    int next = 0;
    const char* shown = "";
    for (uint32_t now = 0; now < end; now += FRAME_MS) {
        while (next < n && lines[next].t <= now) {
            char reply[64], tmp[128];
            snprintf(tmp, sizeof(tmp), "%s", lines[next].text);
            proto_line(brain, tmp, now, reply, sizeof(reply));
            printf("%7.2f  > %-28s %s\n", now / 1000.0, lines[next].text, reply);
            if (cap) fprintf(cap, "%d e %s\n", frame, lines[next].text);
            next++;
        }
        brain.render(c, now);
        if (strcmp(shown, brain.current())) {
            shown = brain.current();
            printf("%7.2f    %s\n", now / 1000.0, shown);
            if (cap) fprintf(cap, "%d a %s\n", frame, shown);
        }
        w.frame(c);
        frame++;
    }
    w.close();
    if (cap) fclose(cap);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: sim list | anim <name> [ms] | all | script <file>\n");
        return 1;
    }
    const char* cmd = argv[1];
    if (!strcmp(cmd, "list")) {
        for (int i = 0; i < anim_count(); i++) {
            const Anim* a = anim_at(i);
            printf("%-18s %5u ms%s\n", a->name, a->ms, a->flags & LOOP ? "  loop" : "");
        }
    } else if (!strcmp(cmd, "anim") && argc >= 3) {
        const Anim* a = find_anim(argv[2]);
        if (!a) {
            fprintf(stderr, "no anim %s\n", argv[2]);
            return 1;
        }
        render_anim(a, argc >= 4 ? atoi(argv[3]) : a->ms);
    } else if (!strcmp(cmd, "all")) {
        for (int i = 0; i < anim_count(); i++) render_anim(anim_at(i), anim_at(i)->ms);
    } else if (!strcmp(cmd, "script") && argc >= 3) {
        // ./sim script scripts/demo.txt [name] [tail_ms]
        const char* name = argc >= 4 ? argv[3] : "demo";
        return run_script(argv[2], name, argc >= 5 ? atoi(argv[4]) : 3000);
    } else {
        fprintf(stderr, "unknown command\n");
        return 1;
    }
    return 0;
}
