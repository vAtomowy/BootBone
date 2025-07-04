#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "nvs_store.h"
#include "webserver.h"

#include "esp_spiffs.h"

#include "driver/gpio.h"
#include "indicator.h"

Indicator_t led;

static const char *TAG = "BootBone";

void mount_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS Mount failed");
    } else {
        size_t total = 0, used = 0;
        esp_spiffs_info(NULL, &total, &used);
        ESP_LOGI(TAG, "SPIFFS mounted. Total: %d, Used: %d", total, used);
    }
}

static void bootbone_task(void *pvParameters) {

    ESP_LOGI(TAG, "Bootbone starting in RTOS task...");
    ESP_ERROR_CHECK(nvs_store_init());
    mount_spiffs();
    ESP_ERROR_CHECK(webserver_start());

    while(1)
    { 
      ESP_LOGI(TAG, "Bootbone default LOG");
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelete(NULL); 
}

void app_main(void) {
    
    Indicator_Init(&led, GPIO_NUM_6);

    Indicator_Control(&led, INDICATOR_BLINK);
    vTaskDelay(pdMS_TO_TICKS(2000));

    xTaskCreate(bootbone_task, "bootbone_task", 8192, NULL, 5, NULL);
}
