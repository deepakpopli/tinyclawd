// the director: decides what Clawd plays next.
//
// there is always a "base" loop for the current mode (idle face, thinking,
// the tool animation, needs you, asleep) and one-shots on top of it
// (celebrations, touch reactions, idle micro behaviours). one-shots come
// from weighted pools with cooldowns and never repeat back to back.
#include "brain.h"
#include "menu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool idle_at_rest(uint32_t t, uint32_t seed);

// ms times wrap after 49 days, so compare them by their difference
static bool before(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }

// ------------------------------------------------------------------- tuning

struct Timing {
    uint32_t bored;      // idle this long -> bored behaviours
    uint32_t drowsy;     // -> yawns, then falls asleep
    uint32_t sleep;
    uint32_t gap_min, gap_max;   // pause between idle micro behaviours
};
static const Timing DAY = {3 * 60000, 12 * 60000, 18 * 60000, 3500, 9000};
static const Timing NIGHT = {60000, 3 * 60000, 5 * 60000, 6000, 14000};
static const Timing FAST = {30000, 150000, 180000, 2500, 5000};   // "tune fast", for testing

static const uint32_t TOOL_MIN_MS = 2500;      // a tool animation stays at least this long
static const uint32_t TOOL_ROTATE_MS = 8000;   // long tool runs switch variant
static const uint32_t WAIT_ROTATE_MS = 9000;
static const uint32_t STALE_MS = 10 * 60000;   // no events for this long while busy = we missed a hook
static const uint32_t SESSION_MS = 15 * 60000;
static const uint32_t TAP_WINDOW = 4000;
static const uint32_t TYPING_STALE_MS = 12000;   // the laptop resends every 5 s while you type
static const uint32_t BREAK_MIN_MS = 25000;      // a little break while claude works, every 25-45 s
static const uint32_t BREAK_MAX_MS = 45000;
static const uint32_t SONG_MIN_MS = 30000;       // music on while claude works: the equalizer
static const uint32_t SONG_MAX_MS = 50000;       // every 30-50 s, for 6 s
static const uint32_t SONG_MS = 6000;
static const uint32_t TYPING_ROTATE_MS = 10000;  // long typing switches what watches you
static const uint32_t GROOVE_ROTATE_MS = 20000;  // a new music visualizer now and then

// -------------------------------------------------------------------- pools

struct Pick {
    const char* name;
    uint8_t w;        // weight
    uint16_t cool;    // seconds before it can play again
};
#define POOL(n) n, sizeof(n) / sizeof(n[0])

// idle micro behaviours
static const Pick AWAKE[] = {
    {"blink", 4, 0}, {"look_around", 6, 12}, {"wink", 3, 40}, {"side_eye", 3, 40},
    {"peek", 3, 50}, {"turn_around", 2, 60}, {"music", 3, 90}, {"ball_watch", 3, 90},
    {"head_juggle", 2, 90}, {"spin", 2, 60}, {"wave", 1, 120}, {"dance", 1, 120},
    {"content", 2, 90}, {"yawn", 1, 120}, {"startled", 1, 180}, {"seq:concert", 1, 300},
    {"seq:training", 1, 300}, {"keepy_uppy", 1, 180}, {"dribble", 1, 180}, {"penalty_save", 1, 240},
    {"usage", 2, 600},   // only once usage data came in (extra_ok)
    {"coffee", 1, 180}, {"stretch", 2, 300}, {"juggle", 2, 240}, {"paper_plane", 1, 180},
};
static const Pick BORED[] = {
    {"bored", 6, 45}, {"yawn", 4, 60}, {"keepy_uppy", 4, 120}, {"dribble", 3, 120},
    {"ball_watch", 3, 90}, {"head_juggle", 3, 90}, {"music", 4, 90}, {"dance", 2, 120},
    {"peek", 3, 50}, {"look_around", 3, 12}, {"side_eye", 2, 40}, {"free_kick", 2, 150},
    {"rainbow_flick", 2, 150}, {"crossbar", 2, 150}, {"header", 2, 150},
    {"bicycle_kick", 1, 200}, {"penalty_save", 2, 150}, {"half_time", 2, 200},
    {"turn_around", 2, 60}, {"spin", 1, 60}, {"blink", 3, 0},
    {"seq:match", 3, 240}, {"seq:training", 2, 240}, {"seq:concert", 2, 300},
    {"seq:nap", 1, 600}, {"usage", 2, 600},
    {"stretch", 2, 300}, {"juggle", 2, 240}, {"stargaze", 2, 300},
};
static const Pick DROWSY[] = {
    {"yawn", 5, 30}, {"bored", 3, 30}, {"blink", 2, 0}, {"side_eye", 1, 60}, {"stargaze", 3, 300},
};
static const Pick ASLEEP[] = {
    {"dream", 5, 60}, {"asleep", 3, 0},
};
// while music plays on the laptop
static const Pick MUSIC[] = {
    {"dance", 4, 60}, {"headbang", 3, 60}, {"now_playing", 3, 120}, {"content", 2, 90},
    {"wink", 1, 40}, {"spin", 1, 90}, {"music", 3, 60},
    {"equalizer", 2, 90}, {"eq_mirror", 2, 90}, {"eq_hills", 2, 90},
};

// celebrations, by how long claude worked on the turn
static const Pick DONE_QUICK[] = {
    {"done", 5, 0}, {"header", 2, 60}, {"wink", 2, 30}, {"content", 2, 30}, {"turn_around", 1, 60},
};
static const Pick DONE_MID[] = {
    {"done", 4, 0}, {"goal", 3, 90}, {"header", 2, 60}, {"party", 1, 90}, {"spin", 1, 60},
    {"wave", 1, 90}, {"paper_plane", 2, 180},
};
static const Pick DONE_LONG[] = {
    {"goal", 4, 60}, {"trophy", 3, 120}, {"bicycle_kick", 3, 120}, {"free_kick", 2, 120},
    {"rainbow_flick", 2, 120}, {"knee_slide", 2, 120}, {"party", 2, 90}, {"done", 1, 0},
};

