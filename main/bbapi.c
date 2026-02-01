#include "bbapi.h"
#include "ws_comm.h"  
#include "esp_log.h"
#include "nvs_store.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define TAG "BBAPI"
static bool s_initialized = false;
static bool s_params_initialized = false;

#define MAKE_VERSION(major, minor, patch, build) \
    (((uint32_t)(major) << 24) | ((uint32_t)(minor) << 16) | ((uint32_t)(patch) << 8) | (uint32_t)(build))


// ===== FAKE_API_TASK (wszystko prywatne) =====
#define FAKE_TAG "FAKE_API"

static void send_json_hello(void) {
    BBAPI_send_text("{\"v\":1,\"type\":\"hello\",\"device\":\"esp32c3\"}");
}

static void send_json_telemetry(int seq) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"v\":1,\"type\":\"telemetry\",\"temp\":%.1f,\"hum\":%d,\"seq\":%d}",
             23.5 + (seq % 5), 40 + (seq % 10), seq);
    BBAPI_send_text(buf);
}

static void send_cmd_set_led_timeout(int value, TickType_t to) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"v\":1,\"type\":\"cmd\",\"name\":\"set_led\",\"value\":%d,\"id\":\"%u\"}",
             value, (unsigned)xTaskGetTickCount());
    esp_err_t err = BBAPI_send_text_timeout(buf, to);
    if (err != ESP_OK) {
        ESP_LOGW(FAKE_TAG, "TX timeout (set_led=%d)", value);
    }
}

static void send_plain_text(const char* txt) {
    BBAPI_send_text(txt);
}

static void drain_rx(void) {
    char buf[512];
    while (BBAPI_recv_text(buf, sizeof(buf), 0)) {
        ESP_LOGI(FAKE_TAG, "RX <- SERVER: %s", buf);
    }
}

static void log_queues(void) {
    ESP_LOGI(FAKE_TAG, "Q TX=%u RX=%u ready=%d", 
             (unsigned)BBAPI_tx_queued(), 
             (unsigned)BBAPI_rx_queued(), 
             BBAPI_is_ready());
}

static void fake_api_task(void* pv) {
    int seq = 0;
    TickType_t last_action = xTaskGetTickCount();

    for (;;) {
        if (!BBAPI_is_ready()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        drain_rx();

        TickType_t now = xTaskGetTickCount();
        if (now - last_action >= pdMS_TO_TICKS(2000)) {
            switch ((seq / 3) % 3) {
                case 0: send_json_hello(); break;
                case 1: send_json_telemetry(seq); break;
                case 2: send_plain_text("plain-echo"); break;
            }
            last_action = now;
            seq++;
        }

        if ((seq % 15) == 0) {
            send_cmd_set_led_timeout((seq / 15) & 1, pdMS_TO_TICKS(500));
        }

        log_queues();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ===== WRAPERY BBAPI =====
static const char* BOOTBONE_DEFAULT_PARAMS[][2] = {
    {"device_id", "ESP32C3-DEFAULT"},
    {"hw_model", "ESP32C3-DEV"},
    {"device_type", "bootbone_controller"},
    {"serial_number", "SN-00000000"},
    {"nazwa_klienta", "Perplexity Labs"},
    {"pub_key_hash", "0000000000000000000000000000000000000000000000000000000000000000"}
};

static void init_default_params(void) {
    if (s_params_initialized) return;
    
    ESP_LOGI(TAG, "Initializing BootBone default parameters...");
    
    for (int i = 0; i < sizeof(BOOTBONE_DEFAULT_PARAMS) / sizeof(BOOTBONE_DEFAULT_PARAMS[0]); i++) {
        nvs_store_set_str(BOOTBONE_DEFAULT_PARAMS[i][0], BOOTBONE_DEFAULT_PARAMS[i][1]);
    }
    
    nvs_store_set_u32("hw_version", MAKE_VERSION(1, 0, 0, 0));
    nvs_store_set_u32("bootbone_fw_version", MAKE_VERSION(1, 0, 0, 0));
    nvs_store_set_u32("mainapp_fw_version", 0);
    
    nvs_store_set_u32("bbapi_init_flag", 1);  // Flaga inicjalizacji
    s_params_initialized = true;
    ESP_LOGI(TAG, "BootBone parameters initialized OK");
}

static bool is_bootbone_param(const char* key) 
{
    return strncmp(key, "device_id", 9) == 0 ||
           strncmp(key, "hw_model", 8) == 0 ||
           strncmp(key, "device_type", 10) == 0 ||
           strncmp(key, "serial_number", 13) == 0 ||
           strncmp(key, "nazwa_klienta", 13) == 0 ||
           strncmp(key, "pub_key_hash", 12) == 0 ||
           strncmp(key, "hw_version", 10) == 0 ||
           strncmp(key, "bootbone_fw_version", 19) == 0 ||
           strncmp(key, "mainapp_fw_version", 18) == 0;
}

static bool is_string_param(const char* key) 
{
    return strncmp(key, "device_id", 9) == 0 ||
           strncmp(key, "hw_model", 8) == 0 ||
           strncmp(key, "device_type", 10) == 0 ||
           strncmp(key, "serial_number", 13) == 0 ||
           strncmp(key, "nazwa_klienta", 13) == 0 ||
           strncmp(key, "pub_key_hash", 12) == 0;
}

esp_err_t BBAPI_init(const char* ws_uri) {
    if (s_initialized) return ESP_OK;
    
    ESP_ERROR_CHECK(nvs_store_init());
    
    uint32_t init_flag = 0;
    if (nvs_store_get_u32("bbapi_init_flag", &init_flag) != ESP_OK || init_flag == 0) 
    {
        init_default_params();
    }

    if (s_initialized) return ESP_OK;
    esp_err_t err = ws_comm_start(ws_uri);
    if (err != ESP_OK) return err;
    
    xTaskCreate(fake_api_task, "fake_api_task", 8192, NULL, 5, NULL);
    
    s_initialized = true;
    ESP_LOGI(TAG, "BBAPI + FAKE_API started");
    return ESP_OK;
}

void BBAPI_deinit(void) {
    if (!s_initialized) return;
    ws_comm_stop();
    s_initialized = false;
    ESP_LOGI(TAG, "BBAPI deinit");
}

bool BBAPI_is_ready(void) {
    return ws_comm_is_connected();
}

esp_err_t BBAPI_send_text(const char* text) {
    return ws_comm_send_text(text);
}

esp_err_t BBAPI_send_text_timeout(const char* text, TickType_t to) {
    return ws_comm_send_text_timeout(text, to);
}

bool BBAPI_recv_text(char* out, size_t out_len, TickType_t to) {
    return ws_comm_recv_text(out, out_len, to);
}

size_t BBAPI_tx_queued(void) {
    return ws_comm_tx_queued();
}

size_t BBAPI_rx_queued(void) {
    return ws_comm_rx_queued();
}

esp_err_t BBAPI_get_param(const char* key, void* buffer, size_t bufsize, size_t* out_len) {
    if (!is_bootbone_param(key)) {
        return ESP_ERR_NOT_SUPPORTED; 
    }
    
    if (is_string_param(key)) {
        return nvs_store_get_str(key, (char*)buffer, bufsize, out_len);
    } else {
        uint32_t* val = (uint32_t*)buffer;
        return nvs_store_get_u32(key, val);
    }
}
