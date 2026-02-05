// command parsing + telemetry json tests
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../main/cmd_parse.h"

static void test_set_led(void) {
    parsed_cmd_t c;
    const char *json = "{\"command\":{\"id\":42,\"type\":\"set_led\",\"payload\":{\"value\":1}}}";
    assert(parse_command(json, &c) == 1);
    assert(c.id == 42);
    assert(strcmp(c.type, "set_led") == 0);
    assert(c.has_value && c.value == 1);
    printf(" set_led ok\n");
}

static void test_set_led_off(void) {
    parsed_cmd_t c;
    const char *json = "{\"command\":{\"id\":7,\"type\":\"set_led\",\"payload\":{\"value\":0}}}";
    assert(parse_command(json, &c));
    assert(c.value == 0);
    printf(" set_led_off ok\n");
}

static void test_set_period(void) {
    parsed_cmd_t c;
    const char *json = "{\"command\":{\"id\":99,\"type\":\"set_period\",\"payload\":{\"ms\":500}}}";
    assert(parse_command(json, &c));
    assert(c.id == 99);
    assert(strcmp(c.type, "set_period") == 0);
    assert(c.has_ms && c.ms == 500);
    assert(!c.has_value);
    printf(" set_period ok\n");
}

static void test_null_command(void) {
    parsed_cmd_t c;
    const char *json = "{\"command\":null}";
    assert(parse_command(json, &c) == 0);
    printf(" null_command ok\n");
}

static void test_missing_command(void) {
    parsed_cmd_t c;
    assert(parse_command("{}", &c) == 0);
    assert(parse_command("{\"foo\":1}", &c) == 0);
    printf(" missing_command ok\n");
}

static void test_garbage(void) {
    parsed_cmd_t c;
    assert(parse_command("not json", &c) == 0);
    assert(parse_command("", &c) == 0);
    assert(parse_command("{", &c) == 0);
    printf(" garbage ok\n");
}

static void test_missing_fields(void) {
    parsed_cmd_t c;
    // no id
    assert(parse_command("{\"command\":{\"type\":\"set_led\",\"payload\":{\"value\":1}}}", &c) == 0);
    // no type
    assert(parse_command("{\"command\":{\"id\":1,\"payload\":{\"value\":1}}}", &c) == 0);
    // no payload
    assert(parse_command("{\"command\":{\"id\":1,\"type\":\"set_led\"}}", &c) == 0);
    printf(" missing_fields ok\n");
}

static void test_unknown_cmd_type(void) {
    parsed_cmd_t c;
    const char *json = "{\"command\":{\"id\":5,\"type\":\"reboot\",\"payload\":{}}}";
    assert(parse_command(json, &c) == 1);
    assert(strcmp(c.type, "reboot") == 0);
    assert(!c.has_value && !c.has_ms);
    printf(" unknown_type ok\n");
}

static void test_payload_wrong_type(void) {
    parsed_cmd_t c;
    // value is string instead of number
    const char *json = "{\"command\":{\"id\":1,\"type\":\"set_led\",\"payload\":{\"value\":\"yes\"}}}";
    assert(parse_command(json, &c) == 1);
    assert(!c.has_value);
    printf(" payload_wrong_type ok\n");
}

static void test_telemetry_json(void) {
    telemetry_t t = {0};
    t.ts_ms = 1000;
    strncpy(t.device_id, "esp-01", 31);
    strncpy(t.boot_id, "abc123", 15);
    t.seq = 5; t.uptime_ms = 9999; t.rssi = -62; t.free_heap = 180000; t.led = 1;

    char *js = telemetry_to_json(&t);
    assert(js != NULL);
    assert(strstr(js, "\"device_id\":\"esp-01\""));
    assert(strstr(js, "\"boot_id\":\"abc123\""));
    assert(strstr(js, "\"seq\":5"));
    assert(strstr(js, "\"rssi\":-62"));
    assert(strstr(js, "\"free_heap\":180000"));
    assert(strstr(js, "\"led\":1"));
    assert(strstr(js, "\"uptime_ms\":9999"));
    assert(strstr(js, "\"ts_ms\":1000"));
    free(js);
    printf(" telemetry_json ok\n");
}

static void test_telemetry_json_zeros(void) {
    telemetry_t t = {0};
    strncpy(t.device_id, "x", 31);
    char *js = telemetry_to_json(&t);
    assert(js != NULL);
    assert(strstr(js, "\"seq\":0"));
    assert(strstr(js, "\"led\":0"));
    free(js);
    printf(" telemetry_json_zeros ok\n");
}

int main(void) {
    printf("cmd_parse tests\n");

    test_set_led();
    test_set_led_off();
    test_set_period();
    test_null_command();
    test_missing_command();
    test_garbage();
    test_missing_fields();
    test_unknown_cmd_type();
    test_payload_wrong_type();
    test_telemetry_json();
    test_telemetry_json_zeros();

    printf("all ok\n");
    return 0;
}
