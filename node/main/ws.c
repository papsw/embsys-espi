#include "ws.h"
#include "rgb.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include <stdbool.h>


static esp_websocket_client_handle_t s_client    = NULL;
static ws_on_message_t               s_on_msg    = NULL;
static volatile bool                 s_connected = false;

static void ws_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    esp_websocket_event_data_t *ev = (esp_websocket_event_data_t *)data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            s_connected = true;
            led_set(IDLE);
            ESP_LOGI("ws", "connected");
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            s_connected = false;
            led_set(NET_ERROR);
            ESP_LOGI("ws", "disconnected");
            break;

        case WEBSOCKET_EVENT_DATA:
            if (ev->op_code == 0x01 && s_on_msg) {
                s_on_msg(ev->data_ptr, ev->data_len);
            }
            break;
    }
}

void ws_init(const char *uri, ws_on_message_t on_msg) {
    s_on_msg = on_msg;
    esp_websocket_client_config_t cfg = { .uri = uri };
    s_client = esp_websocket_client_init(&cfg);
    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
}

void ws_start(void) {
    esp_websocket_client_start(s_client);
}

void ws_stop(void) {
    esp_websocket_client_stop(s_client);
}

bool ws_is_connected(void) {
    return s_connected;
}

void ws_send(const char *json, int len){
    if (s_connected) {
        esp_websocket_client_send_text(s_client, json, len, pdMS_TO_TICKS(1000));
    }
}
