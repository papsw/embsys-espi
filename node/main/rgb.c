#include "rgb.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include <stdbool.h>


#define LEDC_MODE   LEDC_LOW_SPEED_MODE // supports 8'b res
#define GPIO_R      25
#define GPIO_G      26
#define GPIO_B      27
#define CH_R        LEDC_CHANNEL_0
#define CH_G        LEDC_CHANNEL_1
#define CH_B        LEDC_CHANNEL_2

static status_state_t     s_state        = IDLE;
static bool               s_blink_on     = false;
static esp_timer_handle_t s_blink_timer  = NULL;
static esp_timer_handle_t s_flash_timer  = NULL;
static bool               s_flash_active = false;
static int                s_flash_r;
static int                s_flash_g;
static int                s_flash_b;


static void set_color(int r, int g, int b) {
    ledc_set_duty(LEDC_MODE, CH_R, 255 - r);   ledc_update_duty(LEDC_MODE, CH_R);
    ledc_set_duty(LEDC_MODE, CH_G, 255 - g);   ledc_update_duty(LEDC_MODE, CH_G);
    ledc_set_duty(LEDC_MODE, CH_B, 255 - b);   ledc_update_duty(LEDC_MODE, CH_B);
}

static void apply(void) {
    if (s_flash_active){ 
        set_color(s_flash_r, s_flash_g, s_flash_b);
        return;
    }

    switch (s_state){
        case IDLE:      set_color(0,   255, 0);   break;
        case COMPUTING: set_color(0,   0,   255); break;
        case NET_ERROR: set_color(255, 180, 0);   break;
        case BUF_FULL:  set_color(255, 0,   0);   break;
        case WIFI_DC:   set_color(s_blink_on ? 255 : 0, 0, 0); break;
    }
}

static void blink_cb(void *arg) {
    s_blink_on = !s_blink_on;
    apply();
}

static void flash_cb(void *arg) {
    s_flash_active = false;
    apply();
}


void led_init(void) {
    ledc_timer_config_t tcfg = {
        .speed_mode      = LEDC_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&tcfg);

    int pins[]           = {GPIO_R, GPIO_G, GPIO_B};
    ledc_channel_t chs[] = {CH_R,   CH_G,   CH_B};

    for (int i = 0; i < 3; i++) {
        ledc_channel_config_t cc = {
            .gpio_num   = pins[i],
            .speed_mode = LEDC_MODE,
            .channel    = chs[i],
            .timer_sel  = LEDC_TIMER_0,
            .duty       = 255,  //0 on 255 off
            .hpoint     = 0,
        };
        ledc_channel_config(&cc);
    }

    esp_timer_create_args_t blink_args = { .callback = blink_cb, .name = "led_blink" };
    esp_timer_create(&blink_args, &s_blink_timer);

    esp_timer_create_args_t flash_args = { .callback = flash_cb, .name = "led_flash" };
    esp_timer_create(&flash_args, &s_flash_timer);
}

void led_set(status_state_t state) {
    s_state = state;
    esp_timer_stop(s_blink_timer);
    if (state == WIFI_DC) {
        s_blink_on = true;
        esp_timer_start_periodic(s_blink_timer, 250 * 1000);
    }
    apply();
}

status_state_t led_get(void) {
    return s_state;
}

void led_flash(unsigned char r, unsigned char g, unsigned char b) {
    s_flash_r = r;  s_flash_g = g;  s_flash_b = b;
    s_flash_active = true;
    esp_timer_stop(s_flash_timer);
    esp_timer_start_once(s_flash_timer, 200 * 1000);
    apply();
}
