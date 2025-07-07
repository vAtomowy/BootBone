#include "ws_comm.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "WS_SERVER";

static httpd_handle_t server = NULL;
static httpd_handle_t start_webserver(void);

static int client_fd = -1;
static void (*on_receive_cb)(const char *json_str) = NULL;

void websocket_set_on_receive(void (*callback)(const char *json_str)) {
    on_receive_cb = callback;
}

void websocket_send_json(const char *json_str) {
    if (client_fd < 0 || !json_str) return;

    httpd_ws_frame_t ws_pkt = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)json_str,
        .len = strlen(json_str)
    };

    esp_err_t ret = httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send WebSocket message: %s", esp_err_to_name(ret));
    }
}

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, new client connected");
        client_fd = httpd_req_to_sockfd(req);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = {0};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    size_t buf_len = req->content_len;
    uint8_t *buf = calloc(1, buf_len + 1);
    if (!buf) return ESP_ERR_NO_MEM;

    ws_pkt.payload = buf;
    ws_pkt.len = buf_len;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, buf_len);
    if (ret != ESP_OK) {
        free(buf);
        return ret;
    }

    buf[buf_len] = '\0';

    ESP_LOGI(TAG, "Received WebSocket data: %s", buf);
    if (on_receive_cb) {
        on_receive_cb((const char *)buf);
    }

    free(buf);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true,
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws_uri);
        ESP_LOGI(TAG, "WebSocket server started on /ws");
    }

    return server;
}

void websocket_server_start(void) {
    start_webserver();
}

// WS EXAMPLE: 
// #include "websocket_server.h"
// #include "cJSON.h"

// static void on_ws_json_received(const char *json) {
//     ESP_LOGI("WS_RECEIVED", "Got JSON: %s", json);

//     cJSON *root = cJSON_Parse(json);
//     if (!root) return;

//     const cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
//     if (cmd && cJSON_IsString(cmd)) {
//         ESP_LOGI("WS_JSON", "Command: %s", cmd->valuestring);
//     }

//     cJSON_Delete(root);
// }

// void app_main(void) {
//     websocket_set_on_receive(on_ws_json_received);
//     websocket_server_start();

//     // Opcjonalne wysłanie JSON-a po starcie
//     websocket_send_json("{\"status\":\"started\"}");
// }
