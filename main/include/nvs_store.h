#ifndef NVS_STORE_H
#define NVS_STORE_H

#include "esp_err.h"

typedef enum {
    NVS_CMD_SAVE_WIFI,
    NVS_CMD_RESET,
} nvs_cmd_type_t;

typedef struct {
    nvs_cmd_type_t type;
    char ssid[64];
    char pass[64];
} nvs_cmd_t;

esp_err_t nvs_store_init(void);
esp_err_t nvs_store_queue_save_wifi(const char *ssid, const char *pass);
esp_err_t nvs_store_queue_reset(void);

#endif // NVS_STORE_H