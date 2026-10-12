// joins the games. each lives in its own games_*.cpp, a missing one is skipped
#include <string.h>

#include "game.h"

extern const Game game_run __attribute__((weak));
extern const Game game_keepy __attribute__((weak));
extern const Game game_penalty __attribute__((weak));
extern const Game game_panic __attribute__((weak));
extern const Game game_stack __attribute__((weak));

static const Game* all(int i) {
    const Game* list[] = {&game_run, &game_keepy, &game_penalty, &game_panic, &game_stack};
    int n = 0;
    for (const Game* g : list) {
        if (!g) continue;
        if (n++ == i) return g;
    }
    return nullptr;
}

int game_count() {
    int n = 0;
    while (all(n)) n++;
    return n;
}

const Game* game_at(int i) { return all(i); }

const Game* find_game(const char* id) {
    for (int i = 0; const Game* g = all(i); i++)
        if (!strcmp(g->id, id)) return g;
    return nullptr;
}
