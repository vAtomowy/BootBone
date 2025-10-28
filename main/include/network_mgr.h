#pragma once
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_MODE_NULL = 0,
    NET_MODE_AP,
    NET_MODE_STA,
    NET_MODE_APSTA
} net_mode_t;

typedef struct {
    char ssid[32];
    char pass[64];
    uint8_t channel;
    uint8_t max_conn;
    wifi_auth_mode_t authmode; // WIFI_AUTH_OPEN jeśli brak hasła
} net_ap_cfg_t;

typedef struct {
    char ssid[32];
    char pass[64];
} net_sta_cfg_t;

// Jednorazowa inicjalizacja stosu i Wi-Fi (esp_netif_init, event loop, create_default_* i esp_wifi_init)
esp_err_t network_mgr_init(void);

// Rejestracja wspólnych handlerów (użyjemy funkcji zgodnej z Twoim sta_comm: wifi_event_handler)
esp_err_t network_mgr_register_handlers(esp_event_handler_t wifi_ip_handler, void* ctx);

// Rozruch SoftAP
esp_err_t network_mgr_start_ap(const net_ap_cfg_t* cfg);

// Rozruch STA (po zapisaniu SSID/PASS)
esp_err_t network_mgr_start_sta(const net_sta_cfg_t* cfg);

// Przełączenie trybu (wywołuje esp_wifi_set_mode; nie deinituje Wi-Fi)
esp_err_t network_mgr_set_mode(net_mode_t mode);

// Stop aktualnego Wi-Fi (esp_wifi_stop)
esp_err_t network_mgr_stop_wifi(void);

// Pomocnicze
bool       network_mgr_sta_got_ip(void);
net_mode_t network_mgr_get_mode(void);

#ifdef __cplusplus
}
#endif
