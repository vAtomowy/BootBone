#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "nvs_store.h"
#include "webserver.h"

#include "driver/gpio.h"
#include "indicator.h"

Indicator_t led;

static const char *TAG = "BootBone";

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

// Zapis danych
// nvs_store_queue_save_device_info("MojeUrządzenie", 12345);
// nvs_store_queue_save_ca_cert(ca_cert_pem_string);

// // Odczyt
// char devname[64];
// uint32_t devid;
// nvs_store_get_device_info(devname, sizeof(devname), &devid);

// char cert_buf[4096];
// if (nvs_store_get_ca_cert(cert_buf, sizeof(cert_buf)) == ESP_OK) {
//     ESP_LOGI(TAG, "CA cert: %s", cert_buf);
// }