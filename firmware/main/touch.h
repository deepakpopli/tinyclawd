#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "brain.h"

// TTP223 on GPIO1. a small task polls it every 10 ms and pushes
// Touch values (TOUCH_TAP / TOUCH_DOUBLE / TOUCH_LONG) into the queue.
namespace touch {

void start(QueueHandle_t events);   // queue of uint8_t (Touch)
const char* name(Touch t);          // "tap", "double", "long"

}
