#include "telemetry_buf.h"

static telemetry_t g_buf[TELEMETRY_BUF_CAP];
static int g_hd = 0;
static int g_tl = 0;
static int g_cnt = 0;

void telemetry_buf_reset(void) { g_hd = g_tl = g_cnt = 0; }

int telemetry_buf_count(void) { return g_cnt; }

void telemetry_buf_push(const telemetry_t *t) {
    if (g_cnt == TELEMETRY_BUF_CAP) {
        g_tl = (g_tl + 1) % TELEMETRY_BUF_CAP;
        g_cnt--;
    }
    g_buf[g_hd] = *t;
    g_hd = (g_hd + 1) % TELEMETRY_BUF_CAP;
    g_cnt++;
}

int telemetry_buf_pop(telemetry_t *out) {
    if (g_cnt == 0) return 0;
    *out = g_buf[g_tl];
    g_tl = (g_tl + 1) % TELEMETRY_BUF_CAP;
    g_cnt--;
    return 1;
}