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
    ads_fmt_unknown_arr_sep(r, values, count, ",");
}

void ads_fmt_unknown_arr_sep(ads_decode_result_t *r, const char *const *values, size_t count, const char *sep) {
    if (!r || !values || count == 0) return;
    if (!sep) sep = ",";
    size_t sep_len = strlen(sep);
    size_t total = 1;
    for (size_t i = 0; i < count; i++) total += strlen(values[i] ? values[i] : "") + sep_len;
    char *joined = malloc(total);
    if (!joined) return;
    joined[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        if (i > 0) strcat(joined, sep);
        strcat(joined, values[i] ? values[i] : "");
    }
    ads_result_append_remaining(r, joined, sep);
    free(joined);
}

void ads_fmt_unknown(ads_decode_result_t *r, const char *value) {
    ads_result_append_remaining(r, value, ",");
}

void ads_fmt_unknown_sep(ads_decode_result_t *r, const char *value, const char *sep) {
    ads_result_append_remaining(r, value, sep);
}

/* ─── Time-of-day helper ─────────────────────────────────────────────────── */

char *ads_fmt_time_of_day_str(int64_t seconds) {
    char *out = malloc(16);
    if (!out) return NULL;
    if (seconds >= 0 && seconds < 86400) {
        int h = (int)(seconds / 3600);
        int m = (int)((seconds % 3600) / 60);
        int s = (int)(seconds % 60);
        snprintf(out, 16, "%02d:%02d:%02d", h, m, s);
    } else {
        snprintf(out, 16, "%lld", (long long)seconds);
    }
    return out;
}

/* ─── Time-of-day formatters (value in seconds since midnight) ───────────── */

static void push_tod(ads_decode_result_t *r, ads_value_t *v, const char *raw_key,
                     const char *kind, const char *code, const char *label) {
    if (!v) return;
    int64_t n = 0;
    ads_value_as_int(v, &n);
    cJSON_AddNumberToObject(r->raw, raw_key, (double)n);
    char *display = ads_fmt_time_of_day_str(n);
    push_item(r, kind, code, label, display ? display : "");
    free(display);
    ads_value_free(v);
}

void ads_fmt_eta(ads_decode_result_t *r, ads_value_t *v) {
    push_tod(r, v, "eta_time", "time", "ETA", "Estimated Time of Arrival");
}
void ads_fmt_off(ads_decode_result_t *r, ads_value_t *v) {
    push_tod(r, v, "off_time", "time", "OFF", "Takeoff Time");
}
void ads_fmt_on(ads_decode_result_t *r, ads_value_t *v) {
    push_tod(r, v, "on_time", "time", "ON", "Landing Time");
}
void ads_fmt_in(ads_decode_result_t *r, ads_value_t *v) {
    push_tod(r, v, "in_time", "time", "IN", "Arrival at Gate");
}
void ads_fmt_out(ads_decode_result_t *r, ads_value_t *v) {
    push_tod(r, v, "out_time", "time", "OUT", "Departure from Gate");
}

/* ─── Calendar formatters ────────────────────────────────────────────────── */

static void push_int_item(ads_decode_result_t *r, ads_value_t *v, const char *raw_key,
                          const char *kind, const char *code, const char *label) {
    if (!v) return;
    int64_t n = 0;
    ads_value_as_int(v, &n);
    cJSON_AddNumberToObject(r->raw, raw_key, (double)n);
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)n);
    push_item(r, kind, code, label, buf);
    ads_value_free(v);
}

void ads_fmt_day(ads_decode_result_t *r, ads_value_t *v)            { push_int_item(r, v, "day", "day", "MSG_DAY", "Day of Month"); }
void ads_fmt_departure_day(ads_decode_result_t *r, ads_value_t *v)  { push_int_item(r, v, "departure_day", "day", "DEP_DAY", "Departure Day"); }
void ads_fmt_arrival_day(ads_decode_result_t *r, ads_value_t *v)    { push_int_item(r, v, "arrival_day", "day", "ARR_DAY", "Arrival Day"); }
void ads_fmt_month(ads_decode_result_t *r, ads_value_t *v)          { push_int_item(r, v, "month", "month", "MSG_MONTH", "Month"); }

/* ─── Velocity / atmosphere formatters ───────────────────────────────────── */

void ads_fmt_mach(ads_decode_result_t *r, ads_value_t *v)            { push_numeric(r, v, "mach", "mach", "MACH", "Mach", ""); }
void ads_fmt_groundspeed(ads_decode_result_t *r, ads_value_t *v)     { push_numeric(r, v, "groundspeed", "groundspeed", "GS", "Ground Speed", "knots"); }
void ads_fmt_airspeed(ads_decode_result_t *r, ads_value_t *v)        { push_numeric(r, v, "airspeed", "airspeed", "IAS", "Indicated Airspeed", "knots"); }
void ads_fmt_temperature(ads_decode_result_t *r, ads_value_t *v)     { push_numeric(r, v, "outside_air_temperature", "outside_air_temperature", "OATEMP", "Outside Air Temperature (C)", "degrees"); }
void ads_fmt_total_air_temp(ads_decode_result_t *r, ads_value_t *v)  { push_numeric(r, v, "total_air_temperature", "total_air_temperature", "TATEMP", "Total Air Temperature (C)", "degrees"); }

