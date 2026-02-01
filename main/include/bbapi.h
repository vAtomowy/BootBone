#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t BBAPI_init(const char* ws_uri);
void BBAPI_deinit(void);
bool BBAPI_is_ready(void);
esp_err_t BBAPI_send_text(const char* text);
esp_err_t BBAPI_send_text_timeout(const char* text, TickType_t to);
bool BBAPI_recv_text(char* out, size_t out_len, TickType_t to);
size_t BBAPI_tx_queued(void);
size_t BBAPI_rx_queued(void);
esp_err_t BBAPI_get_param(const char* key, void* buffer, size_t bufsize, size_t* out_len);

#ifdef __cplusplus
}
#endif
