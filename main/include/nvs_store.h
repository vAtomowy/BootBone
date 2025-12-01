#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef enum {
    NVS_CMD_SET_STR,
    NVS_CMD_SET_U32,
    NVS_CMD_GET_STR,
    NVS_CMD_GET_U32,
    NVS_CMD_RESET
} nvs_cmd_type_t;

typedef struct {
    nvs_cmd_type_t type;
    char key[32];
    union {
        char str_value[256];
        uint32_t u32_value;
    };
    size_t out_len;
} nvs_cmd_t;

esp_err_t nvs_store_init(void);
esp_err_t nvs_store_set_str(const char* key, const char* value);
esp_err_t nvs_store_set_u32(const char* key, uint32_t value);
esp_err_t nvs_store_get_str(const char* key, char* buffer, size_t bufsize, size_t* out_len);
esp_err_t nvs_store_get_u32(const char* key, uint32_t* value);
esp_err_t nvs_store_reset(void);