static const Pick START[] = {{"wave", 3, 0}, {"kickoff", 3, 0}, {"wink", 1, 0}};
static const Pick COMMIT[] = {{"party", 3, 0}, {"knee_slide", 2, 0}, {"trophy", 1, 0}, {"git_branch", 4, 0}};
static const Pick TEST_PASS[] = {{"goal", 3, 0}, {"header", 2, 0}, {"bicycle_kick", 1, 0}, {"bug_squash", 2, 0}};
static const Pick TOOL_FAIL[] = {{"surprised", 3, 20}, {"startled", 2, 20}, {"side_eye", 1, 30}, {"rubber_duck", 3, 120}};
static const Pick INTERRUPT[] = {{"penalty_save", 3, 0}, {"surprised", 2, 0}};
static const Pick END[] = {{"full_time", 3, 0}, {"wave", 1, 0}};
static const Pick PERM_IN[] = {{"surprised", 2, 0}, {"startled", 1, 0}};
static const Pick P_SUBAGENTS_MARCH[] = {{"subagents_march", 1, 0}};
static const Pick P_SUBSTITUTION[] = {{"substitution", 1, 0}};
static const Pick P_SQUAD_RETURN[] = {{"squad_return", 1, 0}};
static const Pick P_ANGRY[] = {{"angry", 2, 0}, {"dizzy", 1, 0}};
static const Pick P_RED_CARD[] = {{"red_card", 1, 0}};
static const Pick P_YELLOW_CARD[] = {{"yellow_card", 2, 0}, {"sad", 1, 0}};
static const Pick P_SQUISH[] = {{"squish", 1, 0}};
static const Pick P_FULL_TIME[] = {{"full_time", 1, 0}};
static const Pick P_LIMIT_HIT[] = {{"limit_hit", 1, 0}};
static const Pick P_STOPPAGE_TIME[] = {{"stoppage_time", 1, 0}};
static const Pick P_TIRED[] = {{"tired", 1, 0}};

// touch
static const Pick TAP1[] = {{"petted", 5, 0}, {"content", 3, 0}, {"shy", 3, 0}, {"wink", 1, 0}};
static const Pick TAP_MORE[] = {{"giggle", 1, 0}};
static const Pick TAP_LOTS[] = {{"laugh", 1, 0}};
static const Pick TAP_TOO_MANY[] = {{"dizzy", 1, 0}};
static const Pick DOUBLE[] = {{"usage", 4, 0}, {"spin", 2, 0}, {"context", 1, 0}};
static const Pick LONG[] = {{"love", 1, 0}};

// what a tool looks like (base loops while it runs)
struct ToolMap {
    const char* prefix;
    const Pick* pool;
    int n;
};
static const Pick T_BASH[] = {{"terminal", 4, 0}, {"compile", 3, 0}, {"working", 2, 0}, {"build_blocks", 2, 0}};
static const Pick T_READ[] = {{"reading", 5, 0}, {"page_flip", 4, 0}, {"searching", 1, 0}};
static const Pick T_SEARCH[] = {{"searching", 5, 0}, {"radar", 4, 0}, {"reading", 1, 0}};
static const Pick T_EDIT[] = {{"typing", 5, 0}, {"build_blocks", 3, 0}, {"working", 1, 0}};
static const Pick T_EDIT_MANY[] = {{"typing", 3, 0}, {"build_blocks", 2, 0}, {"dribble", 2, 0}, {"working", 1, 0}};
static const Pick T_WEB[] = {{"web", 5, 0}, {"satellite", 4, 0}, {"look_around", 1, 0}};
static const Pick T_BUILD[] = {{"compile", 4, 0}, {"crane", 4, 0}, {"working", 2, 0}, {"build_blocks", 3, 0}};
static const Pick T_TEST[] = {{"test_tubes", 5, 0}, {"checklist_run", 4, 0}, {"compile", 2, 0}};
static const Pick T_GIT[] = {{"git_graph", 5, 0}, {"git_branch", 4, 0}};
static const Pick T_INSTALL[] = {{"package", 4, 0}, {"download", 4, 0}, {"plug", 2, 0}};
static const Pick T_TODO[] = {{"todo", 4, 0}, {"checklist_run", 2, 0}};
static const Pick T_MCP[] = {{"plug", 1, 0}};
static const Pick T_OTHER[] = {{"working", 1, 0}};
// Build, Test, Git and Install are bash commands, the bridge names them by what they do
static const ToolMap TOOLS[] = {
    {"Bash", POOL(T_BASH)}, {"BashOutput", POOL(T_BASH)}, {"Read", POOL(T_READ)},
    {"NotebookRead", POOL(T_READ)}, {"Grep", POOL(T_SEARCH)}, {"Glob", POOL(T_SEARCH)},
    {"LS", POOL(T_SEARCH)}, {"Edit", POOL(T_EDIT)}, {"MultiEdit", POOL(T_EDIT)},
    {"Write", POOL(T_EDIT)}, {"NotebookEdit", POOL(T_EDIT)}, {"WebFetch", POOL(T_WEB)},
    {"WebSearch", POOL(T_WEB)}, {"TodoWrite", POOL(T_TODO)}, {"mcp__", POOL(T_MCP)},
    {"Build", POOL(T_BUILD)}, {"Test", POOL(T_TEST)}, {"Git", POOL(T_GIT)}, {"Install", POOL(T_INSTALL)},
};

