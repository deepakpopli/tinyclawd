#include "link.h"

#include <string.h>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char* TAG = "link";
static QueueHandle_t s_q;
static bool s_ok = false;

static void link_task(void*) {
    usblink::Line line;
    int len = 0;
    bool too_long = false;
    uint8_t buf[64];

    for (;;) {
        int n = usb_serial_jtag_read_bytes(buf, sizeof(buf), pdMS_TO_TICKS(100));
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                if (!too_long && len > 0) {
                    line.s[len] = 0;
                    if (xQueueSend(s_q, &line, pdMS_TO_TICKS(10)) != pdTRUE)
                        ESP_LOGW(TAG, "line dropped, queue full");
                }
                len = 0;
                too_long = false;
                continue;
            }
            if (too_long) continue;
            if (len >= usblink::MAX_LINE) {
                too_long = true;  // skip the rest of this line
                continue;
            }
            line.s[len++] = c;
        }
    }
}

void usblink::start(QueueHandle_t lines) {
    s_q = lines;
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.tx_buffer_size = 1024;
    cfg.rx_buffer_size = 512;
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err != ESP_OK) {
        // no link, the pet still runs on its own
        ESP_LOGW(TAG, "usb serial driver failed: %s", esp_err_to_name(err));
        return;
    }
    // route printf / logs through the driver too, so they don't fight
    // over the hardware fifo with our own writes
    usb_serial_jtag_vfs_use_driver();
    s_ok = true;
    xTaskCreate(link_task, "link", 3072, nullptr, 4, nullptr);
}

void usblink::send(const char* s, int wait_ms) {
    if (!s_ok) return;
    char out[MAX_LINE + 2];
    int n = strnlen(s, MAX_LINE);
    memcpy(out, s, n);
    out[n++] = '\n';
    // short timeout: if the host is not reading the tx buffer fills up and we just drop
    usb_serial_jtag_write_bytes(out, n, pdMS_TO_TICKS(wait_ms));
}
