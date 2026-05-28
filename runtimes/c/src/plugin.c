/* DecodeResult lifecycle + value helpers backed by cJSON. */

#include "ads_runtime.h"
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>

struct ads_value {
    cJSON *node;
    bool owns;
};

struct ads_decode_result {
    bool decoded;
    char *plugin_name;
    char *description;
    ads_decode_level_t decode_level;
    cJSON *raw;       /* object */
    cJSON *items;     /* array of {kind, code, label, value} */
    char *remaining;  /* may be NULL */
    char *message_json; /* serialized snapshot if set_message was called */
};

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    char *r = malloc(strlen(s) + 1);
    if (r) strcpy(r, s);
    return r;
}

ads_decode_result_t *ads_result_new(const char *plugin_name,
                                    const char *description,
                                    const ads_message_t *msg) {
    ads_decode_result_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->plugin_name = xstrdup(plugin_name);
    r->description = xstrdup(description);
    r->decode_level = ADS_DECODE_LEVEL_NONE;
    r->raw = cJSON_CreateObject();
    r->items = cJSON_CreateArray();
    if (msg) {
        cJSON *m = cJSON_CreateObject();
        cJSON_AddStringToObject(m, "label", msg->label ? msg->label : "");
        if (msg->sublabel) cJSON_AddStringToObject(m, "sublabel", msg->sublabel);
        cJSON_AddStringToObject(m, "text", msg->text ? msg->text : "");
        r->message_json = cJSON_PrintUnformatted(m);
        cJSON_Delete(m);
    }
    return r;
}

void ads_result_free(ads_decode_result_t *result) {
    if (!result) return;
    free(result->plugin_name);
    free(result->description);
    cJSON_Delete(result->raw);
    cJSON_Delete(result->items);
    free(result->remaining);
    free(result->message_json);
    free(result);
}

void ads_result_set_decoded(ads_decode_result_t *r, bool decoded) {
    r->decoded = decoded;
    if (!decoded) r->decode_level = ADS_DECODE_LEVEL_NONE;
    else r->decode_level = r->remaining ? ADS_DECODE_LEVEL_PARTIAL : ADS_DECODE_LEVEL_FULL;
}

ads_decode_result_t *ads_result_fail_unknown(ads_decode_result_t *r, const char *text) {
    r->decoded = false;
    r->decode_level = ADS_DECODE_LEVEL_NONE;
    free(r->remaining);
    r->remaining = xstrdup(text);
    return r;
}

void ads_result_raw_set(ads_decode_result_t *r, const char *key, ads_value_t *value) {
    if (!r || !key || !value || !value->node) return;
    /* Detach the cJSON node from the value wrapper (transfer ownership). */
    cJSON *detached = cJSON_Duplicate(value->node, 1);
    cJSON_AddItemToObject(r->raw, key, detached);
    ads_value_free(value);
}

ads_value_t *ads_result_raw_get(const ads_decode_result_t *r, const char *key) {
    cJSON *node = cJSON_GetObjectItemCaseSensitive(r->raw, key);
    if (!node) return NULL;
    ads_value_t *v = calloc(1, sizeof(*v));
    if (!v) return NULL;
    v->node = node;     /* borrowed pointer */
    v->owns = false;
    return v;
}

char *ads_result_to_json(const ads_decode_result_t *r) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "decoded", r->decoded);
    cJSON *decoder = cJSON_CreateObject();
    cJSON_AddStringToObject(decoder, "name", r->plugin_name);
    cJSON_AddStringToObject(decoder, "type", "pattern-match");
    const char *level = r->decode_level == ADS_DECODE_LEVEL_FULL ? "full"
                      : r->decode_level == ADS_DECODE_LEVEL_PARTIAL ? "partial"
                      : "none";
    cJSON_AddStringToObject(decoder, "decodeLevel", level);
    cJSON_AddItemToObject(root, "decoder", decoder);

    cJSON *formatted = cJSON_CreateObject();
    cJSON_AddStringToObject(formatted, "description", r->description);
    cJSON_AddItemReferenceToObject(formatted, "items", r->items);
    cJSON_AddItemToObject(root, "formatted", formatted);

    cJSON_AddItemReferenceToObject(root, "raw", r->raw);

    if (r->remaining) {
        cJSON *rem = cJSON_CreateObject();
        cJSON_AddStringToObject(rem, "text", r->remaining);
        cJSON_AddItemToObject(root, "remaining", rem);
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

/* ─── ads_value_t ────────────────────────────────────────────────────────── */

static ads_value_t *make_value(cJSON *node) {
    ads_value_t *v = calloc(1, sizeof(*v));
    if (!v) { cJSON_Delete(node); return NULL; }
    v->node = node;
    v->owns = true;
    return v;
}

ads_value_t *ads_value_from_string(const char *s) { return make_value(cJSON_CreateString(s ? s : "")); }
ads_value_t *ads_value_from_double(double n)      { return make_value(cJSON_CreateNumber(n)); }
ads_value_t *ads_value_from_int(int64_t n)        { return make_value(cJSON_CreateNumber((double)n)); }
ads_value_t *ads_value_from_bool(bool b)          { return make_value(cJSON_CreateBool(b)); }
ads_value_t *ads_value_null(void)                 { return make_value(cJSON_CreateNull()); }

void ads_value_free(ads_value_t *v) {
    if (!v) return;
    if (v->owns && v->node) cJSON_Delete(v->node);
    free(v);
}

bool ads_value_as_double(const ads_value_t *v, double *out) {
    if (!v || !cJSON_IsNumber(v->node)) return false;
    *out = v->node->valuedouble;
    return true;
}
bool ads_value_as_int(const ads_value_t *v, int64_t *out) {
    if (!v || !cJSON_IsNumber(v->node)) return false;
    *out = (int64_t)v->node->valuedouble;
    return true;
}
const char *ads_value_as_string(const ads_value_t *v) {
    if (!v || !cJSON_IsString(v->node)) return NULL;
    return v->node->valuestring;
}

void ads_str_list_free(ads_str_list_t *list) {
    if (!list || !list->items) return;
    for (size_t i = 0; i < list->count; i++) free(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = list->capacity = 0;
}

void ads_bytes_free(ads_bytes_t *b) {
    if (!b) return;
    free(b->data);
    b->data = NULL;
    b->len = 0;
}
