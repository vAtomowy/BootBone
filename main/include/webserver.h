#pragma once

#include "esp_err.h"

void mount_spiffs(void);
esp_err_t webserver_start(void);
void webserver_stop(void);
