#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// line link over the USB-Serial-JTAG port (the same port as the console).
// a task reads bytes, builds lines and pushes them to a queue.
namespace usblink {

static const int MAX_LINE = 120;

struct Line {
    char s[MAX_LINE + 1];
};

void start(QueueHandle_t lines);   // queue of usblink::Line
void send(const char* s, int wait_ms = 2);   // writes s + "\n", drops it if no one is reading

}