static const Pick THINK[] = {
    {"thinking_words", 6, 0}, {"thinking", 3, 0}, {"big_spinner", 3, 0}, {"lightbulb", 3, 0},
    {"idea_orbit", 3, 0}, {"ponder", 2, 0}, {"vibe_think", 2, 0},
};
static const Pick WAIT[] = {{"needs_you", 3, 0}, {"var_check", 2, 0}};
static const Pick SUBS[] = {{"subagents", 1, 0}};
static const Pick IDLE[] = {{"idle", 1, 0}};
static const Pick SLEEPING[] = {{"asleep", 1, 0}};
// full screen music, takes turns while the music plays
static const Pick GROOVE[] = {{"equalizer", 3, 0}, {"eq_mirror", 3, 0}, {"eq_hills", 3, 0}};
static const Pick NOW_PLAYING[] = {{"now_playing", 1, 0}};
static const Pick YOU_TYPING[] = {
    {"you_typing", 4, 0}, {"typing_along", 3, 0}, {"letters_fly", 3, 0}, {"keyboard_peek", 3, 0},
};
// little breaks while claude works, then back to the work
static const Pick WORK_BREAK[] = {
    {"coffee", 3, 120}, {"stretch", 2, 180}, {"lightbulb", 2, 60}, {"look_around", 3, 30},
    {"wink", 2, 60}, {"juggle", 2, 180}, {"side_eye", 2, 60}, {"content", 2, 90},
    {"paper_plane", 1, 180}, {"peek", 2, 60},
};

// little stories, played back to back. a|b = pick one
static const char* const SEQS[][2] = {
    {"match", "kickoff dribble goal|crossbar|free_kick knee_slide|trophy|scoreboard"},
    {"training", "keepy_uppy head_juggle header|bicycle_kick|rainbow_flick"},
    {"concert", "music dance content"},
    {"nap", "yawn sleepy dream wake_up"},
};

// ------------------------------------------------------------------- state

enum Mode : uint8_t { IDLE_M, THINK_M, TOOL_M, WAIT_M, SLEEP_M };
enum Trans : uint8_t { CUT, LIDS, DISSOLVE };

static const int MAX_ANIMS = 192;
static const int QLEN = 6;

struct Play {
    const Anim* a = nullptr;
    uint32_t start = 0;
    Ctx ctx;
};

struct Brain::Impl {
    uint32_t rng = 1;
    Mode mode = IDLE_M;
    Play cur, prev;
    Play shown;                 // what was on screen at the last render
    bool oneshot = false;       // cur is a one-shot (returns to base when done)
    Trans trans = CUT;
    uint32_t trans_start = 0;
    const Anim* queue[QLEN];
    int qn = 0;
    uint32_t last_play[MAX_ANIMS];
    const Anim* recent[2] = {nullptr, nullptr};

    // world
    const Timing* timing = &DAY;
    bool fast = false;
    int hour = -1;
    uint32_t hour_at = 0;
    uint32_t last_activity = 0;    // events or touch
    uint32_t last_event = 0;
    bool session = false;
    uint32_t next_micro = 0;
    uint32_t base_since = 0;       // when the current base pool started
    const Pick* base_pool = IDLE;
    int base_n = 1;
    const Anim* base = nullptr;
    char tool[24] = "";
    const Pick* pending_tool = nullptr;   // tool switch waiting for TOOL_MIN_MS
    int pending_n = 0;
    uint32_t prompt_at = 0;
    int edits_this_turn = 0;
    int fails_in_row = 0;
    int subagents = 0;
    int taps = 0;
    uint32_t last_tap = 0;
    int pct5h = -1, pctwk = -1, ctxpct = -1, reset_min = -1;
    bool warned90 = false;
    int done = 0, bugs = 0;
    bool music = false;            // the laptop is playing music
    char track[49] = "";
    uint8_t eq[8] = {};            // last music levels from the laptop
    uint32_t eq_at = 0;            // when they came, 0 = never
    char who[17] = "";             // project waiting for you
    int sessions = 0;              // busy sessions, dots when 2+
    bool resume = false;           // last line was "ev done small", others still work
    int typing = 0;                // you type on the laptop, keys per second
    uint32_t next_break = 0;       // next little break while claude works, 0 = not working
    bool on_break = false;         // cur is such a break, a new tool waits for it to end
    uint32_t next_song = 0;        // next equalizer break while claude works and music plays
    uint32_t until = 0;            // a one-shot that ends early, 0 = when the animation ends
    uint32_t typing_at = 0;        // last typing line
    Canvas a, b;   // scratch for transitions

    uint32_t rand() {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    }
    int index_of(const Anim* an) {
        for (int i = 0; i < anim_count() && i < MAX_ANIMS; i++)
            if (anim_at(i) == an) return i;
        return -1;
    }
    bool cooling(const Pick& p, const Anim* an, uint32_t now) {
        int i = index_of(an);
        if (i < 0 || !last_play[i] || !p.cool) return false;
        return now - last_play[i] < (uint32_t)p.cool * 1000;
    }

    // weighted pick from a pool. skips missing animations, the last two
    // played and anything still cooling down. returns a name (may be "seq:x").
    const char* pick(const Pick* pool, int n, uint32_t now) {
        int total = 0;
        for (int pass = 0; pass < 2 && !total; pass++) {
            for (int i = 0; i < n; i++) total += usable(pool[i], now, pass) ? pool[i].w : 0;
            if (total) {
                int r = rand() % total;
                for (int i = 0; i < n; i++) {
                    if (!usable(pool[i], now, pass)) continue;
                    r -= pool[i].w;
                    if (r < 0) return pool[i].name;
                }
            }
        }
        return nullptr;
    }
    // pass 0 is strict, pass 1 ignores repeats and cooldowns
    bool usable(const Pick& p, uint32_t now, int pass) {
        if (!strncmp(p.name, "seq:", 4)) return pass == 1 || !seq_cooling(p, now);
        const Anim* an = find_anim(p.name);
        if (!an || !extra_ok(an)) return false;
        if (pass == 1) return true;
        if (an == recent[0] || an == recent[1]) return false;
        return !cooling(p, an, now);
    }
    // some animations only make sense with data
    bool extra_ok(const Anim* an) {
        if (!strcmp(an->name, "usage")) return pct5h >= 0 || pctwk >= 0;
        if (!strcmp(an->name, "context")) return ctxpct >= 0;
        if (!strcmp(an->name, "scoreboard")) return done + bugs > 0;
        return true;
    }
    uint32_t seq_last[4] = {0, 0, 0, 0};
    int seq_index(const char* name) {
        for (int i = 0; i < 4; i++)
            if (!strcmp(SEQS[i][0], name)) return i;
        return -1;
    }
    bool seq_cooling(const Pick& p, uint32_t now) {
        int i = seq_index(p.name + 4);
        return i < 0 || (seq_last[i] && now - seq_last[i] < (uint32_t)p.cool * 1000);
    }

