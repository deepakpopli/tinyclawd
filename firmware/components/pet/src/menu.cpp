// the one button menu: pet -> menu items -> games list -> a game
#include "menu.h"

#include <stdio.h>

#include "game.h"

namespace menu {

static const uint32_t RING_SHOW_MS = 250;   // hold this long before the ring shows
static const uint32_t LOVE_MS = 700;        // let go after this = love
static const uint32_t OPEN_MS = 1500;       // ring full = open
static const uint32_t IDLE_MS = 10000;      // untouched menu goes back to the pet

enum State : uint8_t { PET, ITEMS, GAMES, PLAY };

// the top level. add focus / clock / settings here later
enum Item : uint8_t { I_GAMES, I_USAGE, I_BACK };
struct Entry {
    Item id;
    const char* name;
};
static const Entry ITEMS_LIST[] = {{I_GAMES, "games"}, {I_USAGE, "usage"}, {I_BACK, "back"}};
static const int N_ITEMS = sizeof(ITEMS_LIST) / sizeof(ITEMS_LIST[0]);

static const int MAX_GAMES = 8;
static int best[MAX_GAMES];   // best score per game, RAM only (TODO: keep in nvs)

static State state = PET;
static int sel = 0;            // item or game on screen
static const Game* game = nullptr;
static int game_i = 0;
static bool holding = false;
static bool fired = false;     // this hold already opened something
static bool saw_down = false;  // the pad sends down / up, so ignore its long
static uint32_t hold_at = 0;
static uint32_t last_touch = 0;

bool active() { return state != PET; }

const char* where() {
    static char s[32];
    if (state == ITEMS) snprintf(s, sizeof(s), "menu %s", ITEMS_LIST[sel].name);
    else if (state == GAMES) snprintf(s, sizeof(s), "games %s", game_count() ? game_at(sel)->id : "none");
    else if (state == PLAY && game) snprintf(s, sizeof(s), "game %s", game->id);
    else return "pet";
    return s;
}

void close() {
    state = PET;
    game = nullptr;
    holding = false;
}

static void end_game() {
    if (game && game_i < MAX_GAMES) {
        int s = game->score();
        if (s > best[game_i]) best[game_i] = s;
    }
    game = nullptr;
    state = GAMES;
    sel = game_i;
}

static Act open(uint32_t now) {
    switch (state) {
    case PET:
        state = ITEMS;
        sel = 0;
        return USED;
    case ITEMS:
        switch (ITEMS_LIST[sel].id) {
        case I_GAMES:
            state = GAMES;
            sel = 0;
            return USED;
        case I_USAGE: close(); return USAGE;
        case I_BACK: close(); return USED;
        }
        return USED;
    case GAMES:
        if (game_count() == 0) return USED;
        game_i = sel;
        game = game_at(sel);
        game->start(now, now * 2654435761u + 1, game_i < MAX_GAMES ? best[game_i] : 0);
        state = PLAY;
        return USED;
    case PLAY: end_game(); return USED;
    }
    return USED;
}

static void forward(Btn b, uint32_t now) {
    if (state == PLAY && game) game->input(b, now);
}

Act touch(Touch t, uint32_t now) {
    last_touch = now;
    switch (t) {
    case TOUCH_DOWN:
        saw_down = true;
        holding = true;
        fired = false;
        hold_at = now;
        forward(BTN_DOWN, now);
        return USED;
    case TOUCH_UP: {
        bool was = holding && !fired;
        holding = false;
        forward(BTN_UP, now);
        if (state == PET && was && now - hold_at >= LOVE_MS) return LOVE;
        return USED;
    }
    case TOUCH_LONG:
        if (state == PET && !saw_down) return LOVE;   // old scripts without down / up
        return USED;
    case TOUCH_TAP:
        if (state == PET) return PASS;
        if (state == PLAY) forward(BTN_TAP, now);
        else if (state == ITEMS) sel = (sel + 1) % N_ITEMS;
        else if (game_count() > 0) sel = (sel + 1) % game_count();
        return USED;
    case TOUCH_DOUBLE:
        if (state == PET) return PASS;
        if (state == PLAY) forward(BTN_DOUBLE, now);
        else if (state == GAMES) {
            state = ITEMS;
            sel = 0;
        } else close();
        return USED;
    }
    return PASS;
}

Act update(uint32_t now) {
    if (holding && !fired && now - hold_at >= OPEN_MS) {
        fired = true;
        return open(now);
    }
    if ((state == ITEMS || state == GAMES) && !holding && now - last_touch > IDLE_MS) close();
    return USED;
}

void ring(Canvas& c, uint32_t now) {
    if (!holding || fired) return;
    uint32_t held = now - hold_at;
    if (held < RING_SHOW_MS) return;
    float k = (held - RING_SHOW_MS) / (float)(OPEN_MS - RING_SHOW_MS);
    if (k > 1) k = 1;
    // a dark band so the ring reads over any frame
    c.fill_rect(0, 0, 127, 5, false);
    c.fill_rect(0, 58, 127, 63, false);
    c.fill_rect(0, 0, 5, 63, false);
    c.fill_rect(122, 0, 127, 63, false);
    // a 3 px band around the screen edge, clockwise from the top middle.
    // only rectangles: arcs need sin/cos and the C3 has no fpu
    int len = (int)(k * 384);
    int n = len < 64 ? len : 64;   // top, middle to the right
    if (n > 0) c.fill_rect(64, 0, 63 + n, 2);
    len -= 64;
    n = len < 64 ? len : 64;       // right side, down
    if (n > 0) c.fill_rect(125, 0, 127, n - 1);
    len -= 64;
    n = len < 128 ? len : 128;     // bottom, to the left
    if (n > 0) c.fill_rect(128 - n, 61, 127, 63);
    len -= 128;
    n = len < 64 ? len : 64;       // left side, up
    if (n > 0) c.fill_rect(0, 64 - n, 2, 63);
    len -= 64;
    if (len > 0) c.fill_rect(0, 0, len - 1, 2);   // top, left to the middle
}

// ------------------------------------------------------------------ icons

static void icon_games(Canvas& c, int cx, int cy) {
    c.fill_ellipse(cx - 18, cy - 9, cx + 18, cy + 9);
    c.fill_rect(cx - 11, cy - 1, cx - 5, cy + 1, false);   // d-pad
    c.fill_rect(cx - 9, cy - 3, cx - 7, cy + 3, false);
    c.fill_circle(cx + 7, cy + 2, 2, false);               // buttons
    c.fill_circle(cx + 11, cy - 2, 2, false);
}

static void icon_usage(Canvas& c, int cx, int cy) {
    c.hline(cx - 16, cx + 16, cy + 9);
    c.fill_rect(cx - 13, cy + 1, cx - 7, cy + 8);
    c.fill_rect(cx - 3, cy - 5, cx + 3, cy + 8);
    c.fill_rect(cx + 7, cy - 10, cx + 13, cy + 8);
}

static void icon_back(Canvas& c, int cx, int cy) {
    const float tri[] = {(float)cx - 16, (float)cy, (float)cx - 4, (float)cy - 10, (float)cx - 4, (float)cy + 10};
    c.fill_poly(tri, 3);
    c.fill_rect(cx - 4, cy - 3, cx + 14, cy + 3);
}

static void icon_play(Canvas& c, int cx, int cy) {
    c.ellipse(cx - 12, cy - 12, cx + 12, cy + 12, 2);
    const float tri[] = {(float)cx - 4, (float)cy - 7, (float)cx + 8, (float)cy, (float)cx - 4, (float)cy + 7};
    c.fill_poly(tri, 3);
}

// the big fonts only have capitals
static void big_text(Canvas& c, int y, const char* s, const Font& f) {
    char up[24];
    int i = 0;
    for (; s[i] && i < 23; i++) up[i] = s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i];
    up[i] = 0;
    c.text_center(64, y, up, f);
}

