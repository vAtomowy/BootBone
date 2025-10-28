#pragma once

#include "esp_event.h"

void init_STA_AP(void);
void connect_to_AP(const char *ssid, const char *password);
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