    // -------------------------------------------------------------- playing

    void fill_ctx(Ctx& c) {
        c.seed = rand();
        c.subagents = subagents;
        c.pct5h = pct5h;
        c.pctwk = pctwk;
        c.ctx = ctxpct;
        c.reset_min = reset_min;
        c.done = done;
        c.bugs = bugs;
        c.tool = tool;
        c.track = track;
        c.who = who;
        c.typing = typing;
    }

    void mark(const Anim* an, uint32_t now) {
        int i = index_of(an);
        if (i >= 0) last_play[i] = now ? now : 1;
        if (an != recent[0]) {
            recent[1] = recent[0];
            recent[0] = an;
        }
    }

    // switch what is on screen, choosing a transition that does not jump.
    // decisions use what is really on screen, so two switches in one tick
    // still blend from the old frame.
    void show(const Anim* an, uint32_t now, bool is_oneshot) {
        if (!an) return;
        const Play& on = shown.a ? shown : cur;
        Trans t = LIDS;
        bool on_finished = on.a && now - on.start >= on.a->ms;
        bool calm_end = on.a && (on.a->flags & CALM_OUT) && on_finished;
        bool on_idle = on.a && !strcmp(on.a->name, "idle") && idle_at_rest(now - on.start, on.ctx.seed);
        if ((calm_end || on_idle) && (an->flags & CALM_IN)) t = CUT;
        else if (on.a && sleepish(on.a) && sleepish(an)) t = CUT;
        else if (on.a && !strcmp(on.a->name, "you_typing") && !strcmp(an->name, "typing_ready")) t = CUT;
        else if ((on.a && (on.a->flags & SCENE)) || (an->flags & SCENE)) t = DISSOLVE;
        if (!on.a) t = CUT;
        if (t != CUT) {
            prev = on;
            trans_start = now;
        }
        trans = t;
        cur.a = an;
        cur.start = now;
        fill_ctx(cur.ctx);
        oneshot = is_oneshot;
        on_break = false;
        until = 0;
        mark(an, now);
    }

    // closed-eye animations flow into each other without a blink
    static bool sleepish(const Anim* an) {
        static const char* const names[] = {"sleepy", "asleep", "dream", "wake_up", "grumpy_wake",
                                            "typing_wake"};
        for (const char* n : names)
            if (!strcmp(an->name, n)) return true;
        return false;
    }

    bool cur_finished(uint32_t now) { return cur.a && now - cur.start >= cur.a->ms; }

    void play_name(const char* name, uint32_t now) {
        if (!name) return;
        if (!strncmp(name, "seq:", 4)) {
            start_seq(name + 4, now);
            return;
        }
        show(find_anim(name), now, true);
    }

    void oneshot_from(const Pick* pool, int n, uint32_t now) {
        qn = 0;
        play_name(pick(pool, n, now), now);
    }

