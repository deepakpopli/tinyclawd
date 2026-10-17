#include "sh1106.h"

#include <string.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"

// 400 kHz is safe. 800 kHz could be tried on the real board (the old code ran
// 1.1 MHz), full frame time drops from ~23 ms to ~12 ms. `hw i2c 800` tries it live.
static const int PIN_SDA = 20;
static const int PIN_SCL = 21;
static const uint8_t ADDR = 0x3C;
static const int XFER_TIMEOUT_MS = 50;
static const uint32_t RETRY_MS = 1000;    // wait this long after an error before trying again

static const char* TAG = "sh1106";

static i2c_master_bus_handle_t s_bus = nullptr;
static i2c_master_dev_handle_t s_dev = nullptr;
static uint8_t s_last[Canvas::PAGES][Canvas::W];
static bool s_ready = false;      // init sequence went through
static bool s_full = true;        // next flush sends every page
static bool s_logged = false;     // only log the first error in a row
static bool s_need_on = false;    // send display-on after the next good flush
static uint32_t s_retry_at = 0;
static sh1106::Conf s_conf;

static uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }

static void fail(const char* what, esp_err_t err) {
    if (!s_logged) {
        ESP_LOGW(TAG, "%s failed: %s", what, esp_err_to_name(err));
        s_logged = true;
    }
    s_ready = false;
    s_retry_at = now_ms() + RETRY_MS;
}

static esp_err_t send(const uint8_t* buf, size_t len) {
    return i2c_master_transmit(s_dev, buf, len, XFER_TIMEOUT_MS);
}

static bool cmds(const uint8_t* c, size_t n) {
    uint8_t buf[32];
    buf[0] = 0x00;  // control byte: command stream
    if (n > sizeof(buf) - 1) return false;
    memcpy(buf + 1, c, n);
    esp_err_t err = send(buf, n + 1);
    if (err != ESP_OK) {
        fail("cmd", err);
        return false;
    }
    return true;
}

static bool make_bus() {
    if (s_bus) return true;
    i2c_master_bus_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = (gpio_num_t)PIN_SDA;
    cfg.scl_io_num = (gpio_num_t)PIN_SCL;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) {
        fail("bus", err);
        s_bus = nullptr;
        return false;
    }
    return true;
}

static bool bus_setup() {
    if (s_dev) return true;
    if (!make_bus()) return false;

    i2c_device_config_t dev_cfg;
    memset(&dev_cfg, 0, sizeof(dev_cfg));
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = ADDR;
    dev_cfg.scl_speed_hz = s_conf.hz;
    esp_err_t err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        fail("add device", err);
        s_dev = nullptr;
        return false;
    }
    // the i2c driver logs every failed transfer, we log once ourselves instead
    esp_log_level_set("i2c.master", ESP_LOG_NONE);
    return true;
}

static bool init_seq() {
    // same sequence as the old mochi oled.c, it is proven on this panel
    const uint8_t seq[] = {
        0xAE,        // display off
        0xD5, 0x80,  // clock divide / osc
        0xA8, 0x3F,  // multiplex 64
        0xD3, 0x00,  // display offset 0
        0x40,        // start line 0
        0xAD, 0x8B,  // dc-dc on
        (uint8_t)(s_conf.mirror != s_conf.flip ? 0xA0 : 0xA1),  // segment remap
        (uint8_t)(s_conf.flip ? 0xC0 : 0xC8),                   // com scan direction
        0xDA, 0x12,  // com pins
        0x81, s_conf.contrast,
        0xD9, 0x22,  // pre-charge
        0xDB, 0x35,  // vcomh
        0xA4,        // show ram
        0xA6,        // not inverted
    };
    if (!cmds(seq, sizeof(seq))) return false;
    s_ready = true;
    s_full = true;
    s_need_on = true;  // turn on only after the ram has a real frame in it
    return true;
}

bool sh1106::init() {
    if (!bus_setup()) return false;
    if (!init_seq()) return false;
    Canvas blank;
    blank.clear();
    flush(blank);
    return s_ready;
}

static bool send_page(int p, const uint8_t* row) {
    const uint8_t addr[] = {
        (uint8_t)(0xB0 | p),
        (uint8_t)(0x00 | (s_conf.offset & 0x0F)),
        (uint8_t)(0x10 | (s_conf.offset >> 4)),
    };
    if (!cmds(addr, sizeof(addr))) return false;

    uint8_t buf[Canvas::W + 1];
    buf[0] = 0x40;  // control byte: data stream
    memcpy(buf + 1, row, Canvas::W);
    esp_err_t err = send(buf, sizeof(buf));
    if (err != ESP_OK) {
        fail("data", err);
        return false;
    }
    return true;
}

int sh1106::flush(const Canvas& c) {
    if (!s_ready) {
        if ((int32_t)(now_ms() - s_retry_at) < 0) return 0;
        if (!bus_setup() || !init_seq()) return 0;
    }

    int sent = 0;
    for (int p = 0; p < Canvas::PAGES; p++) {
        if (!s_full && memcmp(s_last[p], c.buf[p], Canvas::W) == 0) continue;
        if (!send_page(p, c.buf[p])) return sent;  // s_full stays set, all pages go next time
        memcpy(s_last[p], c.buf[p], Canvas::W);
        sent++;
    }
    s_full = false;
    if (s_need_on) {
        const uint8_t on = 0xAF;
        if (!cmds(&on, 1)) return sent;
    }
    s_need_on = false;
    if (s_logged) {
        ESP_LOGW(TAG, "screen back");
        s_logged = false;
    }
    return sent;
}

void sh1106::configure(const Conf& c) {
    bool speed = c.hz != s_conf.hz;
    s_conf = c;
    if (speed && s_dev) {
        // the clock is fixed per device handle, so drop it and add it again
        i2c_master_bus_rm_device(s_dev);
        s_dev = nullptr;
        s_ready = false;
        s_retry_at = 0;
        return;  // next flush runs bus_setup + init_seq with the new settings
    }
    if (s_ready) init_seq();  // offset, flip, mirror, contrast, then a full redraw
}

bool sh1106::ok() { return s_ready; }

int sh1106::scan(uint8_t* found, int max) {
    if (!make_bus()) return 0;
    int n = 0;
    for (uint8_t a = 0x08; a < 0x78 && n < max; a++)
        if (i2c_master_probe(s_bus, a, 20) == ESP_OK) found[n++] = a;
    return n;
}
