#include <cJSON.h>
#include <string.h>

#include <ring_buf.h>
#include <msg.h>

bool parse_task_msg(const char *json, task_t *out) {
    memset(out, 0, sizeof(task_t));

    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    cJSON *msg_type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(msg_type) || strcmp(msg_type->valuestring, "task") != 0) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *task_json = cJSON_GetObjectItem(root, "task");
    if (!task_json || !cJSON_IsObject(task_json)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *id      = cJSON_GetObjectItem(task_json, "id");
    cJSON *tp      = cJSON_GetObjectItem(task_json, "type");
    cJSON *samples = cJSON_GetObjectItem(task_json, "samples");

    if (!cJSON_IsNumber(id) || !cJSON_IsString(tp) || !cJSON_IsNumber(samples)) {
        cJSON_Delete(root);
        return false;
    }

    // cjson's only data type is double, so it needs to be cast as the esp has no second fpu
    out->id = id->valueint;
    strncpy(out->type, tp->valuestring, sizeof(out->type)-1);
    out->samples = samples->valueint;

    cJSON_Delete(root);
    return true;
}

char *build_json_msg (const ring_buf_t *in, const char *worker_id) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "type", "result");
    cJSON_AddStringToObject(obj, "worker_id", worker_id);
    cJSON_AddNumberToObject(obj, "task_id", in->task_id);
    cJSON_AddNumberToObject(obj, "hits", in->hits);
    cJSON_AddNumberToObject(obj, "total", in->total);
    cJSON_AddNumberToObject(obj, "compute_ms", in->compute_ms);
    char *json_str = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return json_str;
}

char *heartbeat_json (const char *worker_id, const char *status, int uptime_ms,
     int free_heap, short rssi, int buf_count) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "type", "heartbeat");
    cJSON_AddStringToObject(obj, "worker_id", worker_id);
    cJSON_AddStringToObject(obj, "status", status);
    cJSON_AddNumberToObject(obj, "uptime_ms", uptime_ms);
    cJSON_AddNumberToObject(obj, "free_heap", free_heap);
    cJSON_AddNumberToObject(obj, "rssi", rssi);
    cJSON_AddNumberToObject(obj, "buf_count", buf_count);
    char *json_str = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    return json_str;
}