    void start_seq(const char* name, uint32_t now) {
        int i = seq_index(name);
        if (i < 0) return;
        seq_last[i] = now ? now : 1;
        char buf[128];
        strncpy(buf, SEQS[i][1], sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        qn = 0;
        for (char* tok = strtok(buf, " "); tok && qn < QLEN; tok = strtok(nullptr, " ")) {
            // a|b|c: choose one
            const char* opts[4];
            int no = 0;
            for (char* o = tok; o && no < 4;) {
                opts[no++] = o;
                char* bar = strchr(o, '|');
                if (bar) *bar++ = 0;
                o = bar;
            }
            const char* chosen = opts[rand() % no];
            // a goal wins the match, a crossbar does not get a knee slide
            if (qn && queue[qn - 1] && !strcmp(queue[qn - 1]->name, "crossbar") &&
                (!strcmp(chosen, "knee_slide") || !strcmp(chosen, "trophy")))
                chosen = "sad";
            if (const Anim* an = find_anim(chosen)) queue[qn++] = an;
        }
        if (qn) next_from_queue(now);
    }

    void next_from_queue(uint32_t now) {
        const Anim* an = queue[0];
        for (int i = 1; i < qn; i++) queue[i - 1] = queue[i];
        qn--;
        show(an, now, true);
    }

    // ----------------------------------------------------------- the base

    void set_base(const Pick* pool, int n, uint32_t now, bool force = false) {
        if (pool == base_pool && !force) return;
        base_pool = pool;
        base_n = n;
        base_since = now;
        const char* name = pick(pool, n, now);
        base = find_anim(name ? name : "idle");
        if (!oneshot) show(base, now, false);
    }

    // the idle base: watching you type, a visualizer while music plays, or the plain face
    void idle_base(uint32_t now, bool force = false) {
        if (typing) set_base(POOL(YOU_TYPING), now, force);
        else if (music) set_base(POOL(GROOVE), now, force);
        else set_base(IDLE, 1, now, force);
    }

    void go_mode(Mode m, uint32_t now) {
        mode = m;
        switch (m) {
        case IDLE_M:
            idle_base(now);
            next_micro = now + gap();
            break;
        case THINK_M:
            if (subagents) set_base(SUBS, 1, now);
            else set_base(POOL(THINK), now);
            break;
        case WAIT_M: set_base(WAIT, 2, now); break;
        case SLEEP_M: set_base(SLEEPING, 1, now); next_micro = now + 20000 + rand() % 20000; break;
        case TOOL_M: break;
        }
    }

    uint32_t gap() {
        uint32_t g = timing->gap_min + rand() % (timing->gap_max - timing->gap_min);
        return music ? 2 * g : g;   // the music loop is fun to watch on its own
    }

    bool dozing() { return mode == SLEEP_M || (cur.a && (!strcmp(cur.a->name, "sleepy") || !strcmp(cur.a->name, "dream"))); }

    void wake(uint32_t now) {
        // asleep or dozing: open the eyes first
        if (dozing()) {
            mode = IDLE_M;
            base_pool = nullptr;
            idle_base(now);
            show(find_anim("wake_up"), now, true);
        }
    }

    // queue an event reaction behind a wake up if one is playing
    void react(const Pick* pool, int n, uint32_t now) {
        const char* name = pick(pool, n, now);
        if (oneshot && cur.a && (!strcmp(cur.a->name, "wake_up") || !strcmp(cur.a->name, "grumpy_wake"))) {
            const Anim* an = name ? find_anim(name) : nullptr;
            qn = 0;
            if (an) queue[qn++] = an;
            return;
        }
        qn = 0;
        play_name(name, now);
    }

    const Pick* tool_pool(const char* name, int& n) {
        bool many_edits = edits_this_turn >= 4;
        for (const ToolMap& t : TOOLS) {
            if (strncmp(name, t.prefix, strlen(t.prefix))) continue;
            if (t.pool == T_EDIT && many_edits) {
                n = sizeof(T_EDIT_MANY) / sizeof(Pick);
                return T_EDIT_MANY;
            }
            n = t.n;
            return t.pool;
        }
        n = 1;
        return T_OTHER;
    }

    void activity(uint32_t now) {
        last_activity = now;
        next_micro = now + gap();
    }

    // ------------------------------------------------------------ typing

    void set_typing(int rate, uint32_t now) {
        rate = rate < 0 ? 0 : (rate > 15 ? 15 : rate);
        int was = typing;
        typing = rate;
        typing_at = now;
        cur.ctx.typing = rate;   // the keyboard speeds up right away
        if (!rate) {
            if (was) typing_stopped(now);
            return;
        }
        activity(now);
        if (!was) typing_started(now);
    }

    void typing_started(uint32_t now) {
        if (dozing()) {
            // woken by typing: soft, no grumpy face
            mode = IDLE_M;
            base_pool = nullptr;
            oneshot = false;
            qn = 0;
            idle_base(now);
            show(find_anim("typing_wake"), now, true);
            return;
        }
        if (mode != IDLE_M) return;   // claude is busy, typing does not change that
        // stop an idle micro behaviour, but let event reactions and wake ups finish
        bool reaction = cur.a && now - cur.start < 3000 && cur.start - last_event < 300;
        bool waking = cur.a && (!strcmp(cur.a->name, "wake_up") || !strcmp(cur.a->name, "grumpy_wake") ||
                                !strcmp(cur.a->name, "typing_wake"));
        if (oneshot && !reaction && !waking) {
            oneshot = false;
            qn = 0;
        }
        idle_base(now, true);
    }

    void typing_stopped(uint32_t now) {
        if (mode != IDLE_M) return;
        bool watching = !oneshot && base_pool == YOU_TYPING;
        idle_base(now, true);
        if (watching) show(find_anim("typing_ready"), now, true);   // "ready?"
        next_micro = now + gap();
    }

    // ------------------------------------------------------------- tick

    void update(uint32_t now) {
        // stale: we probably missed the Stop hook
        if ((mode == THINK_M || mode == TOOL_M) && now - last_event > STALE_MS) go_mode(IDLE_M, now);
        if (mode == WAIT_M && now - last_event > 3 * STALE_MS) go_mode(IDLE_M, now);
        if (session && now - last_event > SESSION_MS) session = false;
        if (typing && now - typing_at > TYPING_STALE_MS) set_typing(0, now);   // the laptop went away

        // night is a bit sleepier
        int h = hour_now(now);
        bool night = h >= 0 && (h >= 23 || h < 7);
        timing = fast ? &FAST : (night ? &NIGHT : &DAY);

        // pending tool switch after the minimum time
        if (pending_tool && mode == TOOL_M && !subagents && now - base_since >= TOOL_MIN_MS) {
            const Pick* p = pending_tool;
            pending_tool = nullptr;
            set_base(p, pending_n, now, true);
        }

        // one-shot finished: next in the queue, or back to the base
        if (oneshot && (cur_finished(now) || (until && !before(now, until)))) {
            if (qn) next_from_queue(now);
            else {
                oneshot = false;
                if (mode == SLEEP_M && cur.a && !strcmp(cur.a->name, "sleepy")) base = find_anim("asleep");
                if (mode == IDLE_M && base_pool == GROOVE) {   // a fresh visualizer, not the same one again
                    const char* name = pick(POOL(GROOVE), now);
                    if (name) base = find_anim(name);
                }
                show(base, now, false);
                if (mode == IDLE_M) next_micro = now + gap();
            }
        }

        // long tool runs, waits and typing rotate between variants
        uint32_t every = rotate_every();
        if (!oneshot && every && base_n > 1 && cur.a) {
            const char* name = now - cur.start > every ? pick(base_pool, base_n, now) : nullptr;
            if (name && strcmp(name, cur.a->name)) {
                base = find_anim(name);
                show(base, now, false);
            }
        }

        work_breaks(now);
        if (mode == IDLE_M && !oneshot) idle_tick(now);
        if (mode == SLEEP_M && !oneshot && !before(now, next_micro)) {
            // dreams now and then
            play_name(pick(ASLEEP, sizeof(ASLEEP) / sizeof(Pick), now), now);
            next_micro = now + 25000 + rand() % 35000;
        }
    }

    void idle_tick(uint32_t now) {
        if (typing) {   // you are right here typing: no micro behaviours, no sleep
            last_activity = now;
            next_micro = now + gap();
            return;
        }
        uint32_t quiet = music ? 0 : now - last_activity;   // music is company
        // usage limit reached: show it now and then
        if (pct5h >= 100 && !before(now, next_micro) && rand() % 3 == 0) {
            play_name("limit_hit", now);
            next_micro = now + gap();
            return;
        }
        if (quiet > timing->sleep && !waiting_for_you()) {
            mode = SLEEP_M;
            base = find_anim("asleep");
            base_pool = SLEEPING;
            show(find_anim("sleepy"), now, true);
            next_micro = now + 30000;
            return;
        }
        if (before(now, next_micro)) return;
        // start micro behaviours only on a calm idle frame so the cut is invisible
        if (cur.a && !strcmp(cur.a->name, "idle") && !idle_at_rest(now - cur.start, cur.ctx.seed) &&
            before(now, next_micro + 600))
            return;
        const char* name;
        if (music) name = pick(MUSIC, sizeof(MUSIC) / sizeof(Pick), now);
        else if (quiet > timing->drowsy) name = pick(DROWSY, sizeof(DROWSY) / sizeof(Pick), now);
        else if (quiet > timing->bored) name = pick(BORED, sizeof(BORED) / sizeof(Pick), now);
        else name = pick(AWAKE, sizeof(AWAKE) / sizeof(Pick), now);
        // tired eyes win sometimes when usage is high
        if (pct5h >= 80 && pct5h < 100 && rand() % 4 == 0) name = "tired";
        if (session && done + bugs > 0 && rand() % 12 == 0) name = "scoreboard";
        play_name(name, now);
        next_micro = now + gap();
    }

    bool waiting_for_you() { return mode == WAIT_M; }

    uint32_t rotate_every() {
        if (mode == WAIT_M) return WAIT_ROTATE_MS;
        if (mode == TOOL_M || mode == THINK_M) return TOOL_ROTATE_MS;
        if (mode == IDLE_M && base_pool == YOU_TYPING) return TYPING_ROTATE_MS;
        if (mode == IDLE_M && base_pool == GROOVE) return GROOVE_ROTATE_MS;
        return 0;
    }

    uint32_t span(uint32_t lo, uint32_t hi) { return lo + rand() % (hi - lo); }

    // claude works on its own: no squad on screen, not waiting for you
    bool working() { return (mode == THINK_M || mode == TOOL_M) && !subagents; }

    // a long stretch of work gets a little break now and then
    void work_breaks(uint32_t now) {
        if (!working()) {
            next_break = next_song = 0;
            return;
        }
        if (!next_break) next_break = now + span(BREAK_MIN_MS, BREAK_MAX_MS);
        if (!music) next_song = 0;
        else if (!next_song) next_song = now + span(8000, 15000);   // first one soon
        if (oneshot) return;
        if (next_song && !before(now, next_song)) {
            // listening along for a moment, then back to work
            next_song = now + span(SONG_MIN_MS, SONG_MAX_MS);
            if (before(next_break, now + 12000)) next_break = now + 12000;
            const char* name = pick(POOL(GROOVE), now);
            show(find_anim(name ? name : "equalizer"), now, true);
            on_break = true;
            until = now + SONG_MS;
            return;
        }
        if (before(now, next_break)) return;
        if (next_song && before(next_song, now + 12000)) next_song = now + 12000;
        next_break = now + span(BREAK_MIN_MS, BREAK_MAX_MS);
        const char* name = pick(POOL(WORK_BREAK), now);
        if (!name) return;
        play_name(name, now);
        on_break = true;
    }

    int hour_now(uint32_t now) {
        if (hour < 0) return -1;
        return (hour + (int)((now - hour_at) / 3600000)) % 24;
    }

    // ------------------------------------------------------------- draw

    void draw_play(Canvas& c, Play& p, uint32_t now) {
        c.clear();
        if (!p.a) return;
        uint32_t t = now - p.start;
        if (!(p.a->flags & LOOP) && t >= p.a->ms) t = p.a->ms ? p.a->ms - 1 : 0;
        memcpy(p.ctx.eq, eq, sizeof(eq));   // levels change while it plays
        p.ctx.eq_live = eq_at && now - eq_at < 500;
        p.a->draw(c, t, p.ctx);
    }

    void render(Canvas& out, uint32_t now) {
        const uint32_t LID_MS = 260, DIS_MS = 240;
        uint32_t tt = now - trans_start;
        if (trans == LIDS && tt < LID_MS) {
            // a blink of the whole screen: close over the old frame, open on the new
            bool closing = tt < LID_MS / 2;
            draw_play(out, closing ? prev : cur, now);
            float k = closing ? ease(tt / (LID_MS / 2.0f)) : 1 - ease((tt - LID_MS / 2) / (LID_MS / 2.0f));
            int bar = iround(32 * k);
            if (bar > 0) {
                out.fill_rect(0, 0, 127, bar - 1, false);
                out.fill_rect(0, 64 - bar, 127, 63, false);
            }
            return;
        }
        if (trans == DISSOLVE && tt < DIS_MS) {
            static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
            draw_play(a, prev, now);
            draw_play(b, cur, now);
            int level = iround(16 * ease(tt / (float)DIS_MS));
            for (int y = 0; y < 64; y++)
                for (int x = 0; x < 128; x++)
                    out.px(x, y, BAYER[y & 3][x & 3] < level ? b.get(x, y) : a.get(x, y));
            return;
        }
        trans = CUT;
        draw_play(out, cur, now);
    }

    // a dot per busy session in the top right corner while working
    void session_dots(Canvas& c) {
        if (sessions < 2 || (mode != THINK_M && mode != TOOL_M)) return;
        int n = sessions > 4 ? 4 : sessions;
        c.fill_rect(127 - n * 5, 0, 127, 5, false);
        for (int i = 0; i < n; i++) c.fill_rect(124 - i * 5, 1, 126 - i * 5, 3);
    }

    // a note in the top left corner while music plays and claude works
    void music_sign(Canvas& c, uint32_t now) {
        if (!music || (mode != THINK_M && mode != TOOL_M)) return;
        for (const Pick& g : GROOVE)   // the visualizers are the music already
            if (cur.a && !strcmp(cur.a->name, g.name)) return;
        int y = (now / 500) & 1;   // bobs along
        c.fill_rect(0, 0, 9, 11, false);
        c.fill_rect(1, 7 + y, 3, 9 + y);   // head
        c.vline(4, 1 + y, 9 + y);          // stem
        c.px(5, 2 + y);                    // flag
        c.px(6, 3 + y);
        c.px(7, 4 + y);
        c.px(7, 5 + y);
    }

    // you type while claude works: a small keyboard in the bottom right corner
    void typing_sign(Canvas& c, uint32_t now) {
        if (!typing || (mode != THINK_M && mode != TOOL_M)) return;
        c.fill_rect(112, 55, 127, 63, false);
        c.hline(114, 126, 57);
        c.hline(114, 126, 63);
        c.vline(114, 57, 63);
        c.vline(126, 57, 63);
        uint32_t step = now / (700 / (typing + 2) + 60);   // faster typing, faster keys
        for (int k = 0; k < 3; k++) {
            int x = 117 + k * 3;
            if ((step * 7 + k * 3) % 5 < 2) c.fill_rect(x, 59, x + 1, 61);   // pressed
            else c.px(x, 60);
        }
    }

    void render_and_remember(Canvas& out, uint32_t now) {
        render(out, now);
        shown = cur;
    }
};

static Brain::Impl impl;

void Brain::begin(uint32_t now, uint32_t seed) {
    p = &impl;
    p->rng = seed ? seed : 0x1234567;
    memset(p->last_play, 0, sizeof(p->last_play));
    p->last_activity = p->last_event = now;
    p->base_pool = nullptr;
    p->go_mode(IDLE_M, now);
    p->show(find_anim("wake_up"), now, true);
}

void Brain::event(Ev e, const char* arg, uint32_t now) {
    Impl& s = *p;
    bool resume = s.resume;   // this line is the work another session still does
    s.resume = false;
    s.last_event = now;
    s.activity(now);
    if (e != EV_END) s.session = true;
    s.wake(now);
    if (e == EV_PERM) menu::close();   // needs you beats the menu

    switch (e) {
    case EV_START:
        s.done = s.bugs = 0;
        s.go_mode(IDLE_M, now);
        s.react(POOL(START), now);
        break;
    case EV_END:
        s.session = false;
        s.subagents = 0;
        s.go_mode(IDLE_M, now);
        s.react(POOL(END), now);
        break;
    case EV_PROMPT:
        if (resume) {   // keep the small party and the turn clock
            if (!s.prompt_at) s.prompt_at = now;
            s.go_mode(THINK_M, now);
            break;
        }
        s.prompt_at = now;
        s.edits_this_turn = 0;
        s.fails_in_row = 0;
        s.qn = 0;
        if (s.oneshot && s.cur.a && strcmp(s.cur.a->name, "wake_up")) s.oneshot = false;
        s.go_mode(THINK_M, now);
        break;
    case EV_TOOL: {
        strncpy(s.tool, arg, sizeof(s.tool) - 1);
        s.tool[sizeof(s.tool) - 1] = 0;
        if (!strncmp(arg, "Edit", 4) || !strncmp(arg, "Write", 5) || !strncmp(arg, "MultiEdit", 9)) s.edits_this_turn++;
        if (s.subagents) {   // the squad keeps playing, their tools would flicker
            if (s.mode == WAIT_M) {
                s.mode = TOOL_M;
                s.set_base(SUBS, 1, now, true);
            }
            break;
        }
        if (!strcmp(arg, "Task") || !strcmp(arg, "Agent")) break;   // EV_SUB handles these
        int n;
        const Pick* pool = s.tool_pool(arg, n);
        if (s.mode != TOOL_M) {
            s.mode = TOOL_M;
            s.pending_tool = nullptr;
            if (s.oneshot && s.cur.a && (resume || s.on_break || !strcmp(s.cur.a->name, "wake_up"))) {
                s.base_pool = pool;
                s.base_n = n;
                s.base_since = now;
                const char* name = s.pick(pool, n, now);
                s.base = find_anim(name ? name : "idle");
            } else {
                s.oneshot = false;
                s.qn = 0;
                s.set_base(pool, n, now, true);
            }
        } else if (pool != s.base_pool) {
            // different kind of tool: switch, but not faster than TOOL_MIN_MS
            if (now - s.base_since >= TOOL_MIN_MS) s.set_base(pool, n, now, true);
            else {
                s.pending_tool = pool;
                s.pending_n = n;
            }
        } else {
            s.pending_tool = nullptr;
        }
        break;
    }
    case EV_TOOL_FAIL:
        s.fails_in_row++;
        if (s.fails_in_row >= 3) s.react(POOL(TAP_TOO_MANY), now);   // dizzy
        else if (s.rand() % 2) s.react(POOL(TOOL_FAIL), now);
        break;
    case EV_PERM:
        // "+2" = two sessions waiting, else a project name
        if (*arg == '+') snprintf(s.who, sizeof(s.who), "%d waiting", atoi(arg + 1));
        else {
            strncpy(s.who, arg, sizeof(s.who) - 1);
            s.who[sizeof(s.who) - 1] = 0;
        }
        if (s.mode == WAIT_M) break;   // already waiting, only the name changed
        s.qn = 0;
        s.oneshot = false;
        s.go_mode(WAIT_M, now);
        if (s.rand() % 2) s.react(POOL(PERM_IN), now);
        break;
    case EV_DONE: {
        if (!strcmp(arg, "small")) {   // one session done, others still work
            s.done++;
            s.react(POOL(DONE_QUICK), now);
            s.resume = true;
            break;
        }
        uint32_t took = s.prompt_at ? now - s.prompt_at : 0;
        s.done++;
        s.subagents = 0;
        s.go_mode(IDLE_M, now);
        if (took > 120000) s.react(POOL(DONE_LONG), now);
        else if (took > 20000) s.react(POOL(DONE_MID), now);
        else s.react(POOL(DONE_QUICK), now);
        s.prompt_at = 0;
        break;
    }
    case EV_SUB: {
        int n = 0;
        sscanf(arg, "%d", &n);
        if (n < 0) n = 0;
        int was = s.subagents;
        s.subagents = n;
        s.cur.ctx.subagents = n;
        if (n > 0 && was == 0) {
            if (s.mode == IDLE_M || s.mode == SLEEP_M) s.mode = TOOL_M;
            s.set_base(SUBS, 1, now, true);
            s.react(POOL(P_SUBAGENTS_MARCH), now);
        } else if (n > was) {
            s.react(POOL(P_SUBSTITUTION), now);   // another one joins the squad
        } else if (n == 0 && was > 0) {
            if (s.mode == TOOL_M || s.mode == THINK_M) {
                s.mode = THINK_M;
                s.set_base(POOL(THINK), now, true);
            } else if (s.mode == IDLE_M) {
                s.idle_base(now, true);
            }
            s.react(POOL(P_SQUAD_RETURN), now);
        }
        break;
    }
    case EV_TEST_PASS:
        s.fails_in_row = 0;
        s.react(POOL(TEST_PASS), now);
        break;
    case EV_TEST_FAIL:
        s.bugs++;
        s.fails_in_row++;
        if (s.fails_in_row >= 3) s.react(POOL(P_ANGRY), now);
        else if (s.fails_in_row == 2) s.react(POOL(P_RED_CARD), now);
        else s.react(POOL(P_YELLOW_CARD), now);
        break;
    case EV_COMMIT:
        s.react(POOL(COMMIT), now);
        break;
    case EV_INTERRUPT:
        s.go_mode(IDLE_M, now);
        s.react(POOL(INTERRUPT), now);
        break;
    case EV_COMPACT:
        s.react(POOL(P_SQUISH), now);
        break;
    }
}

void Brain::touch(Touch t, uint32_t now) {
    Impl& s = *p;
    s.activity(now);
    menu::Act a = menu::touch(t, now);
    if (a == menu::USED) return;
    if (a == menu::LOVE) t = TOUCH_LONG;
    if (s.mode == SLEEP_M) {
        // woken up by a poke: grumpy
        s.mode = IDLE_M;
        s.base_pool = nullptr;
        s.idle_base(now);
        s.qn = 0;
        s.play_name(find_anim("grumpy_wake") ? "grumpy_wake" : "wake_up", now);
        return;
    }
    switch (t) {
    case TOUCH_TAP:
        s.taps = now - s.last_tap < TAP_WINDOW ? s.taps + 1 : 1;
        s.last_tap = now;
        if (s.taps >= 6) s.oneshot_from(POOL(TAP_TOO_MANY), now);
        else if (s.taps >= 4) s.oneshot_from(POOL(TAP_LOTS), now);
        else if (s.taps >= 2) s.oneshot_from(POOL(TAP_MORE), now);
        else s.oneshot_from(POOL(TAP1), now);
        break;
    case TOUCH_DOUBLE: s.oneshot_from(POOL(DOUBLE), now); break;
    case TOUCH_LONG: s.oneshot_from(POOL(LONG), now); break;
    case TOUCH_DOWN:
    case TOUCH_UP: break;
    }
}

void Brain::usage(int pct5h, int pctwk, int ctx, int reset_min, uint32_t now) {
    Impl& s = *p;
    int was = s.pct5h;
    if (pct5h >= 0) s.pct5h = pct5h;
    if (pctwk >= 0) s.pctwk = pctwk;
    if (ctx >= 0) s.ctxpct = ctx;
    if (reset_min >= 0) s.reset_min = reset_min;
    if (pct5h < 0) return;
    // the window reset: full time whistle
    if (was >= 50 && pct5h < 10) s.react(POOL(P_FULL_TIME), now);
    else if (pct5h >= 100 && was < 100) s.react(POOL(P_LIMIT_HIT), now);
    else if (pct5h >= 90 && !s.warned90) {
        s.warned90 = true;
        s.react(POOL(P_STOPPAGE_TIME), now);
    } else if (pct5h >= 80 && was >= 0 && was < 80) s.react(POOL(P_TIRED), now);
    if (pct5h < 90) s.warned90 = false;
}

void Brain::music(bool on, const char* title, uint32_t now) {
    Impl& s = *p;
    bool was = s.music;
    if (on && was && !strncmp(s.track, title, sizeof(s.track) - 1)) return;   // same song again
    if (!on && !was) return;
    s.music = on;
    strncpy(s.track, on ? title : "", sizeof(s.track) - 1);
    s.track[sizeof(s.track) - 1] = 0;
    s.activity(now);   // also restarts the idle clock when it stops
    if (!on) {
        if (s.mode == IDLE_M) s.idle_base(now);
        return;
    }
    s.wake(now);
    if (s.mode != IDLE_M || s.typing) return;   // claude is busy or you type, the title can wait
    s.react(POOL(NOW_PLAYING), now);
    s.idle_base(now);
}

void Brain::typing(int rate, uint32_t now) { p->set_typing(rate, now); }

void Brain::eq(const uint8_t* levels, uint32_t now) {
    memcpy(p->eq, levels, sizeof(p->eq));
    p->eq_at = now ? now : 1;
}

void Brain::clock(int hour, int minute, uint32_t now) {
    p->hour = hour;
    p->hour_at = now - minute * 60000u;
}

void Brain::sessions(int n) { p->sessions = n < 0 ? 0 : n; }

void Brain::tune(bool fast) { p->fast = fast; }

void Brain::play(const char* name, uint32_t now) {
    p->qn = 0;
    p->play_name(name, now);
}

void Brain::render(Canvas& c, uint32_t now) {
    if (menu::update(now) == menu::USAGE) play("usage", now);
    p->update(now);
    if (menu::active()) menu::draw(c, now);
    else {
        p->render_and_remember(c, now);
        p->session_dots(c);
        p->music_sign(c, now);
        p->typing_sign(c, now);
    }
    menu::ring(c, now);
}

const char* Brain::current() const {
    if (menu::active()) return menu::where();
    return p->cur.a ? p->cur.a->name : "none";
}
