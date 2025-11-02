#include "ws_comm.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "WS_COMM";


#define WS_COMM_TX_QUEUE_LEN   16   
#define WS_COMM_RX_QUEUE_LEN   16   
#define WS_COMM_MAX_MSG        512  
#define WS_COMM_HEARTBEAT_MS   30000 

typedef struct {
    size_t len;
    char   data[WS_COMM_MAX_MSG];
} ws_msg_t; 

static esp_websocket_client_handle_t s_ws = NULL;
static TaskHandle_t s_task = NULL;
static QueueHandle_t s_txq = NULL;
static QueueHandle_t s_rxq = NULL;
static bool s_connected = false;
static char* s_uri = NULL;
static volatile bool s_run = false;
static int64_t s_last_rx_ms = 0; 

static inline int64_t now_ms(void) { return esp_timer_get_time() / 1000; } 

static void ws_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            s_connected = true;
            s_last_rx_ms = now_ms();
            ESP_LOGI(TAG, "WS connected");
            break;
        case WEBSOCKET_EVENT_DATA: {
            if (data->op_code == 1 && data->data_len > 0) {
                ws_msg_t msg = {0};
                size_t cpy = (data->data_len < WS_COMM_MAX_MSG-1) ? data->data_len : (WS_COMM_MAX_MSG-1);
                memcpy(msg.data, data->data_ptr, cpy);
                msg.data[cpy] = 0;
                msg.len = cpy;
                s_last_rx_ms = now_ms();
                if (xQueueSend(s_rxq, &msg, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "RX queue full, drop"); 
                } else {
                    ESP_LOGI(TAG, "WS RX: %s", msg.data);
                }
            }
            break;
        }
        case WEBSOCKET_EVENT_DISCONNECTED:
            s_connected = false;
            ESP_LOGW(TAG, "WS disconnected");
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGW(TAG, "WS error");
            break;
        default:
            break;
    }
}

static void ws_send_msg(const ws_msg_t* m) {
    if (!s_ws || !s_connected) return;
    int sent = esp_websocket_client_send_text(s_ws, m->data, (int)m->len, pdMS_TO_TICKS(5000));
    if (sent < 0) {
        ESP_LOGW(TAG, "send failed");
    }
}

