// ringbuf tests
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../main/telemetry_buf.h"

static telemetry_t mkt(int n) {
    telemetry_t t = {0};
    t.seq = n;
    t.ts_ms = n * 1000LL;
    t.uptime_ms = n * 10;
    t.rssi = -50;
    t.free_heap = 80000;
    t.led = n & 1;
    snprintf(t.device_id, 32, "esp-%d", n);
    snprintf(t.boot_id, 16, "b%d", n % 100);
    return t;
}   

static void test_empty(void) {
    telemetry_t tmp;
    telemetry_buf_reset();
    assert(telemetry_buf_pop(&tmp) == 0);
    assert(telemetry_buf_count() == 0);
    printf(" empty ok\n");
}

static void test_basic(void) {
    telemetry_t tmp, t1;
    telemetry_buf_reset();
    t1 = mkt(42);
    telemetry_buf_push(&t1);
    assert(telemetry_buf_pop(&tmp));
    assert(tmp.seq == 42);
    printf(" basic ok\n");
}

static void test_fifo(void) {
    telemetry_t tmp, t1, t2, t3;
    telemetry_buf_reset();
    t1 = mkt(1);
    t2 = mkt(2);
    t3 = mkt(3);
    telemetry_buf_push(&t1);
    telemetry_buf_push(&t2);
    telemetry_buf_push(&t3);
    telemetry_buf_pop(&tmp);
    assert(tmp.seq == 1);
    telemetry_buf_pop(&tmp);
    assert(tmp.seq == 2);
    telemetry_buf_pop(&tmp);
    assert(tmp.seq == 3);
    printf(" fifo ok\n");
}

static void test_fill(void) {
    telemetry_t tmp, t1;
    telemetry_buf_reset();
    for (int i = 0; i < TELEMETRY_BUF_CAP; i++) {
        t1 = mkt(i);
        telemetry_buf_push(&t1);
    }
    assert(telemetry_buf_count() == TELEMETRY_BUF_CAP);
    for (int i = 0; i < TELEMETRY_BUF_CAP; i++) {
        telemetry_buf_pop(&tmp);
        assert(tmp.seq == (uint32_t)i);
    }
    printf(" fill ok\n");
}

static void test_overflow(void) {
    telemetry_t tmp, t1;
    telemetry_buf_reset();
    for (int i = 0; i < TELEMETRY_BUF_CAP; i++) {
        t1 = mkt(i);
        telemetry_buf_push(&t1);
    }
    t1 = mkt(9999);
    telemetry_buf_push(&t1);
    telemetry_buf_pop(&tmp);
    assert(tmp.seq == 1);
    printf(" overflow ok\n");
}

static void test_overflow_many(void) {
    telemetry_t tmp, t1;
    telemetry_buf_reset();
    for (int i = 0; i < TELEMETRY_BUF_CAP + 37; i++) {
        t1 = mkt(i);
        telemetry_buf_push(&t1);
    }
    telemetry_buf_pop(&tmp);
    assert(tmp.seq == 37);
    printf(" overflow many ok\n");
}

static void test_wrap(void) {
    telemetry_t tmp, t1;
    telemetry_buf_reset();
    for (int i = 0; i < 150; i++) {
        t1 = mkt(i);
        telemetry_buf_push(&t1);
    }
    for (int i = 0; i < 100; i++)
        telemetry_buf_pop(&tmp);
    for (int i = 500; i < 500 + TELEMETRY_BUF_CAP; i++) { t1 = mkt(i); telemetry_buf_push(&t1); }
    telemetry_buf_pop(&tmp);
    assert(tmp.seq == 500);
    printf(" wrap ok\n");
}

static void test_reset(void) {
    telemetry_t t1;
    telemetry_buf_reset();
    for (int i = 0; i < 25; i++) { t1 = mkt(i); telemetry_buf_push(&t1); }
    telemetry_buf_reset();
    assert(telemetry_buf_count() == 0);
    printf(" reset ok\n");
}

static void test_strings(void) {
    telemetry_t tmp, t1;
    telemetry_buf_reset();
    t1 = mkt(7777); telemetry_buf_push(&t1); telemetry_buf_pop(&tmp);
    assert(strcmp(tmp.device_id, "esp-7777") == 0);
    assert(tmp.ts_ms == 7777000LL);
    printf(" strings ok\n");
}

static void test_random(void) {
    telemetry_t tmp, t1, t2;
    telemetry_buf_reset();
    telemetry_t mdl[TELEMETRY_BUF_CAP];
    int mh = 0, mt = 0, mc = 0;
    unsigned sd = 12345;
    int sq = 1;
    for (int op = 0; op < 5000; op++) {
        sd = sd * 1103515245 + 12345;
        if (((sd >> 16) % 100) < 60) {
            t1 = mkt(sq++);
            telemetry_buf_push(&t1);
            if (mc == TELEMETRY_BUF_CAP) { mt = (mt + 1) % TELEMETRY_BUF_CAP; mc--; }
            mdl[mh] = t1; mh = (mh + 1) % TELEMETRY_BUF_CAP; mc++;
        } else {
            int r = telemetry_buf_pop(&tmp);
            int mr = mc > 0;
            if (mr) { t2 = mdl[mt]; mt = (mt + 1) % TELEMETRY_BUF_CAP; mc--; }
            assert(r == mr);
            if (r) assert(tmp.seq == t2.seq);
        }
        assert(telemetry_buf_count() == mc);
    }
    printf(" random ok\n");
}

static void test_cycles(void) {
    telemetry_t tmp, t1;
    int cnt;
    for (int cy = 0; cy < 50; cy++) {
        telemetry_buf_reset();
        for (int i = 0; i < TELEMETRY_BUF_CAP; i++) { t1 = mkt(cy * 1000 + i); telemetry_buf_push(&t1); }
        cnt = 0;
        while (telemetry_buf_pop(&tmp)) cnt++;
        assert(cnt == TELEMETRY_BUF_CAP);
    }
    printf(" cycles ok\n");
}

int main(void) {
    printf("ringbuf tests\n");

    test_empty();
    test_basic();
    test_fifo();
    test_fill();
    test_overflow();
    test_overflow_many();
    test_wrap();
    test_reset();
    test_strings();
    test_random();
    test_cycles();

    printf("all ok\n");
    return 0;
}
