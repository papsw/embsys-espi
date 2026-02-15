#pragma once
#include <ring_buf.h>

typedef struct {
    int  id;
    char type[32];
    int  samples;
} task_t;

bool parse_task_msg  (const char *json, task_t *out);
char *build_json_msg (const ring_buf_t *in, const char *worker_id);
char *heartbeat_json (const char *worker_id, const char *status, int uptime_ms,
     int free_heap, short rssi, int buf_count);
