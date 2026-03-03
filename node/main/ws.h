#pragma once
#include <stdbool.h>

typedef void (*ws_on_message_t)(const char *data, int len);

void ws_init(const char *uri, ws_on_message_t on_msg);
void ws_start(void);
void ws_stop(void);
bool ws_is_connected(void);
void ws_send(const char *json, int len);