#include "nvs_store.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_task_wdt.h"
#include "esp_log.h"
#include <string.h>
#include <inttypes.h>

#define NVS_NAMESPACE "boneboot"
#define TAG "NVS_STORE"

static nvs_handle_t s_nvs_handle = 0;
static QueueHandle_t s_nvs_queue = NULL;
static TaskHandle_t s_nvs_task = NULL;

static void nvs_worker_task(void *param) 
{
    esp_task_wdt_add(NULL);

    nvs_cmd_t cmd;
    while (1) {
        esp_task_wdt_reset();

        if (xQueueReceive(s_nvs_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            esp_err_t err = ESP_OK;
            switch (cmd.type) {
                case NVS_CMD_SET_STR:
                    ESP_LOGD(TAG, "SET STR: %s", cmd.key);
                    err = nvs_set_str(s_nvs_handle, cmd.key, cmd.str_value);
                    nvs_commit(s_nvs_handle);
                    break;
                case NVS_CMD_SET_U32:
                    ESP_LOGD(TAG, "SET U32: %s", cmd.key);
                    err = nvs_set_u32(s_nvs_handle, cmd.key, cmd.u32_value);
                    nvs_commit(s_nvs_handle);
                    break;
                case NVS_CMD_GET_STR:
                    cmd.out_len = sizeof(cmd.str_value);
                    err = nvs_get_str(s_nvs_handle, cmd.key, cmd.str_value, &cmd.out_len);
                    if (err != ESP_OK) cmd.out_len = 0;
                    xQueueSend(s_nvs_queue, &cmd, 0);
                    vTaskDelay(pdMS_TO_TICKS(1));
                    continue;
                case NVS_CMD_GET_U32:
                    err = nvs_get_u32(s_nvs_handle, cmd.key, &cmd.u32_value);
                    cmd.out_len = (err == ESP_OK) ? 4 : 0;
                    xQueueSend(s_nvs_queue, &cmd, 0);
                    vTaskDelay(pdMS_TO_TICKS(1));
                    continue;
                case NVS_CMD_RESET:
                    ESP_LOGW(TAG, "RESET NVS");
                    nvs_erase_all(s_nvs_handle);
                    nvs_commit(s_nvs_handle);
                    break;
            }
            vTaskDelay(pdMS_TO_TICKS(2));
        } else {
            esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

esp_err_t nvs_store_init(void) 
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) return ret;

    s_nvs_queue = xQueueCreate(10, sizeof(nvs_cmd_t));
    if (!s_nvs_queue) return ESP_ERR_NO_MEM;

    xTaskCreate(nvs_worker_task, "nvs_task", 4096, NULL, 5, &s_nvs_task);
    return ESP_OK;
}

esp_err_t nvs_store_set_str(const char* key, const char* value) {
    if (!s_nvs_queue) return ESP_FAIL;
    nvs_cmd_t cmd = { .type = NVS_CMD_SET_STR };
    strncpy(cmd.key, key, sizeof(cmd.key) - 1);
    cmd.key[sizeof(cmd.key) - 1] = '\0';
    strncpy(cmd.str_value, value, sizeof(cmd.str_value) - 1);
    cmd.str_value[sizeof(cmd.str_value) - 1] = '\0';
    return xQueueSend(s_nvs_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t nvs_store_set_u32(const char* key, uint32_t value) {
    if (!s_nvs_queue) return ESP_FAIL;
    nvs_cmd_t cmd = { .type = NVS_CMD_SET_U32 };
    strncpy(cmd.key, key, sizeof(cmd.key) - 1);
    cmd.key[sizeof(cmd.key) - 1] = '\0';
    cmd.u32_value = value;
    return xQueueSend(s_nvs_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t nvs_store_get_str(const char* key, char* buffer, size_t bufsize, size_t* out_len) {
    if (!s_nvs_queue) return ESP_FAIL;
    nvs_cmd_t cmd = { .type = NVS_CMD_GET_STR };
    strncpy(cmd.key, key, sizeof(cmd.key) - 1);
    cmd.key[sizeof(cmd.key) - 1] = '\0';

    if (xQueueSend(s_nvs_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_FAIL;
    }
    if (xQueueReceive(s_nvs_queue, &cmd, pdMS_TO_TICKS(500)) == pdTRUE) {
        size_t len = cmd.out_len <= bufsize ? cmd.out_len : bufsize;
        strncpy(buffer, cmd.str_value, len);
        if (len > 0 && len < bufsize) buffer[len] = '\0';
        if (out_len) *out_len = len;
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_store_get_u32(const char* key, uint32_t* value) {
    if (!s_nvs_queue) return ESP_FAIL;
    nvs_cmd_t cmd = { .type = NVS_CMD_GET_U32 };
    strncpy(cmd.key, key, sizeof(cmd.key) - 1);
    cmd.key[sizeof(cmd.key) - 1] = '\0';

    if (xQueueSend(s_nvs_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_FAIL;
    }
    if (xQueueReceive(s_nvs_queue, &cmd, pdMS_TO_TICKS(500)) == pdTRUE) {
        *value = cmd.u32_value;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t nvs_store_reset(void) {
    if (!s_nvs_queue) return ESP_FAIL;
    nvs_cmd_t cmd = { .type = NVS_CMD_RESET };
    return xQueueSend(s_nvs_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE ? ESP_OK : ESP_FAIL;
}