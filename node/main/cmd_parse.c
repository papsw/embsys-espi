#include <string.h>
#include "cmd_parse.h"

int parse_command(const char *json, parsed_cmd_t *out) {
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_Parse(json);
    if (!root) return 0;

    cJSON *cmd = cJSON_GetObjectItem(root, "command");
    if (!cmd || cJSON_IsNull(cmd)) { cJSON_Delete(root); return 0; }

    cJSON *id = cJSON_GetObjectItem(cmd, "id");
    cJSON *tp = cJSON_GetObjectItem(cmd, "type");
    cJSON *pl = cJSON_GetObjectItem(cmd, "payload");
    if (!cJSON_IsNumber(id) || !cJSON_IsString(tp) || !cJSON_IsObject(pl)) {
        cJSON_Delete(root);
        return 0;
    }

    out->id = (long)id->valuedouble;
    strncpy(out->type, tp->valuestring, 31);

    if (strcmp(out->type, "set_led") == 0) {
        cJSON *v = cJSON_GetObjectItem(pl, "value");
        if (cJSON_IsNumber(v)) { out->has_value = 1; out->value = (int)v->valuedouble; }
    } else if (strcmp(out->type, "set_period") == 0) {
        cJSON *m = cJSON_GetObjectItem(pl, "ms");
        if (cJSON_IsNumber(m)) { out->has_ms = 1; out->ms = (int)m->valuedouble; }
    }

    cJSON_Delete(root);
    return 1;
}

char *telemetry_to_json(telemetry_t *t) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "device_id", t->device_id);
    cJSON_AddStringToObject(j, "boot_id", t->boot_id);
    cJSON_AddNumberToObject(j, "seq", t->seq);
    cJSON_AddNumberToObject(j, "uptime_ms", t->uptime_ms);
    cJSON_AddNumberToObject(j, "rssi", t->rssi);
    cJSON_AddNumberToObject(j, "free_heap", t->free_heap);
    cJSON_AddNumberToObject(j, "led", t->led);
    cJSON_AddNumberToObject(j, "ts_ms", (double)t->ts_ms);
    char *s = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    return s;
}
