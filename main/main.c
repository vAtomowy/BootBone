#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "events.h"

#include "nvs_store.h"
#include "webserver.h"
#include "sta_comm.h"
#include "ws_comm.h"
#include "network_mgr.h"
#include "bbapi.h"

#include "driver/gpio.h"
#include "indicator.h"
#include "indicator.h"

static const char *TAG = "BootBone";

Indicator_t led;
EventGroupHandle_t bootbone_s;

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

static void bootbone_task(void *pvParameters) {

    ESP_LOGI(TAG, "Bootbone starting in RTOS task...");

    ESP_ERROR_CHECK(nvs_flash_init());                                  
    ESP_ERROR_CHECK(nvs_store_init());                                   

    ESP_ERROR_CHECK(network_mgr_init());                                  
    ESP_ERROR_CHECK(network_mgr_register_handlers(wifi_event_handler, NULL)); 

    bootbone_s = xEventGroupCreate();
    mount_spiffs();                                                        

    ESP_ERROR_CHECK(webserver_start());                                   

    while (1) 
    {

        EventBits_t bits = xEventGroupWaitBits(
            bootbone_s,
            PROV_DONE | CONNECTED_TO_AP,  
            pdTRUE,                        
            pdFALSE,
            pdMS_TO_TICKS(500)
        );

        if (!(bits & PROV_DONE) && !(xEventGroupGetBits(bootbone_s) & ON_STA_MODE)) {
            ESP_LOGI(TAG, "Bootbone waiting on fill form");
        }

        if ((bits & PROV_DONE) && !(xEventGroupGetBits(bootbone_s) & ON_STA_MODE)) {
            xEventGroupSetBits(bootbone_s, ON_STA_MODE);
            ESP_LOGI(TAG, "Set Bootbone mode to STA");
        }

        EventBits_t state = xEventGroupGetBits(bootbone_s);
        if ((state & ON_STA_MODE) && !(state & WAIT_ON_CONN) && !(state & CONNECTED_TO_AP)) {

            webserver_stop();                           
            vTaskDelay(pdMS_TO_TICKS(300));
            ESP_LOGI(TAG, "Webserver STOP !");

            ESP_ERROR_CHECK(network_mgr_stop_wifi());     

            char local_ssid[32] = {0};
            char local_pass[64] = {0};
            size_t local_ssid_len = sizeof(local_ssid);
            size_t local_pass_len = sizeof(local_pass);
            (void)nvs_store_get_wifi(local_ssid, local_ssid_len, local_pass, local_pass_len);

            ESP_LOGI(TAG, "SSID: %.*s, Password: %.*s",
                    (int)local_ssid_len, local_ssid,
                    (int)local_pass_len, local_pass);

            connect_to_AP(local_ssid, local_pass);
            xEventGroupSetBits(bootbone_s, WAIT_ON_CONN);
        }

        if (bits & CONNECTED_TO_AP) {
            ESP_LOGI(TAG, "Connected to AP and got IP - switching to normal operation");
            xEventGroupClearBits(bootbone_s, WAIT_ON_CONN | ON_STA_MODE | ROLLBACK_TO_AP);
            xEventGroupSetBits(bootbone_s, CONNECTED_TO_AP);
            BBAPI_init();
        }

        if (xEventGroupGetBits(bootbone_s) & CONNECTED_TO_AP) 
        {
            static bool ws_started = false;
            if (!ws_started) {
                ws_comm_start("ws://192.168.2.140:8000/ws"); 
                ws_started = true;
            }
            vTaskDelay(pdMS_TO_TICKS(500)); 
        }

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