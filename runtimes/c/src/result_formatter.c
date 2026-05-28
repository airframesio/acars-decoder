/* ResultFormatter — pushes formatted items into the cJSON items array on
 * the result. Field names match TS/Rust ResultFormatter output. */

#include "ads_helpers.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ads_value { cJSON *node; bool owns; };

/* Access the result's items array. result is opaque elsewhere, so we share
 * the layout here via a forward declaration matching plugin.c. */
struct ads_decode_result {
    bool decoded;
    char *plugin_name;
    char *description;
    int decode_level;
    cJSON *raw;
    cJSON *items;
    char *remaining;
    char *message_json;
};

static void push_item(ads_decode_result_t *r, const char *kind, const char *code,
                      const char *label, const char *value) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "type", kind);
    cJSON_AddStringToObject(item, "code", code);
    cJSON_AddStringToObject(item, "label", label);
    cJSON_AddStringToObject(item, "value", value);
    cJSON_AddItemToArray(r->items, item);
}

void ads_fmt_position(ads_decode_result_t *r, ads_value_t *lat, ads_value_t *lon) {
    double la = 0, lo = 0;
    ads_value_as_double(lat, &la);
    ads_value_as_double(lon, &lo);
    cJSON *pos = cJSON_CreateObject();
    cJSON_AddNumberToObject(pos, "latitude", la);
    cJSON_AddNumberToObject(pos, "longitude", lo);
    cJSON_AddItemToObject(r->raw, "position", pos);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.3f %c, %.3f %c",
             la < 0 ? -la : la, la < 0 ? 'S' : 'N',
             lo < 0 ? -lo : lo, lo < 0 ? 'W' : 'E');
    push_item(r, "aircraft_position", "ARP", "Aircraft Position", buf);
    ads_value_free(lat); ads_value_free(lon);
}

void ads_fmt_position_value(ads_decode_result_t *r, ads_value_t *position) {
    double la = 0, lo = 0;
    if (position && position->node) {
        const cJSON *l = cJSON_GetObjectItemCaseSensitive(position->node, "latitude");
        const cJSON *g = cJSON_GetObjectItemCaseSensitive(position->node, "longitude");
        if (cJSON_IsNumber(l)) la = l->valuedouble;
        if (cJSON_IsNumber(g)) lo = g->valuedouble;
    }
    ads_fmt_position(r, ads_value_from_double(la), ads_value_from_double(lo));
    ads_value_free(position);
}

static void push_numeric(ads_decode_result_t *r, ads_value_t *v, const char *raw_key,
                         const char *kind, const char *code, const char *label, const char *unit) {
    double n = 0;
    ads_value_as_double(v, &n);
    cJSON_AddNumberToObject(r->raw, raw_key, n);
    char buf[64];
    if (unit && *unit) snprintf(buf, sizeof(buf), "%g %s", n, unit);
    else               snprintf(buf, sizeof(buf), "%g", n);
    push_item(r, kind, code, label, buf);
    ads_value_free(v);
}

void ads_fmt_altitude(ads_decode_result_t *r, ads_value_t *v)        { push_numeric(r, v, "altitude", "altitude", "ALT", "Altitude", "feet"); }
void ads_fmt_speed(ads_decode_result_t *r, ads_value_t *v)           { push_numeric(r, v, "speed", "speed", "SPD", "Speed", "knots"); }
void ads_fmt_heading(ads_decode_result_t *r, ads_value_t *v)         { push_numeric(r, v, "heading", "heading", "HDG", "Heading", "deg"); }
void ads_fmt_timestamp(ads_decode_result_t *r, ads_value_t *v)       { push_numeric(r, v, "message_timestamp", "timestamp", "TS", "Timestamp", ""); }
void ads_fmt_fuel(ads_decode_result_t *r, ads_value_t *v)            { push_numeric(r, v, "fuel_on_board", "fuel", "FUEL", "Fuel", ""); }

static void push_string(ads_decode_result_t *r, ads_value_t *v, const char *raw_key,
                        const char *kind, const char *code, const char *label) {
    const char *s = ads_value_as_string(v);
    if (s) cJSON_AddStringToObject(r->raw, raw_key, s);
    push_item(r, kind, code, label, s ? s : "");
    ads_value_free(v);
}

void ads_fmt_callsign(ads_decode_result_t *r, ads_value_t *v)        { push_string(r, v, "callsign", "callsign", "CS", "Callsign"); }
void ads_fmt_flight_number(ads_decode_result_t *r, ads_value_t *v)   { push_string(r, v, "flight_number", "flight_number", "FLT", "Flight Number"); }
void ads_fmt_tail(ads_decode_result_t *r, ads_value_t *v)            { push_string(r, v, "tail", "tail", "TAIL", "Tail Number"); }
void ads_fmt_departure_airport(ads_decode_result_t *r, ads_value_t *v) { push_string(r, v, "departure_icao", "airport_origin", "DEP", "Origin"); }
void ads_fmt_arrival_airport(ads_decode_result_t *r, ads_value_t *v)   { push_string(r, v, "arrival_icao", "airport_destination", "ARR", "Destination"); }

void ads_fmt_unknown_arr(ads_decode_result_t *r, const char *const *values, size_t count) {
    size_t total = 1;
    for (size_t i = 0; i < count; i++) total += strlen(values[i] ? values[i] : "") + 1;
    char *joined = malloc(total);
    if (!joined) return;
    joined[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        if (i > 0) strcat(joined, ",");
        strcat(joined, values[i] ? values[i] : "");
    }
    if (r->remaining) {
        size_t newlen = strlen(r->remaining) + 1 + strlen(joined) + 1;
        char *grown = malloc(newlen);
        snprintf(grown, newlen, "%s,%s", r->remaining, joined);
        free(r->remaining);
        r->remaining = grown;
    } else {
        r->remaining = strdup(joined);
    }
    free(joined);
}
