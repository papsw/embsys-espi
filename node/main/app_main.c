#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "rgb.h"
#include "ws.h"


#define WIFI_SSID     CONFIG_WIFI_SSID
#define WIFI_PASS     CONFIG_WIFI_PASS
#define WS_URI        CONFIG_PI_URI
#define TAG           "main"
#define CONNECTED_BIT BIT0


static EventGroupHandle_t s_wifi_eg;

static void on_disconnect(void *, esp_event_base_t, int32_t, void *); // to match esp_event_handler_t
static void on_got_ip    (void *, esp_event_base_t, int32_t, void *); // to match esp_event_handler_t
static void wifi_init    (void);
static void on_ws_msg    (const char *data, int len);


void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    led_init();
    led_set(WIFI_DC);

    wifi_init();

    ESP_LOGI(TAG, "wifi connected");

    ws_init(WS_URI, on_ws_msg);
    ws_start();
}

static void on_disconnect(void *, esp_event_base_t, int32_t, void *) {
    led_set(WIFI_DC);
    esp_wifi_connect();
}

static void on_got_ip(void *, esp_event_base_t, int32_t, void *) {
    led_set(IDLE);
    xEventGroupSetBits(s_wifi_eg, CONNECTED_BIT);
}

static void wifi_init(void) {
    s_wifi_eg = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_disconnect, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP,         on_got_ip,    NULL);

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
    esp_wifi_connect();

    xEventGroupWaitBits(s_wifi_eg, CONNECTED_BIT, false, true, portMAX_DELAY);
}

static void on_ws_msg(const char *data, int len)
{
    ESP_LOGI(TAG, "ws msg: %.*s", len, data);
}
