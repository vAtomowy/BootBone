#ifndef NVS_STORE_H
#define NVS_STORE_H

#include "esp_err.h"
#include <stdint.h>

typedef enum {
    NVS_CMD_SAVE_WIFI,
    NVS_CMD_SAVE_DEVICE_INFO,
    NVS_CMD_SAVE_CA_CERT,
    NVS_CMD_RESET,
} nvs_cmd_type_t;

typedef struct {
    nvs_cmd_type_t type;
    char ssid[64];
    char pass[64];
    char device_name[64];
    uint32_t device_id;
    const char *ca_cert; 
} nvs_cmd_t;

esp_err_t nvs_store_init(void);
esp_err_t nvs_store_queue_save_wifi(const char *ssid, const char *pass);
esp_err_t nvs_store_queue_save_device_info(const char *name, uint32_t id);
esp_err_t nvs_store_queue_save_ca_cert(const char *cert);
esp_err_t nvs_store_queue_reset(void);

esp_err_t nvs_store_get_wifi(char *ssid_out, size_t ssid_len, char *pass_out, size_t pass_len);
esp_err_t nvs_store_get_device_info(char *name_out, size_t name_len, uint32_t *id_out);
esp_err_t nvs_store_get_ca_cert(char *cert_out, size_t cert_len);

#endif // NVS_STORE_H
