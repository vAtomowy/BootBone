#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

extern EventGroupHandle_t bootbone_s;

#define PROV_DONE        BIT0
#define ON_STA_MODE      BIT1
#define WAIT_ON_CONN     BIT2
#define ROLLBACK_TO_AP   BIT3
#define CONNECTED_TO_AP  BIT4
#define WAIT_ON_RESET    BIT5
#define ON_RESET         BIT6