#include "hw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "game.h"
#include "menu.h"
#include "sh1106.h"
#include "touch.h"

static const char* TAG = "hw";
static const uint8_t SAVE_VERSION = 1;

struct Settings {
    sh1106::Conf screen;
    uint32_t double_ms = 280;
    uint32_t long_ms = 700;
};

// what goes into nvs, kept flat so it does not change with the structs above
struct Saved {
    uint8_t version, offset, flip, mirror, contrast;
    uint16_t khz, double_ms, long_ms;
};

static Settings s_set;
static int s_saved_best[8];   // what is in nvs, so we only write on a change
static int s_test = 0;

static struct {
    uint32_t frames, draw_sum, draw_max, flush_sum, flush_max, pages;
    int64_t since;
} s_stats;

static void apply() {
    sh1106::configure(s_set.screen);
    touch::set_times(s_set.double_ms, s_set.long_ms);
}

void hw::load() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    nvs_handle_t h;
    Saved sv;
    size_t n = sizeof(sv);
    if (err == ESP_OK && nvs_open("hw", NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_blob(h, "set", &sv, &n) == ESP_OK && n == sizeof(sv) && sv.version == SAVE_VERSION) {
            s_set.screen.offset = sv.offset;
            s_set.screen.flip = sv.flip;
            s_set.screen.mirror = sv.mirror;
            s_set.screen.contrast = sv.contrast;
            s_set.screen.hz = sv.khz * 1000;
            s_set.double_ms = sv.double_ms;
            s_set.long_ms = sv.long_ms;
        }
        nvs_close(h);
    }
    apply();
    s_stats.since = esp_timer_get_time();

    // best scores, one key per game id ("b_run")
    if (err == ESP_OK && nvs_open("scores", NVS_READONLY, &h) == ESP_OK) {
        for (int i = 0; i < game_count() && i < 8; i++) {
            char key[16];
            snprintf(key, sizeof(key), "b_%s", game_at(i)->id);
            int32_t v = 0;
            if (nvs_get_i32(h, key, &v) == ESP_OK) {
                menu::set_best(i, v);
                s_saved_best[i] = v;
            }
        }
        nvs_close(h);
    }
}

void hw::scores_tick(uint32_t now) {
    static uint32_t next = 0;
    if ((int32_t)(now - next) < 0) return;
    next = now + 3000;   // a quick look every few seconds is plenty
    nvs_handle_t h = 0;
    for (int i = 0; i < game_count() && i < 8; i++) {
        int v = menu::best_of(i);
        if (v == s_saved_best[i]) continue;
        if (!h && nvs_open("scores", NVS_READWRITE, &h) != ESP_OK) return;
        char key[16];
        snprintf(key, sizeof(key), "b_%s", game_at(i)->id);
        if (nvs_set_i32(h, key, v) == ESP_OK) s_saved_best[i] = v;
    }
    if (h) {
        nvs_commit(h);
        nvs_close(h);
    }
}

