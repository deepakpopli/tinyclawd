// desktop simulator: runs the pet engine and dumps frames for gif.py
//
//   ./sim list                    all animation names
//   ./sim anim blink [ms]         one animation -> out/blink.frames
//   ./sim all                     every animation
//   ./sim script demo.txt         a fake session through the brain -> out/demo.frames
//   ./sim soak [hours]            a long fake day, prints what played how often
//   ./sim games                   all game ids
//   ./sim game run [ms]           a game played by its bot -> out/game_run.frames
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "anim.h"
#include "brain.h"
#include "game.h"
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
    // SEED=7 SUBS=3 ./sim anim goal, to check the variations
    x.seed = getenv("SEED") ? strtoul(getenv("SEED"), nullptr, 10) : 12345;
    x.subagents = getenv("SUBS") ? atoi(getenv("SUBS")) : 5;
    // TYPING=12 ./sim anim you_typing, keys per second
    x.typing = getenv("TYPING") ? atoi(getenv("TYPING")) : 0;
    x.pct5h = 42;
    x.pctwk = 18;
    x.ctx = 64;
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

// the game's bot holds the button, short presses also send a tap
static void render_game(const Game* g, uint32_t ms) {
    char name[64];
    snprintf(name, sizeof(name), "game_%s", g->id);
    Writer w;
    if (!w.open(name)) return;
    Canvas c;
    uint32_t seed = getenv("SEED") ? strtoul(getenv("SEED"), nullptr, 10) : 12345;
    g->start(0, seed, 12);
    bool held = false;
    uint32_t down_at = 0;
    uint32_t t = 0;
    for (; t < ms; t += FRAME_MS) {
        bool b = g->bot(t);
        if (b != held) {
            held = b;
            g->input(b ? BTN_DOWN : BTN_UP, t);
            if (b) down_at = t;
            else if (t - down_at < 300) g->input(BTN_TAP, t);
        }
        c.clear();
        if (!g->draw(c, t)) break;
        w.frame(c);
    }
    w.close();
    printf("%s %u ms, score %d\n", g->name, t, g->score());
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

// a long fake day: random sessions, idle gaps, touches. no frames, just stats
// so we can see if it really keeps changing. ./sim soak [hours]
static int soak(int hours) {
    Brain brain;
    Canvas c;
    uint32_t rng = 99;
    auto rnd = [&](uint32_t n) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return rng % n;
    };
    static int plays[256], work_plays[256];
    static uint32_t screen_ms[256], work_ms[256];
    brain.begin(0, 7);
    char reply[64], line[64];
    auto send = [&](const char* l, uint32_t now) {
        snprintf(line, sizeof(line), "%s", l);
        proto_line(brain, line, now, reply, sizeof(reply));
    };
    // what the bridge sends: real tools plus the kinds of bash command
    const char* tools[] = {"Bash", "Read", "Grep", "Glob", "Edit", "Write", "WebFetch", "TodoWrite", "mcp__x__y",
                           "Bash", "Grep", "Read", "Build", "Test", "Git", "Install", "Edit", "Bash"};
    const int ntools = sizeof(tools) / sizeof(tools[0]);
    uint32_t end = hours * 3600000u, now = 0, next_ev = 5000;
    int state = 0, steps_left = 0, turns_left = 0;
    const char* shown = "";
    int changes = 0, work_changes = 0;
    uint32_t work_total = 0;
    send("time 09:00", 0);
    while (now < end) {
        if (now >= next_ev) {
            if (state == 0) {   // idle gap over: start a session
                send("ev start", now);
                state = 1;
                turns_left = 3 + rnd(6);
                next_ev = now + 3000 + rnd(20000);
            } else if (state == 1) {   // a prompt
                send("ev prompt", now);
                state = 2;
                steps_left = 2 + rnd(12);
                next_ev = now + 2000 + rnd(8000);
            } else if (state == 2) {   // tools
                char l[64];
                int r = rnd(100);
                if (r < 6) send("ev perm", now);
                else if (r < 9) { snprintf(l, sizeof(l), "ev sub %d", 1 + (int)rnd(5)); send(l, now); }
                else if (r < 12) send("ev sub 0", now);
                else if (r < 15) send(rnd(3) ? "ev test pass" : "ev test fail", now);
                else if (r < 17) send("ev commit", now);
                else if (r < 19) send("ev toolfail Bash", now);
                else { snprintf(l, sizeof(l), "ev tool %s", tools[rnd(ntools)]); send(l, now); }
                next_ev = now + 800 + rnd(9000);
                if (--steps_left <= 0) state = 3;
            } else if (state == 3) {   // turn done
                send("ev done", now);
                state = --turns_left > 0 ? 1 : 4;
                next_ev = now + 10000 + rnd(120000);
            } else {   // session over, long idle gap
                send("ev end", now);
                state = 0;
                next_ev = now + 60000 + rnd(50 * 60000);
            }
        }
        if (rnd(3000) == 0) send(rnd(4) ? "touch tap" : (rnd(2) ? "touch double" : "touch long"), now);
        brain.render(c, now);
        const char* cur = brain.current();
        const Anim* a = find_anim(cur);
        int idx = -1;
        for (int i = 0; i < anim_count(); i++)
            if (anim_at(i) == a) idx = i;
        bool work = state == 2 || state == 3;   // between a prompt and done
        if (strcmp(cur, shown)) {
            shown = cur;
            changes++;
            if (idx >= 0) plays[idx]++;
            if (idx >= 0 && work) work_plays[idx]++, work_changes++;
        }
        if (idx >= 0) screen_ms[idx] += FRAME_MS * 5;
        if (idx >= 0 && work) work_ms[idx] += FRAME_MS * 5, work_total += FRAME_MS * 5;
        now += FRAME_MS * 5;   // 10 fps is enough for stats
    }
    printf("%d hours, %d changes (one every %.1f s)\n", hours, changes, end / 1000.0 / changes);
    // while claude works: how many different ones, and the biggest share
    int distinct = 0, top = 0;
    for (int i = 0; i < anim_count(); i++) {
        if (work_plays[i]) distinct++;
        if (work_ms[i] > work_ms[top]) top = i;
    }
    printf("work: %.0f min, %d changes (one every %.1f s), %d different, top %s %.1f%%\n",
           work_total / 60000.0, work_changes, work_total / 1000.0 / (work_changes ? work_changes : 1),
           distinct, anim_at(top)->name, 100.0 * work_ms[top] / (work_total ? work_total : 1));
    printf("%-18s %6s %8s %6s %8s\n", "anim", "plays", "screen%", "work", "work%");
    for (int i = 0; i < anim_count(); i++)
        printf("%-18s %6d %7.1f%% %6d %7.1f%%\n", anim_at(i)->name, plays[i], 100.0 * screen_ms[i] / end,
               work_plays[i], 100.0 * work_ms[i] / (work_total ? work_total : 1));
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: sim list | anim <name> [ms] | all | script <file> | soak [hours] | games | game <id> [ms]\n");
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
    } else if (!strcmp(cmd, "games")) {
        for (int i = 0; i < game_count(); i++) printf("%-10s %s\n", game_at(i)->id, game_at(i)->name);
    } else if (!strcmp(cmd, "game") && argc >= 3) {
        const Game* g = find_game(argv[2]);
        if (!g) {
            fprintf(stderr, "no game %s\n", argv[2]);
            return 1;
        }
        render_game(g, argc >= 4 ? atoi(argv[3]) : 30000);
    } else if (!strcmp(cmd, "soak")) {
        return soak(argc >= 3 ? atoi(argv[2]) : 12);
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
