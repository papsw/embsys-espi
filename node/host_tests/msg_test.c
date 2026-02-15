#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/msg.h"

void test_parse(void);
void test_build(int task_id, int hits, int total, int compute_ms);
void test_heartbeat(const char *worker_id, const char *status, int uptime_ms,
     int free_heap, short rssi, int buf_count);

int main(void) {
    test_parse();

    for (int i = 0; i < 100; i++)
        test_build(i, i*10, i*100, i*10);
    
    const char *statuses[] = {"idle", "computing", "error"};
    for (int i=0; i<3; i++)
        for (int j=0; j<100; j++)
            test_heartbeat("esp-01", statuses[i], j*1000, 280000-j*1000, -50-j, j);

    printf("\033[32mAll json tests passed!\033[0m\n");
    return 0;
}

void test_parse(void) {
    task_t out;
    char good[256];

    const char *bad[] = {
        "",
        "im not json",
        "{",
        "{}",
        "null",
        "[]",
        "{\"type\":\"ping\"}",
        "{\"type\":\"pong\"}",
        "{\"type\":\"result\"}",
        "{\"type\":123}",
        "{\"type\":\"task\"}",
        "{\"type\":\"task\",\"task\":null}",
        "{\"type\":\"task\",\"task\":\"hello\"}",
        "{\"type\":\"task\",\"task\":42}",
        "{\"type\":\"task\",\"task\":[]}",
        "{\"type\":\"task\",\"task\":{}}",
        "{\"type\":\"task\",\"task\":{\"id\":7}}",
        "{\"type\":\"task\",\"task\":{\"type\":\"monte_carlo_pi\"}}",
        "{\"type\":\"task\",\"task\":{\"samples\":100}}",
        "{\"type\":\"task\",\"task\":{\"id\":7,\"type\":\"monte_carlo_pi\"}}",
        "{\"type\":\"task\",\"task\":{\"id\":7,\"samples\":100}}",
        "{\"type\":\"task\",\"task\":{\"type\":\"monte_carlo_pi\",\"samples\":100}}",
        "{\"type\":\"task\",\"task\":{\"id\":\"seven\",\"type\":\"monte_carlo_pi\",\"samples\":100}}",
        "{\"type\":\"task\",\"task\":{\"id\":7,\"type\":123,\"samples\":100}}",
        "{\"type\":\"task\",\"task\":{\"id\":7,\"type\":\"monte_carlo_pi\",\"samples\":\"hundred\"}}",
    };

    for (int i = 0; i < (int)(sizeof(bad)/sizeof(bad[0])); i++) {
        assert(parse_task_msg(bad[i], &out) == false);
    }


    for (int id = 1; id <= 20; id++) {
        sprintf(good,
            "{\"type\":\"task\",\"task\":{\"id\":%d,\"type\":\"monte_carlo_pi\",\"samples\":%d}}",
            id, id * 1000);
        assert(parse_task_msg(good, &out) == true);
        assert(out.id == id);
        assert(strcmp(out.type, "monte_carlo_pi") == 0);
        assert(out.samples == id * 1000);
    }
}

void test_build(int task_id, int hits, int total, int compute_ms) {
    ring_buf_t in = {
        .task_id = task_id,
        .hits = hits,
        .total = total,
        .compute_ms = compute_ms
    };

    char *json = build_json_msg(&in, "esp-01");
    char expect[64];

    sprintf(expect, "\"task_id\":%d", task_id);
    assert(strstr(json, expect));

    sprintf(expect, "\"hits\":%d", hits);
    assert(strstr(json, expect));

    sprintf(expect, "\"total\":%d", total);
    assert(strstr(json, expect));

    sprintf(expect, "\"compute_ms\":%d", compute_ms);
    assert(strstr(json, expect));

    assert(strstr(json, "\"worker_id\":\"esp-01\""));
    free(json);
}

void test_heartbeat(const char *worker_id, const char *status, int uptime_ms,
     int free_heap, short rssi, int buf_count) {
    char *json = heartbeat_json(worker_id, status, uptime_ms, free_heap, rssi, buf_count);
    char expect[64];

    sprintf(expect, "\"worker_id\":\"%s\"", worker_id);
    assert(strstr(json, expect));

    sprintf(expect, "\"status\":\"%s\"", status);
    assert(strstr(json, expect));

    sprintf(expect, "\"uptime_ms\":%d", uptime_ms);
    assert(strstr(json, expect));

    sprintf(expect, "\"free_heap\":%d", free_heap);
    assert(strstr(json, expect));

    sprintf(expect, "\"rssi\":%d", rssi);
    assert(strstr(json, expect));

    sprintf(expect, "\"buf_count\":%d", buf_count);
    assert(strstr(json, expect));

    free(json);
}