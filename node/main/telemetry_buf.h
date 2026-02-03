#pragma once
#include <stdint.h>

typedef struct {
    int64_t ts_ms;
    char device_id[32];
    char boot_id[16];
    uint32_t seq;
    uint32_t uptime_ms;
    int rssi;
    int free_heap;
    int led;
} telemetry_t;

#define TELEMETRY_BUF_CAP 200

void telemetry_buf_reset(void);
int telemetry_buf_count(void);
void telemetry_buf_push(const telemetry_t *t);
int telemetry_buf_pop(telemetry_t *out);
