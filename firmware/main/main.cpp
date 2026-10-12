#include <stdio.h>
#include <string.h>

#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "brain.h"
#include "canvas.h"
#include "hw.h"
#include "link.h"
#include "proto.h"
#include "sh1106.h"
#include "touch.h"

// tasks:
//   render (prio 5)  owns the Brain, 40 fps, drains both queues, draws, flushes
//   touch  (prio 6)  polls the pad every 10 ms, pushes Touch events
//   link   (prio 4)  reads usb serial, pushes complete lines
// wifi / ota go here later (own task, talks to render through a queue too)

static const uint32_t FRAME_MS = 25;

static QueueHandle_t s_lines;
static QueueHandle_t s_touches;

static Brain s_brain;   // only the render task touches it
static Canvas s_canvas;

static uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }

// "hw shot": the last frame as 32 lines of hex, "shot <n> <32 bytes>", for screenshots
static void send_shot() {
    char out[80];
    const uint8_t* b = &s_canvas.buf[0][0];
    for (int i = 0; i < 32; i++) {
        int w = snprintf(out, sizeof(out), "shot %02d ", i);
        for (int k = 0; k < 32; k++) w += snprintf(out + w, sizeof(out) - w, "%02x", b[i * 32 + k]);
        usblink::send(out, 50);
    }
}

static void render_task(void*) {
    s_brain.begin(now_ms(), esp_random());
    usblink::send("boot tinyclawd 1");

    usblink::Line line;
    char reply[usblink::MAX_LINE + 1];
    char msg[24];
    uint8_t t;

    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        uint32_t now = now_ms();

        while (xQueueReceive(s_lines, &line, 0) == pdTRUE) {
            reply[0] = 0;
            if (!strcmp(line.s, "hw shot")) {
                send_shot();
                usblink::send("ok shot");
                continue;
            }
            if (!hw::line(line.s, reply, sizeof(reply)))
                proto_line(s_brain, line.s, now, reply, sizeof(reply));
            if (reply[0]) usblink::send(reply);
        }

        while (xQueueReceive(s_touches, &t, 0) == pdTRUE) {
            s_brain.touch((Touch)t, now);
            if (t == TOUCH_DOWN || t == TOUCH_UP) continue;   // keep the link quiet
            snprintf(msg, sizeof(msg), "touch %s", touch::name((Touch)t));
            usblink::send(msg);
        }

        int64_t t0 = esp_timer_get_time();
        if (hw::test()) hw::draw_test(s_canvas, now);
        else s_brain.render(s_canvas, now);
        int64_t t1 = esp_timer_get_time();
        int pages = sh1106::flush(s_canvas);
        hw::frame((uint32_t)(t1 - t0), (uint32_t)(esp_timer_get_time() - t1), pages);

        // if a frame ran long, don't try to catch up, and still let idle run
        if (xTaskDelayUntil(&wake, pdMS_TO_TICKS(FRAME_MS)) == pdFALSE) {
            vTaskDelay(1);
            wake = xTaskGetTickCount();
        }
    }
}

extern "C" void app_main(void) {
    s_lines = xQueueCreate(8, sizeof(usblink::Line));
    s_touches = xQueueCreate(16, sizeof(uint8_t));

    usblink::start(s_lines);
    hw::load();       // saved screen + touch settings
    sh1106::init();   // if the screen is missing, flush keeps retrying
    touch::start(s_touches);

    xTaskCreate(render_task, "render", 8192, nullptr, 5, nullptr);
}
