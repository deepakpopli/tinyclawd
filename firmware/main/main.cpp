#include <stdio.h>

#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "brain.h"
#include "canvas.h"
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

static void render_task(void*) {
    s_brain.begin(now_ms(), esp_random());
    usblink::send("boot tinyclawd 1");

    usblink::Line line;
    char reply[64];
    char msg[24];
    uint8_t t;

    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        uint32_t now = now_ms();

        while (xQueueReceive(s_lines, &line, 0) == pdTRUE) {
            reply[0] = 0;
            proto_line(s_brain, line.s, now, reply, sizeof(reply));
            if (reply[0]) usblink::send(reply);
        }

        while (xQueueReceive(s_touches, &t, 0) == pdTRUE) {
            s_brain.touch((Touch)t, now);
            snprintf(msg, sizeof(msg), "touch %s", touch::name((Touch)t));
            usblink::send(msg);
        }

        s_brain.render(s_canvas, now);
        sh1106::flush(s_canvas);

        // if a frame ran long, don't try to catch up, and still let idle run
        if (xTaskDelayUntil(&wake, pdMS_TO_TICKS(FRAME_MS)) == pdFALSE) {
            vTaskDelay(1);
            wake = xTaskGetTickCount();
        }
    }
}

extern "C" void app_main(void) {
    s_lines = xQueueCreate(8, sizeof(usblink::Line));
    s_touches = xQueueCreate(8, sizeof(uint8_t));

    usblink::start(s_lines);
    sh1106::init();   // if the screen is missing, flush keeps retrying
    touch::start(s_touches);

    xTaskCreate(render_task, "render", 8192, nullptr, 5, nullptr);
}
