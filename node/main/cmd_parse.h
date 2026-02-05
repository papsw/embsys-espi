#pragma once
#include "cJSON.h"
#include "telemetry_buf.h"

typedef struct {
    long id;
    char type[32];
    int has_value; int value;   // set_led
    int has_ms;   int ms;      // set_period
} parsed_cmd_t;

// parse command json, returns 1 if valid command found
int parse_command(const char *json, parsed_cmd_t *out);

// build telemetry json string (caller must free)
char *telemetry_to_json(telemetry_t *t);
