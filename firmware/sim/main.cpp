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
    } else {
        fprintf(stderr, "unknown command\n");
        return 1;
    }
    return 0;
}
