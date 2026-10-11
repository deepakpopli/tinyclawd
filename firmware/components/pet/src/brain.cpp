#include "brain.h"

// placeholder, the real director comes next
struct Brain::Impl {
    const Anim* cur = nullptr;
    uint32_t start = 0;
    Ctx ctx;
};

static Brain::Impl impl;

void Brain::begin(uint32_t now, uint32_t seed) {
    p = &impl;
    p->cur = find_anim("idle");
    p->start = now;
    p->ctx.seed = seed;
}
void Brain::event(Ev, const char*, uint32_t) {}
void Brain::touch(Touch, uint32_t) {}
void Brain::usage(int, int, int, int, uint32_t) {}
void Brain::clock(int, int, uint32_t) {}
void Brain::play(const char* name, uint32_t now) {
    if (const Anim* a = find_anim(name)) {
        p->cur = a;
        p->start = now;
    }
}
void Brain::render(Canvas& c, uint32_t now) {
    c.clear();
    p->cur->draw(c, now - p->start, p->ctx);
}
const char* Brain::current() const { return p->cur->name; }
