#include "bbapi.h"
#include "esp_err.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ws_comm.h"
#include "esp_log.h"

#include "bbapi.h"
#include "ws_comm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define TAG "FAKE_API"

static void send_json_hello(void) {
    ws_comm_send_text("{\"v\":1,\"type\":\"hello\",\"device\":\"esp32c3\"}"); 
}

static void send_json_telemetry(int seq) {
    char buf[256];

    snprintf(buf, sizeof(buf),
             "{\"v\":1,\"type\":\"telemetry\",\"temp\":%.1f,\"hum\":%d,\"seq\":%d}",
             23.5 + (seq % 5), 40 + (seq % 10), seq);
    ws_comm_send_text(buf); 
}

static void send_cmd_set_led_timeout(int value, TickType_t to) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"v\":1,\"type\":\"cmd\",\"name\":\"set_led\",\"value\":%d,\"id\":\"%u\"}",
             value, (unsigned) xTaskGetTickCount());
    esp_err_t err = ws_comm_send_text_timeout(buf, to); 
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "TX timeout (cmd set_led=%d)", value); 
    }
}

static void send_plain_text(const char* txt) {
    ws_comm_send_text(txt); 
}

static void drain_rx(void) {
    char buf[512];
    while (ws_comm_recv_text(buf, sizeof(buf), 0)) {
        ESP_LOGI(TAG, "RX: %s", buf); 
    }
}

static void log_queues(void) {
    size_t tq = ws_comm_tx_queued();
    size_t rq = ws_comm_rx_queued();
    ESP_LOGI(TAG, "Queues TX=%u RX=%u", (unsigned) tq, (unsigned) rq);
}

static void burst_backpressure_test(void) {
    for (int i = 0; i < 32; ++i) {
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"type\":\"burst\",\"i\":%d}", i);
        esp_err_t err = ws_comm_send_text_timeout(buf, pdMS_TO_TICKS(50)); 
        if (err != ESP_OK) 
        {
            ESP_LOGW(TAG, "burst drop i=%d (TX full)", i); 
        }
    }
}

static void fake_api_task(void* pv) {
    int seq = 0;
    TickType_t last_action = xTaskGetTickCount();

    for (;;) {
        if (!ws_comm_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        drain_rx(); 

        TickType_t now = xTaskGetTickCount();
        if (now - last_action >= pdMS_TO_TICKS(2000)) {
            switch ((seq / 3) % 3) {
                case 0:
                    send_json_hello();              
                    break;
                case 1:
                    send_json_telemetry(seq);     
                    break;
                case 2:
                    send_plain_text("plain-echo");  
                    break;
            }
            last_action = now;
            seq++;
        }

        if ((seq % 15) == 0) {
            send_cmd_set_led_timeout((seq / 15) & 1, pdMS_TO_TICKS(500)); 
        }

        if ((seq % 45) == 0) {
            burst_backpressure_test(); 
        }

        log_queues(); 
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void BBAPI_init(void) {
    xTaskCreate(fake_api_task, "fake_api_task", 8192, NULL, 5, NULL); 
}