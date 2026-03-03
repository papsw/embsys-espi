#pragma once

typedef enum {
    IDLE,      //green
    COMPUTING, //blue
    NET_ERROR, //amber
    BUF_FULL,  //red
    WIFI_DC    //red blink
} status_state_t;

void led_init(void);
void led_set(status_state_t state);
status_state_t led_get(void);
void led_flash(unsigned char r, unsigned char g, unsigned char b);