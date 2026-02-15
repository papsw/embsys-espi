#pragma once
#include<stdbool.h>

typedef struct {
    //char  device_id[32];
    int   task_id;
    int   hits;
    int   total;
    int   compute_ms;
} ring_buf_t;

#define  RING_BUFFER_CAP 50

void rbuf_reset (void);
int  rbuf_count (void);
void rbuf_push  (const ring_buf_t *in);
bool rbuf_pop   (ring_buf_t *out);