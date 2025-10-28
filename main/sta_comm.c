#include <stdio.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "network_mgr.h"
#include "sta_comm.h"
#include "events.h"
#include <string.h>

static const char *TAG = "STA_CONNECT";

static int s_retry = 0;
#define MAX_RETRY 10

static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;

void init_STA_AP(void)
{
    ESP_LOGI(TAG, "Registering Wi-Fi and IP event handlers...");

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        &instance_any_id
    ));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        &instance_got_ip
    ));
}

void connect_to_AP(const char *ssid, const char *password) 
{
    ESP_LOGI(TAG, "Connecting to router...");
    net_sta_cfg_t cfg = {0};
    strncpy(cfg.ssid, ssid, sizeof(cfg.ssid));
    strncpy(cfg.pass, password, sizeof(cfg.pass));
    ESP_ERROR_CHECK(network_mgr_start_sta(&cfg)); 
}

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Wi-Fi started, connecting...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "Connected to AP (link up)");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "Got IP address!");
        xEventGroupClearBits(bootbone_s, WAIT_ON_CONN);          
        xEventGroupSetBits(bootbone_s, CONNECTED_TO_AP);     
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected, retrying...");
        if (s_retry++ < MAX_RETRY) {
            vTaskDelay(pdMS_TO_TICKS(500 + s_retry * 200));
            esp_wifi_connect();
        } else {
            s_retry = 0;
            xEventGroupSetBits(bootbone_s, ROLLBACK_TO_AP); 
        }
}
}