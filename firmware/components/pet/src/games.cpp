// joins the games. each lives in its own games_*.cpp
#include <string.h>

#include "game.h"

// plain externs, not weak: the firmware links pet as a library and would
// silently leave weak ones out
extern const Game game_run;
extern const Game game_keepy;
extern const Game game_penalty;
extern const Game game_panic;
extern const Game game_flappy;
extern const Game game_catch;
extern const Game game_whack;

static const Game* const GAMES[] = {&game_run,    &game_keepy, &game_penalty, &game_panic,
                                    &game_flappy, &game_catch, &game_whack};
static const int N_GAMES = sizeof(GAMES) / sizeof(GAMES[0]);

int game_count() { return N_GAMES; }

const Game* game_at(int i) { return i >= 0 && i < N_GAMES ? GAMES[i] : nullptr; }

const Game* find_game(const char* id) {
    for (const Game* g : GAMES)
        if (!strcmp(g->id, id)) return g;
    return nullptr;
}
