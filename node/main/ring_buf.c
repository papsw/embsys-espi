#include "ring_buf.h"

static ring_buf_t ring_buffer[RING_BUFFER_CAP];
static int head = 0;
static int tail = 0;
static int count = 0;

// todo mutex?

void rbuf_reset(void) {
    head = tail = count = 0;
}   

int rbuf_count(void)
{
    return count;
}

void rbuf_push(const ring_buf_t *in) {
    if (count == RING_BUFFER_CAP) { // drop oldest if full
        if (++tail == RING_BUFFER_CAP) tail = 0;
        count--;
    }

    ring_buffer[head] = *in;
    if (++head == RING_BUFFER_CAP) head = 0;
    count++;
}

bool rbuf_pop(ring_buf_t *out) {
    if (!count) return false;

    *out = ring_buffer[tail];
    if (++tail == RING_BUFFER_CAP) tail = 0;
    count--;
    return true;
}
