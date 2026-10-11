#include <string.h>

#include "anim.h"

extern const AnimList anims_claude, anims_idle, anims_work, anims_moods;
extern const AnimList anims_usage, anims_football, anims_match, anims_squad;

static const AnimList* const LISTS[] = {
    &anims_claude, &anims_idle, &anims_work, &anims_moods,
    &anims_usage, &anims_football, &anims_match, &anims_squad,
};
static const int NLISTS = sizeof(LISTS) / sizeof(LISTS[0]);

int anim_count() {
    int n = 0;
    for (int i = 0; i < NLISTS; i++) n += LISTS[i]->n;
    return n;
}

const Anim* anim_at(int i) {
    for (int k = 0; k < NLISTS; k++) {
        if (i < LISTS[k]->n) return LISTS[k]->items[i];
        i -= LISTS[k]->n;
    }
    return nullptr;
}

const Anim* find_anim(const char* name) {
    for (int k = 0; k < NLISTS; k++)
        for (int i = 0; i < LISTS[k]->n; i++)
            if (!strcmp(LISTS[k]->items[i]->name, name)) return LISTS[k]->items[i];
    return nullptr;
}