static void dots(Canvas& c, int n, int at) {
    int x = 64 - (n - 1) * 4;
    for (int i = 0; i < n; i++, x += 8) {
        if (i == at) c.fill_circle(x, 59, 2);
        else c.ellipse(x - 2, 57, x + 2, 61);
    }
}

void draw(Canvas& c, uint32_t now) {
    if (state == PLAY && game) {
        c.clear();   // games draw on a blank canvas, like in the sim
        bool on = game->draw(c, now);
        // games restart themselves after a game over, so keep the best of every run
        if (game_i < MAX_GAMES && game->score() > best[game_i]) best[game_i] = game->score();
        if (!on) end_game();
        else return;
    }
    c.clear();
    if (state == ITEMS) {
        const Entry& e = ITEMS_LIST[sel];
        if (e.id == I_GAMES) icon_games(c, 64, 17);
        else if (e.id == I_USAGE) icon_usage(c, 64, 17);
        else icon_back(c, 64, 17);
        big_text(c, 34, e.name, font_bold16);
        dots(c, N_ITEMS, sel);
        return;
    }
    // games list
    int n = game_count();
    if (n == 0) {
        big_text(c, 18, "no games", font_bold12);
        c.text_center(64, 36, "double tap: back", font_5x7);
        return;
    }
    const Game* g = game_at(sel);
    icon_play(c, 64, 14);
    big_text(c, 31, g->name, font_bold12);
    if (sel < MAX_GAMES && best[sel] > 0) {
        char line[20];
        snprintf(line, sizeof(line), "best %d", best[sel]);
        c.text_center(64, 45, line, font_5x7);
    }
    dots(c, n, sel);
}

}