static bool save() {
    Saved sv = {SAVE_VERSION, s_set.screen.offset, s_set.screen.flip, s_set.screen.mirror,
                s_set.screen.contrast, (uint16_t)(s_set.screen.hz / 1000),
                (uint16_t)s_set.double_ms, (uint16_t)s_set.long_ms};
    nvs_handle_t h;
    if (nvs_open("hw", NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = nvs_set_blob(h, "set", &sv, sizeof(sv)) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    if (!ok) ESP_LOGW(TAG, "save failed");
    return ok;
}

static void show(char* reply, int len, bool restarting = false) {
    const sh1106::Conf& c = s_set.screen;
    snprintf(reply, len, "ok offset=%d flip=%d mirror=%d contrast=%d i2c=%lu double=%lu long=%lu test=%d screen=%s",
             c.offset, c.flip, c.mirror, c.contrast, (unsigned long)(c.hz / 1000),
             (unsigned long)s_set.double_ms, (unsigned long)s_set.long_ms, s_test,
             sh1106::ok() ? "ok" : (restarting ? "restarting" : "missing"));
}

static void stats(char* reply, int len) {
    int64_t now = esp_timer_get_time();
    uint32_t f = s_stats.frames ? s_stats.frames : 1;
    float secs = (now - s_stats.since) / 1e6f;
    snprintf(reply, len, "ok fps=%.1f draw=%lu/%lu flush=%lu/%lu us avg/max pages=%.1f",
             secs > 0 ? s_stats.frames / secs : 0.0f,
             (unsigned long)(s_stats.draw_sum / f), (unsigned long)s_stats.draw_max,
             (unsigned long)(s_stats.flush_sum / f), (unsigned long)s_stats.flush_max,
             (float)s_stats.pages / f);
    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.since = now;
}

void hw::frame(uint32_t draw_us, uint32_t flush_us, int pages) {
    s_stats.frames++;
    s_stats.draw_sum += draw_us;
    s_stats.flush_sum += flush_us;
    s_stats.pages += pages;
    if (draw_us > s_stats.draw_max) s_stats.draw_max = draw_us;
    if (flush_us > s_stats.flush_max) s_stats.flush_max = flush_us;
}

// reads "<n>" and checks the range, writes the error itself
static bool num(const char* s, long lo, long hi, long* out, char* reply, int len) {
    char* end;
    long v = strtol(s, &end, 10);
    if (end == s || *end || v < lo || v > hi) {
        snprintf(reply, len, "err want %ld..%ld", lo, hi);
        return false;
    }
    *out = v;
    return true;
}

bool hw::line(char* line, char* reply, int len) {
    if (strncmp(line, "hw", 2) || (line[2] && line[2] != ' ')) return false;
    char* end = line + strlen(line);
    while (end > line && end[-1] == ' ') *--end = 0;
    char* key = line + 2;
    while (*key == ' ') key++;
    char* arg = strchr(key, ' ');
    if (arg) {
        *arg++ = 0;
        while (*arg == ' ') arg++;
    } else {
        arg = key + strlen(key);
    }
    sh1106::Conf& c = s_set.screen;
    long v;

    if (!*key) {
        show(reply, len);
    } else if (!strcmp(key, "stats")) {
        stats(reply, len);
    } else if (!strcmp(key, "save")) {
        snprintf(reply, len, save() ? "ok saved" : "err save failed");
    } else if (!strcmp(key, "defaults")) {
        s_set = Settings();
        apply();
        snprintf(reply, len, "ok defaults (not saved)");
    } else if (!strcmp(key, "scan")) {
        uint8_t found[8];
        int n = sh1106::scan(found, 8);
        int w = snprintf(reply, len, "ok found %d", n);
        for (int i = 0; i < n && w < len; i++) w += snprintf(reply + w, len - w, " 0x%02X", found[i]);
    } else if (!strcmp(key, "test")) {
        if (!num(arg, 0, 3, &v, reply, len)) return true;
        s_test = v;
        snprintf(reply, len, "ok");
    } else if (!strcmp(key, "touchraw")) {
        if (!num(arg, 0, 1, &v, reply, len)) return true;
        touch::set_raw_log(v);
        snprintf(reply, len, "ok");
    } else {
        // settings: change, apply, show them all again
        if (!strcmp(key, "offset")) {
            if (!num(arg, 0, 4, &v, reply, len)) return true;
            c.offset = v;
        } else if (!strcmp(key, "flip")) {
            if (!num(arg, 0, 1, &v, reply, len)) return true;
            c.flip = v;
        } else if (!strcmp(key, "mirror")) {
            if (!num(arg, 0, 1, &v, reply, len)) return true;
            c.mirror = v;
        } else if (!strcmp(key, "contrast")) {
            if (!num(arg, 0, 255, &v, reply, len)) return true;
            c.contrast = v;
        } else if (!strcmp(key, "i2c")) {
            if (!num(arg, 100, 1000, &v, reply, len)) return true;
            c.hz = v * 1000;
        } else if (!strcmp(key, "double")) {
            if (!num(arg, 100, 1000, &v, reply, len)) return true;
            s_set.double_ms = v;
        } else if (!strcmp(key, "long")) {
            if (!num(arg, 300, 3000, &v, reply, len)) return true;
            s_set.long_ms = v;
        } else {
            snprintf(reply, len, "err unknown hw");
            return true;
        }
        apply();
        // a new i2c speed reconnects the screen on the next frame
        show(reply, len, !strcmp(key, "i2c"));
    }
    return true;
}

void hw::draw_test(Canvas& c, uint32_t now) {
    c.clear();
    if (s_test == 2) {          // every pixel on: dead pixels, brightness
        c.fill();
        return;
    }
    if (s_test == 3) {          // checker that swaps every half second: worst case flush
        bool odd = (now / 500) & 1;
        for (int y = 0; y < Canvas::H; y++)
            for (int x = 0; x < Canvas::W; x++)
                if (((x ^ y) & 1) == odd) c.px(x, y);
        return;
    }
    // 1: a 1 px border must sit exactly on the edge, corners say where they are
    c.rect(0, 0, Canvas::W - 1, Canvas::H - 1);
    c.text(3, 3, "TL");
    c.text(Canvas::W - 3 - c.text_width("TR"), 3, "TR");
    c.text(3, Canvas::H - 10, "BL");
    c.text(Canvas::W - 3 - c.text_width("BR"), Canvas::H - 10, "BR");
    // an arrow pointing up, so a flipped screen is obvious
    const float arrow[] = {64, 10, 56, 20, 61, 20, 61, 30, 67, 30, 67, 20, 72, 20};
    c.fill_poly(arrow, 7);
    char s[24];
    snprintf(s, sizeof(s), "offset %d", s_set.screen.offset);
    c.text_center(64, 36, s);
    snprintf(s, sizeof(s), "i2c %lu", (unsigned long)(s_set.screen.hz / 1000));
    c.text_center(64, 46, s);
}

int hw::test() { return s_test; }
