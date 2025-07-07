#include "webserver.h"
#include "nvs_store.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include <string.h>

static const char *TAG = "WebServer";
static httpd_handle_t server = NULL;

static const char* form_html = 
   "<!DOCTYPE html>"
   "<html>"
   "<head>"
   "  <style>"
   "    html, body {"
   "      margin: 0; padding: 0; height: 100%;"
   "      background-color: #f9fbff;"
   "      font-family: Arial, sans-serif;"
   "      display: flex;"
   "      flex-direction: column;"
   "      min-height: 100vh;"
   "    }"
   "    .logo-wrapper {"
   "      width: 100%;"
   "      text-align: center;"
   "      padding-top: 60px;"
   "      padding-bottom: 40px;"
   "      box-sizing: border-box;"
   "    }"
   "    .container {"
   "      max-width: 400px;"
   "      margin: 0 auto 40px auto;"
   "      flex: 1 0 auto;"
   "      text-align: center;"
   "      padding: 0 20px;"
   "      box-sizing: border-box;"
   "    }"
   "    form {"
   "      margin-top: 30px;"
   "    }"
   "    label {"
   "      display: block;"
   "      margin: 15px 0 8px 0;"
   "      text-align: left;"
   "      font-weight: 600;"
   "      font-size: 14px;"
   "      color: #333;"
   "    }"
   "    input[type='text'], input[type='password'] {"
   "      width: 100%;"
   "      padding: 10px;"
   "      box-sizing: border-box;"
   "      font-size: 14px;"
   "      border: 1px solid #ccc;"
   "      border-radius: 4px;"
   "      transition: border-color 0.3s;"
   "    }"
   "    input[type='text']:focus, input[type='password']:focus {"
   "      border-color: #77aaff;"
   "      outline: none;"
   "    }"
   "    input[type='submit'] {"
   "      margin-top: 25px;"
   "      padding: 12px 25px;"
   "      font-size: 16px;"
   "      background-color: #77aaff;"
   "      border: none;"
   "      border-radius: 5px;"
   "      color: white;"
   "      cursor: pointer;"
   "      transition: background-color 0.3s;"
   "    }"
   "    input[type='submit']:hover {"
   "      background-color: #5599ee;"
   "    }"
   "    img {"
   "      max-width: 35vw;"
   "      height: auto;"
   "      display: inline-block;"
   "      border: 1px solid #EDEDFF;"
   "    }"
   "    footer {"
   "      flex-shrink: 0;"
   "      padding: 15px 30px;"
   "      font-size: 12px;"
   "      font-weight: bold;"
   "      color: #555;"
   "      display: flex;"
   "      justify-content: space-between;"
   "      background: #e6ecff;"
   "      border-top: 1px solid #ccd7ff;"
   "      box-sizing: border-box;"
   "    }"
   "  </style>"
   "</head>"
   "<body>"
   "  <div class='logo-wrapper'>"
   "    <img src='logo.png' alt='Logo'>"
   "  </div>"
   "  <div class='container'>"
   "    <h2>WiFi network configuration</h2>"
   "    <form method='POST' action='/save'>"
   "      <label for='ssid'>Network name (SSID):</label>"
   "      <input id='ssid' name='ssid' type='text' required>"
   "      <label for='pass'>Network password:</label>"
   "      <input id='pass' name='pass' type='password'>"
   "      <input type='submit' value='Save'>"
   "    </form>"
   "  </div>"
   "  <footer>"
   "    <span>embedberight.com</span>"
   "    <span>&copy; 2025</span>"
   "  </footer>"
   "</body>"
   "</html>";

void mount_spiffs() 
{
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

static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, form_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t logo_handler(httpd_req_t *req) {
    const char *filepath = "/spiffs/logo.png";
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/png");

    char buf[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), file)) > 0) {
        if (httpd_resp_send_chunk(req, buf, read_bytes) != ESP_OK) {
            fclose(file);
            httpd_resp_send_chunk(req, NULL, 0); 
            return ESP_FAIL;
        }
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0); 
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req) {
    char buf[128];
    int total_len = req->content_len;
    if (total_len >= sizeof(buf)) return ESP_FAIL;
    int ret = httpd_req_recv(req, buf, total_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    char ssid[64] = {0}, pass[64] = {0};
    sscanf(buf, "ssid=%63[^&]&pass=%63s", ssid, pass);
    nvs_store_queue_save_wifi(ssid, pass);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static void wifi_init_softap(void) {
    ESP_LOGI(TAG, "Starting WiFi SoftAP...");
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);

    wifi_config_t wifi_ap_config = { 0 };
    strncpy((char*)wifi_ap_config.ap.ssid, "Bootbone-Setup", sizeof(wifi_ap_config.ap.ssid));
    wifi_ap_config.ap.ssid_len = strlen("Bootbone-Setup");
    wifi_ap_config.ap.channel = 1;
    wifi_ap_config.ap.max_connection = 4;
    wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;

    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
    esp_wifi_start();
}

esp_err_t webserver_start(void) {
    wifi_init_softap();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/", .method = HTTP_GET, .handler = root_handler
        };
        httpd_uri_t save = {
            .uri = "/save", .method = HTTP_POST, .handler = save_handler
        };
        httpd_uri_t logo_uri = { 
            .uri = "/logo.png", .method = HTTP_GET, .handler = logo_handler
        };
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &save);
        httpd_register_uri_handler(server, &logo_uri);
        return ESP_OK;
    }
    return ESP_FAIL;
}

void webserver_stop(void) {
    if (server) {
        ESP_LOGI(TAG, "Stopping HTTP Server");
        httpd_stop(server);
        server = NULL;
    }
}
