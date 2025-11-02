#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ws_comm_start(const char* uri);  
void ws_comm_stop(void);                  
bool ws_comm_is_connected(void);          

esp_err_t ws_comm_send_text(const char* text);                   
esp_err_t ws_comm_send_text_timeout(const char* text, TickType_t to); 

bool ws_comm_recv_text(char* out, size_t out_len, TickType_t to); 

size_t ws_comm_tx_queued(void);            
size_t ws_comm_rx_queued(void);           

#ifdef __cplusplus
}
#endif
