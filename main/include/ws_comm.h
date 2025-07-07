#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

void websocket_server_start(void);

void websocket_send_json(const char *json_str);

void websocket_set_on_receive(void (*callback)(const char *json_str));

#endif 
