#include "network_mgr.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "NET_MANAGER";

static bool s_netif_inited = false;
static bool s_wifi_inited  = false;
static bool s_sta_got_ip   = false;
static net_mode_t s_mode   = NET_MODE_NULL;

static esp_event_handler_instance_t s_wifi_any_id_inst = NULL;
static esp_event_handler_instance_t s_ip_got_ip_inst   = NULL;

static void s_ip_evt(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_sta_got_ip = true;
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
    }
}

esp_err_t network_mgr_init(void) {
    if (!s_netif_inited) {
        ESP_ERROR_CHECK(esp_netif_init());                                      
        ESP_ERROR_CHECK(esp_event_loop_create_default());                         
        s_netif_inited = true;
    }

    if (!s_wifi_inited) {
        static esp_netif_t* s_ap_netif  = NULL;
        static esp_netif_t* s_sta_netif = NULL;

        if (!s_ap_netif)  s_ap_netif  = esp_netif_create_default_wifi_ap();        
        if (!s_sta_netif) s_sta_netif = esp_netif_create_default_wifi_sta();      

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));                                  
        s_wifi_inited = true;
        ESP_LOGI(TAG, "esp_netif+wifi initialized");
    }
    return ESP_OK;
}

esp_err_t network_mgr_register_handlers(esp_event_handler_t wifi_ip_handler, void* ctx) {
    if (!s_wifi_inited) return ESP_ERR_INVALID_STATE;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_ip_handler, ctx, &s_wifi_any_id_inst));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_ip_handler, ctx, &s_ip_got_ip_inst));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &s_ip_evt, NULL));

    return ESP_OK;
}

esp_err_t network_mgr_set_mode(net_mode_t mode) {
    if (!s_wifi_inited) return ESP_ERR_INVALID_STATE;

    wifi_mode_t wmode = WIFI_MODE_NULL;
    switch (mode) {
        case NET_MODE_AP:    wmode = WIFI_MODE_AP;    break;
        case NET_MODE_STA:   wmode = WIFI_MODE_STA;   break;
        case NET_MODE_APSTA: wmode = WIFI_MODE_APSTA; break;
        case NET_MODE_NULL:  wmode = WIFI_MODE_NULL;  break;
        default: return ESP_ERR_INVALID_ARG;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(wmode));                                     
    return ESP_OK;
}

esp_err_t network_mgr_start_ap(const net_ap_cfg_t* cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;

    esp_wifi_stop();

    ESP_ERROR_CHECK(network_mgr_set_mode(NET_MODE_AP));

    wifi_config_t ap = {0};
    strncpy((char*)ap.ap.ssid,     cfg->ssid, sizeof(ap.ap.ssid));
    strncpy((char*)ap.ap.password, cfg->pass, sizeof(ap.ap.password));
    ap.ap.ssid_len      = strlen((const char*)ap.ap.ssid);
    ap.ap.channel       = cfg->channel;
    ap.ap.max_connection= cfg->max_conn;
    ap.ap.authmode      = cfg->authmode;
    if (strlen(cfg->pass) == 0) {
        ap.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));                       
    ESP_ERROR_CHECK(esp_wifi_start());                                           
    ESP_LOGI(TAG, "SoftAP '%s' ch%d max_conn=%d", ap.ap.ssid, ap.ap.channel, ap.ap.max_connection);
    return ESP_OK;
}

esp_err_t network_mgr_start_sta(const net_sta_cfg_t* cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;

    s_sta_got_ip = false;

    esp_wifi_stop();

    ESP_ERROR_CHECK(network_mgr_set_mode(NET_MODE_STA));

    wifi_config_t sta = {0};
    strncpy((char*)sta.sta.ssid,     cfg->ssid, sizeof(sta.sta.ssid));
    strncpy((char*)sta.sta.password, cfg->pass, sizeof(sta.sta.password));

    sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));                         
    ESP_ERROR_CHECK(esp_wifi_start());                                             
    return ESP_OK;
}

esp_err_t network_mgr_stop_wifi(void) {
    if (!s_wifi_inited) return ESP_OK;
    ESP_ERROR_CHECK(esp_wifi_stop()); 
    return ESP_OK;
}

bool network_mgr_sta_got_ip(void) {
    return s_sta_got_ip;
}

net_mode_t network_mgr_get_mode(void) {
    return s_mode;
}