/* ─── Fuel formatters ────────────────────────────────────────────────────── */

void ads_fmt_current_fuel(ads_decode_result_t *r, ads_value_t *v)   { push_numeric(r, v, "fuel_on_board", "fuel_on_board", "FOB", "Fuel On Board", ""); }
void ads_fmt_remaining_fuel(ads_decode_result_t *r, ads_value_t *v) { push_numeric(r, v, "fuel_remaining", "fuel_remaining", "FUEL_REM", "Fuel Remaining", ""); }

/* ─── Routing formatters ─────────────────────────────────────────────────── */

void ads_fmt_alternate_airport(ads_decode_result_t *r, ads_value_t *v) { push_string(r, v, "alternate_icao", "icao", "ALT_DST", "Alternate Destination"); }
void ads_fmt_arrival_runway(ads_decode_result_t *r, ads_value_t *v)    { push_string(r, v, "arrival_runway", "runway", "ARWY", "Arrival Runway"); }
void ads_fmt_alternate_runway(ads_decode_result_t *r, ads_value_t *v)  { push_string(r, v, "alternate_runway", "runway", "ALT_RWY", "Alternate Runway"); }

/* ─── Event formatters ───────────────────────────────────────────────────── */

void ads_fmt_state_change(ads_decode_result_t *r, const char *from, const char *to) {
    if (!r || !from || !to) return;
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "from", from);
    cJSON_AddStringToObject(obj, "to", to);
    cJSON_AddItemToObject(r->raw, "state_change", obj);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s -> %s", from, to);
    push_item(r, "state_change", "STATE_CHANGE", "State Change", buf);
}

void ads_fmt_door_event(ads_decode_result_t *r, const char *door, const char *state) {
    if (!r || !door || !state) return;
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "door", door);
    cJSON_AddStringToObject(obj, "state", state);
    cJSON_AddItemToObject(r->raw, "door_event", obj);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s %s", door, state);
    push_item(r, "door_event", "DOOR", "Door Event", buf);
}

/* ─── Free-text formatters ───────────────────────────────────────────────── */

void ads_fmt_text(ads_decode_result_t *r, const char *value) {
    if (!r || !value) return;
    cJSON_AddStringToObject(r->raw, "text", value);
    push_item(r, "text", "TEXT", "Text Message", value);
}

/* ─── Diagnostic formatters ──────────────────────────────────────────────── */

void ads_fmt_checksum(ads_decode_result_t *r, ads_value_t *v) {
    if (!v) return;
    int64_t n = 0;
    ads_value_as_int(v, &n);
    cJSON_AddNumberToObject(r->raw, "checksum", (double)n);
    char buf[32];
    snprintf(buf, sizeof(buf), "%llx", (long long)n);
    push_item(r, "checksum", "CKSUM", "Checksum", buf);
    ads_value_free(v);
}

void ads_fmt_checksum_algorithm(ads_decode_result_t *r, const char *value) {
    if (!r || !value) return;
    cJSON_AddStringToObject(r->raw, "checksum_algorithm", value);
    push_item(r, "checksum_algorithm", "CKSUM_ALGO", "Checksum Algorithm", value);
}

/* ─── Generic structured-item push ───────────────────────────────────────── */

void ads_fmt_push_item(ads_decode_result_t *r, const char *type, const char *code,
                       const char *label, const char *value) {
    if (!r) return;
    push_item(r,
              type  ? type  : "unknown",
              code  ? code  : "",
              label ? label : "",
              value ? value : "");
}

/* ─── Result mutators (declared in ads_helpers.h) ────────────────────────── */

void ads_result_set_description(ads_decode_result_t *r, const char *description) {
    if (!r) return;
    free(r->description);
    r->description = description ? strdup(description) : NULL;
}

void ads_result_set_remaining(ads_decode_result_t *r, const char *text) {
    if (!r) return;
    free(r->remaining);
    r->remaining = text ? strdup(text) : NULL;
}

void ads_result_append_remaining(ads_decode_result_t *r, const char *text, const char *sep) {
    if (!r || !text) return;
    if (!sep) sep = ",";
    if (r->remaining) {
        size_t newlen = strlen(r->remaining) + strlen(sep) + strlen(text) + 1;
        char *grown = malloc(newlen);
        if (!grown) return;
        snprintf(grown, newlen, "%s%s%s", r->remaining, sep, text);
        free(r->remaining);
        r->remaining = grown;
    } else {
        r->remaining = strdup(text);
    }
}

const char *ads_result_get_remaining(const ads_decode_result_t *r) {
    return r ? r->remaining : NULL;
}

void ads_result_set_decode_level(ads_decode_result_t *r, ads_decode_level_t level) {
    if (!r) return;
    r->decode_level = level;
}

void ads_result_clear_items(ads_decode_result_t *r) {
    if (!r) return;
    cJSON_Delete(r->items);
    r->items = cJSON_CreateArray();
}
