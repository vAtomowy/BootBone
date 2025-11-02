#include "ws_comm.h"
#include "esp_log.h"

static const char* TAG = "BBAPI";
static bool s_initialized = false;

esp_err_t bbapi_init(const char* ws_uri) {
    if (s_initialized) return ESP_OK;
    esp_err_t err = ws_comm_start(ws_uri);      // start transportu [web:255]
    if (err != ESP_OK) return err;
    s_initialized = true;
    ESP_LOGI(TAG, "BBAPI init");
    return ESP_OK;
}

void bbapi_deinit(void) {
    if (!s_initialized) return;
    ws_comm_stop();                              // stop transportu [web:255]
    s_initialized = false;
    ESP_LOGI(TAG, "BBAPI deinit");
}

bool bbapi_is_ready(void) {
    // na początek „ready == WS connected”; docelowo dodasz Wi‑Fi/provisioning [web:255]
    return ws_comm_is_connected();
}

esp_err_t bbapi_send_text(const char* text) {
    return ws_comm_send_text(text);              // wrap [web:255]
}

esp_err_t bbapi_send_text_timeout(const char* text, TickType_t to) {
    return ws_comm_send_text_timeout(text, to);  // wrap [web:255]
}

bool bbapi_recv_text(char* out, size_t out_len, TickType_t to) {
    return ws_comm_recv_text(out, out_len, to);  // wrap [web:255]
}