static void ws_comm_task(void* pv) {
    const int backoff_steps_ms[] = {1000, 2000, 5000, 10000, 15000, 30000};
    int backoff_idx = 0;

    while (s_run) {
        if (!s_ws) {
            esp_websocket_client_config_t cfg = {
                .uri = s_uri,
                // .reconnect_timeout_ms = 10000, 
                // .disable_auto_reconnect = false,
            };
            s_ws = esp_websocket_client_init(&cfg);
            if (!s_ws) {
                ESP_LOGE(TAG, "init failed");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            esp_websocket_register_events(s_ws, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
            if (esp_websocket_client_start(s_ws) != ESP_OK) {
                ESP_LOGW(TAG, "start failed");
                esp_websocket_unregister_events(s_ws, WEBSOCKET_EVENT_ANY, ws_event_handler);
                esp_websocket_client_destroy(s_ws);
                s_ws = NULL;
                vTaskDelay(pdMS_TO_TICKS(backoff_steps_ms[backoff_idx]));
                if (backoff_idx < (int)(sizeof(backoff_steps_ms)/sizeof(backoff_steps_ms[0]))-1) backoff_idx++;
                continue;
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        if (s_connected) {
            backoff_idx = 0; 
            static int64_t last_hb = 0;
            int64_t t = now_ms();
            if (last_hb == 0) last_hb = t;
            if (t - last_hb >= WS_COMM_HEARTBEAT_MS) {
                const char* hb = "{\"type\":\"ping\"}";
                ws_msg_t m = { .len = strlen(hb) };
                memcpy(m.data, hb, m.len+1);
                ws_send_msg(&m);
                last_hb = t;
            }

            if (t - s_last_rx_ms > WS_COMM_HEARTBEAT_MS * 2) {
                ESP_LOGW(TAG, "RX timeout -> reconnect");
                esp_websocket_client_close(s_ws, pdMS_TO_TICKS(2000));
                esp_websocket_unregister_events(s_ws, WEBSOCKET_EVENT_ANY, ws_event_handler);
                esp_websocket_client_destroy(s_ws);
                s_ws = NULL;
                s_connected = false;
                continue;
            }

            ws_msg_t to_send;
            if (xQueueReceive(s_txq, &to_send, pdMS_TO_TICKS(50)) == pdTRUE) {
                ws_send_msg(&to_send);
            } else {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(backoff_steps_ms[backoff_idx]));
            if (backoff_idx < (int)(sizeof(backoff_steps_ms)/sizeof(backoff_steps_ms[0]))-1) backoff_idx++;
        }
    }

    if (s_ws) {
        esp_websocket_client_close(s_ws, portMAX_DELAY);
        esp_websocket_unregister_events(s_ws, WEBSOCKET_EVENT_ANY, ws_event_handler);
        esp_websocket_client_destroy(s_ws);
        s_ws = NULL;
    }
    vTaskDelete(NULL);
}

esp_err_t ws_comm_start(const char* uri) {
    if (s_task) return ESP_ERR_INVALID_STATE;
    if (!uri) return ESP_ERR_INVALID_ARG;

    s_txq = xQueueCreate(WS_COMM_TX_QUEUE_LEN, sizeof(ws_msg_t));
    s_rxq = xQueueCreate(WS_COMM_RX_QUEUE_LEN, sizeof(ws_msg_t));
    if (!s_txq || !s_rxq) {
        if (s_txq) vQueueDelete(s_txq);
        if (s_rxq) vQueueDelete(s_rxq);
        s_txq = s_rxq = NULL;
        return ESP_ERR_NO_MEM;
    }

    size_t ulen = strlen(uri);
    s_uri = (char*)malloc(ulen + 1);
    if (!s_uri) {
        vQueueDelete(s_txq); vQueueDelete(s_rxq);
        s_txq = s_rxq = NULL;
        return ESP_ERR_NO_MEM;
    }
    memcpy(s_uri, uri, ulen+1);

    s_run = true;
    s_connected = false;
    s_last_rx_ms = now_ms();

    BaseType_t ok = xTaskCreate(ws_comm_task, "ws_comm_task", 4096, NULL, 5, &s_task);
    if (ok != pdPASS) {
        free(s_uri); s_uri = NULL;
        vQueueDelete(s_txq); vQueueDelete(s_rxq);
        s_txq = s_rxq = NULL;
        s_run = false;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "WS_COMM started");
    return ESP_OK;
}

void ws_comm_stop(void) {
    s_run = false;
    if (s_task) {
        for (int i=0; i<50 && eTaskGetState(s_task) != eDeleted; ++i) vTaskDelay(pdMS_TO_TICKS(20));
        s_task = NULL;
    }
    if (s_ws) {
        esp_websocket_client_close(s_ws, portMAX_DELAY);
        esp_websocket_unregister_events(s_ws, WEBSOCKET_EVENT_ANY, ws_event_handler);
        esp_websocket_client_destroy(s_ws);
        s_ws = NULL;
    }
    if (s_uri) { free(s_uri); s_uri = NULL; }
    if (s_txq) { vQueueDelete(s_txq); s_txq = NULL; }
    if (s_rxq) { vQueueDelete(s_rxq); s_rxq = NULL; }
    s_connected = false;
    ESP_LOGI(TAG, "WS_COMM stopped");
}

bool ws_comm_is_connected(void) {
    return s_connected;
}

esp_err_t ws_comm_send_text_timeout(const char* text, TickType_t to) {
    if (!text || !s_txq) return ESP_ERR_INVALID_STATE;
    ws_msg_t msg = {0};
    size_t len = strnlen(text, WS_COMM_MAX_MSG-1);
    memcpy(msg.data, text, len);
    msg.data[len] = 0;
    msg.len = len;
    return xQueueSend(s_txq, &msg, to) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t ws_comm_send_text(const char* text) {
    return ws_comm_send_text_timeout(text, 0);
}

bool ws_comm_recv_text(char* out, size_t out_len, TickType_t to) {
    if (!out || out_len == 0 || !s_rxq) return false;
    ws_msg_t msg;
    if (xQueueReceive(s_rxq, &msg, to) != pdTRUE) return false;
    size_t cpy = (msg.len < out_len-1) ? msg.len : (out_len-1);
    memcpy(out, msg.data, cpy);
    out[cpy] = 0;
    return true;
}

size_t ws_comm_tx_queued(void) {
    if (!s_txq) return 0;
    return (size_t)uxQueueMessagesWaiting(s_txq);
}

size_t ws_comm_rx_queued(void) {
    if (!s_rxq) return 0;
    return (size_t)uxQueueMessagesWaiting(s_rxq);
}
