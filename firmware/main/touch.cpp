#include "touch.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/task.h"

static const gpio_num_t PIN = GPIO_NUM_1;  // high while touched
static const uint32_t POLL_MS = 10;
static const uint32_t DEBOUNCE_MS = 30;
static const uint32_t DOUBLE_MS = 280;      // gap allowed between two taps
static const uint32_t LONG_MS = 700;

static QueueHandle_t s_q;

static void emit(Touch t) {
    uint8_t v = t;
    xQueueSend(s_q, &v, 0);  // drop it if the render task is way behind
}

static void touch_task(void*) {
    bool raw_last = false;
    bool down = false;          // debounced state
    uint32_t changed = 0;       // when raw last changed
    uint32_t pressed_at = 0;
    bool long_sent = false;
    bool tap_waiting = false;   // one short tap done, waiting to see if a second comes
    uint32_t released_at = 0;

    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(POLL_MS));
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        bool raw = gpio_get_level(PIN) == 1;

        if (raw != raw_last) {
            raw_last = raw;
            changed = now;
        }

        if (raw != down && now - changed >= DEBOUNCE_MS) {
            down = raw;
            if (down) {
                pressed_at = now;
                long_sent = false;
            } else if (!long_sent) {
                if (tap_waiting) {
                    tap_waiting = false;
                    emit(TOUCH_DOUBLE);
                } else {
                    tap_waiting = true;
                    released_at = now;
                }
            }
        }

        if (down && !long_sent && now - pressed_at >= LONG_MS) {
            long_sent = true;
            tap_waiting = false;  // tap then long press counts as just the long press
            emit(TOUCH_LONG);
        }

        // single tap only once the double tap window is over.
        // raw high means a second touch is starting, give it time to debounce.
        if (tap_waiting && !down && !raw && now - released_at > DOUBLE_MS) {
            tap_waiting = false;
            emit(TOUCH_TAP);
        }
    }
}

void touch::start(QueueHandle_t events) {
    s_q = events;
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << PIN;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;  // reads untouched if the sensor is unplugged
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
    xTaskCreate(touch_task, "touch", 2048, nullptr, 6, nullptr);
}

const char* touch::name(Touch t) {
    switch (t) {
    case TOUCH_TAP: return "tap";
    case TOUCH_DOUBLE: return "double";
    case TOUCH_LONG: return "long";
    }
    return "?";
}
