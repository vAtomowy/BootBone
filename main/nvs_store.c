#include "nvs_store.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>
#include <inttypes.h>

#define NVS_NAMESPACE "boneboot"
#define TAG "NVS_STORE"

static nvs_handle_t s_nvs_handle;
static QueueHandle_t s_nvs_queue;
static TaskHandle_t s_nvs_task;

static void nvs_worker_task(void *param) 
{
    nvs_cmd_t cmd;
    while (1) {
        if (xQueueReceive(s_nvs_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd.type) {
                case NVS_CMD_SAVE_WIFI:
                    ESP_LOGI(TAG, "Saving WiFi config: %s / %s", cmd.ssid, cmd.pass);
                    nvs_set_str(s_nvs_handle, "wifi_ssid", cmd.ssid);
                    nvs_set_str(s_nvs_handle, "wifi_pass", cmd.pass);
                    nvs_commit(s_nvs_handle);
                    break;
                case NVS_CMD_RESET:
                    ESP_LOGW(TAG, "Erasing NVS namespace");
                    nvs_erase_all(s_nvs_handle);
                    nvs_commit(s_nvs_handle);
                    break;
                case NVS_CMD_SAVE_DEVICE_INFO:
                    ESP_LOGI(TAG, "Saving device name: %s, ID: %" PRIu32, cmd.device_name, cmd.device_id);
                    nvs_set_str(s_nvs_handle, "dev_name", cmd.device_name);
                    nvs_set_u32(s_nvs_handle, "dev_id", cmd.device_id);
                    nvs_commit(s_nvs_handle);
                    break;
                case NVS_CMD_SAVE_CA_CERT:
                    ESP_LOGI(TAG, "Saving CA cert");
                    nvs_set_str(s_nvs_handle, "ca_cert", cmd.ca_cert);
                    nvs_commit(s_nvs_handle);
                    break;
            }
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

esp_err_t nvs_store_queue_save_wifi(const char *ssid, const char *pass) 
{
    nvs_cmd_t cmd = {
        .type = NVS_CMD_SAVE_WIFI,
    };
    strncpy(cmd.ssid, ssid, sizeof(cmd.ssid) - 1);
    strncpy(cmd.pass, pass, sizeof(cmd.pass) - 1);
    return xQueueSend(s_nvs_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t nvs_store_get_wifi(char *ssid_out, size_t ssid_len, char *pass_out, size_t pass_len) 
{
    esp_err_t err;

    err = nvs_get_str(s_nvs_handle, "wifi_ssid", ssid_out, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read SSID from NVS (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_str(s_nvs_handle, "wifi_pass", pass_out, &pass_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read password from NVS (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Read WiFi config: %s / %s", ssid_out, pass_out);
    return ESP_OK;
}

esp_err_t nvs_store_queue_save_device_info(const char *name, uint32_t id)
{
    nvs_cmd_t cmd = {
        .type = NVS_CMD_SAVE_DEVICE_INFO,
        .device_id = id
    };
    strncpy(cmd.device_name, name, sizeof(cmd.device_name) - 1);
    return xQueueSend(s_nvs_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t nvs_store_queue_save_ca_cert(const char *cert)
{
    nvs_cmd_t cmd = {
        .type = NVS_CMD_SAVE_CA_CERT,
        .ca_cert = cert  
    };
    return xQueueSend(s_nvs_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t nvs_store_get_device_info(char *name_out, size_t name_len, uint32_t *id_out)
{
    esp_err_t err;
    err = nvs_get_str(s_nvs_handle, "dev_name", name_out, &name_len);
    if (err != ESP_OK) return err;

    err = nvs_get_u32(s_nvs_handle, "dev_id", id_out);
    return err;
}

esp_err_t nvs_store_get_ca_cert(char *cert_out, size_t cert_len)
{
    esp_err_t err = nvs_get_str(s_nvs_handle, "ca_cert", cert_out, &cert_len);
    return err;
}

esp_err_t nvs_store_queue_reset(void) 
{
    nvs_cmd_t cmd = {
        .type = NVS_CMD_RESET,
    };
    return xQueueSend(s_nvs_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE ? ESP_OK : ESP_FAIL;
}