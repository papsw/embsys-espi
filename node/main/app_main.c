#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "telemetry_buf.h"

//todo
static const char *TAG = "esp_node";
#define SSID "net"
#define PASS "pass"
#define SRV "http://192.168.1.50:8080"
#define DEVID "esp32-01"
#define LEDPIN 2

static SemaphoreHandle_t mtx;
static volatile int led_state = 0;
static volatile int period = 2000;
static char bootid[16];
static uint32_t seqnum = 0;

static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t c = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&c));
    wifi_config_t wc = { 0 };
    strncpy((char*)wc.sta.ssid, SSID, sizeof wc.sta.ssid);
    strncpy((char*)wc.sta.password, PASS, sizeof wc.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static int64_t getms(void) { return esp_timer_get_time() / 1000; }

static void mkbootid(char *out) { snprintf(out, 16, "%08" PRIx32, esp_random()); }

static int rssi(void) {
    wifi_ap_record_t ap;
    return (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : -127;
}

static void setled(int v) { led_state = (v != 0); gpio_set_level(LEDPIN, led_state); }

static esp_err_t postjson(const char *url, const char *js) {
    esp_http_client_config_t cfg = { .url = url, .timeout_ms = 1500 };
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    esp_http_client_set_method(h, HTTP_METHOD_POST);
    esp_http_client_set_header(h, "Content-Type", "application/json");
    esp_http_client_set_post_field(h, js, strlen(js));
    esp_err_t e = esp_http_client_perform(h);
    int code = esp_http_client_get_status_code(h);
    esp_http_client_cleanup(h);
    if (e != ESP_OK) return e;
    return (code >= 200 && code < 300) ? ESP_OK : ESP_FAIL;
}

static esp_err_t httpget(const char *url, char *buf, int len) {
    esp_http_client_config_t cfg = { .url = url, .timeout_ms = 1500 };
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    esp_http_client_set_method(h, HTTP_METHOD_GET);
    esp_err_t e = esp_http_client_perform(h);
    if (e != ESP_OK) { esp_http_client_cleanup(h); return e; }
    int code = esp_http_client_get_status_code(h);
    if (code < 200 || code >= 300) { esp_http_client_cleanup(h); return ESP_FAIL; }
    int n = esp_http_client_read_response(h, buf, len - 1);
    buf[n < 0 ? 0 : n] = 0;
    esp_http_client_cleanup(h);
    return ESP_OK;
}

static cJSON* mktelemetry(telemetry_t *t) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "device_id", t->device_id);
    cJSON_AddStringToObject(j, "boot_id", t->boot_id);
    cJSON_AddNumberToObject(j, "seq", t->seq);
    cJSON_AddNumberToObject(j, "uptime_ms", t->uptime_ms);
    cJSON_AddNumberToObject(j, "rssi", t->rssi);
    cJSON_AddNumberToObject(j, "free_heap", t->free_heap);
    cJSON_AddNumberToObject(j, "led", t->led);
    cJSON_AddNumberToObject(j, "ts_ms", (double)t->ts_ms);
    return j;
}

static void telem_task(void *arg) {
    char url[256]; snprintf(url, 256, "%s/api/v1/telemetry", SRV);
    while (1) {
        telemetry_t t = {0};
        t.ts_ms = getms();
        strncpy(t.device_id, DEVID, 31); strncpy(t.boot_id, bootid, 15);
        t.seq = ++seqnum;
        t.uptime_ms = (uint32_t)(esp_timer_get_time() / 1000);
        t.rssi = rssi(); t.free_heap = esp_get_free_heap_size(); t.led = led_state ? 1 : 0;

        cJSON *j = mktelemetry(&t);
        char *s = cJSON_PrintUnformatted(j); cJSON_Delete(j);
        esp_err_t r = postjson(url, s);

        xSemaphoreTake(mtx, portMAX_DELAY);
        if (r != ESP_OK) {
            telemetry_buf_push(&t);
            ESP_LOGW(TAG, "post fail, buf=%d", telemetry_buf_count());
        } else {
            telemetry_t tmp; int flushed = 0;
            for (int i = 0; i < 10 && telemetry_buf_pop(&tmp); i++) {
                cJSON *j2 = mktelemetry(&tmp);
                char *s2 = cJSON_PrintUnformatted(j2); cJSON_Delete(j2);
                if (postjson(url, s2) != ESP_OK) { free(s2); telemetry_buf_push(&tmp); break; }
                free(s2); flushed++;
            }
            if (flushed) ESP_LOGI(TAG, "flushed %d", flushed);
        }
        xSemaphoreGive(mtx);
        free(s);
        vTaskDelay(pdMS_TO_TICKS(period));
    }
}

static void cmd_task(void *arg) {
    char url[256], buf[512];
    while (1) {
        snprintf(url, 256, "%s/api/v1/commands/next?device_id=%s&boot_id=%s", SRV, DEVID, bootid);
        if (httpget(url, buf, 512) == ESP_OK) {
            cJSON *root = cJSON_Parse(buf);
            if (root) {
                cJSON *cmd = cJSON_GetObjectItem(root, "command");
                if (cmd && !cJSON_IsNull(cmd)) {
                    cJSON *id = cJSON_GetObjectItem(cmd, "id");
                    cJSON *tp = cJSON_GetObjectItem(cmd, "type");
                    cJSON *pl = cJSON_GetObjectItem(cmd, "payload");
                    if (cJSON_IsNumber(id) && cJSON_IsString(tp) && cJSON_IsObject(pl)) {
                        long cid = (long)id->valuedouble;
                        const char *ctype = tp->valuestring;
                        if (strcmp(ctype, "set_led") == 0) {
                            cJSON *v = cJSON_GetObjectItem(pl, "value");
                            if (cJSON_IsNumber(v)) setled((int)v->valuedouble);
                        } else if (strcmp(ctype, "set_period") == 0) {
                            cJSON *m = cJSON_GetObjectItem(pl, "ms");
                            if (cJSON_IsNumber(m)) {
                                int p = (int)m->valuedouble;
                                period = (p < 200) ? 200 : (p > 10000) ? 10000 : p;
                            }
                        }
                        char ackurl[256];
                        snprintf(ackurl, 256, "%s/api/v1/commands/%ld/ack", SRV, cid);
                        cJSON *ack = cJSON_CreateObject();
                        cJSON_AddStringToObject(ack, "device_id", DEVID);
                        cJSON_AddStringToObject(ack, "boot_id", bootid);
                        cJSON_AddStringToObject(ack, "result", "ok");
                        char *acks = cJSON_PrintUnformatted(ack); cJSON_Delete(ack);
                        postjson(ackurl, acks); free(acks);
                        ESP_LOGI(TAG, "cmd %ld %s", cid, ctype);
                    }
                }
                cJSON_Delete(root);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    gpio_config_t io = { .pin_bit_mask = (1ULL << LEDPIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io); setled(0);
    mtx = xSemaphoreCreateMutex();
    telemetry_buf_reset();
    mkbootid(bootid);
    ESP_LOGI(TAG, "boot=%s", bootid);
    wifi_init();
    xTaskCreate(telem_task, "telem", 6144, NULL, 5, NULL);
    xTaskCreate(cmd_task, "cmd", 6144, NULL, 5, NULL);
}
