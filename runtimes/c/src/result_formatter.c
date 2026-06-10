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
    /* TS CoordinateUtils.coordinateString: strictly > 0 → N/E; 0 → S/W. */
    snprintf(buf, sizeof(buf), "%.3f %c, %.3f %c",
             la < 0 ? -la : la, la > 0 ? 'N' : 'S',
             lo < 0 ? -lo : lo, lo > 0 ? 'E' : 'W');
    push_item(r, "aircraft_position", "POS", "Aircraft Position", buf);
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
void ads_fmt_heading(ads_decode_result_t *r, ads_value_t *v)         { push_numeric(r, v, "heading", "heading", "HDG", "Heading", ""); }
/* ads_fmt_timestamp lives after push_tod below — TS shape is
 * time/TIMESTAMP/"Message Timestamp" with timestampToString display. */
/* TS has no plain `fuel`; alias of currentFuel (fuel_on_board/FOB). */
void ads_fmt_fuel(ads_decode_result_t *r, ads_value_t *v)            { push_numeric(r, v, "fuel_on_board", "fuel_on_board", "FOB", "Fuel On Board", ""); }

static void push_string(ads_decode_result_t *r, ads_value_t *v, const char *raw_key,
                        const char *kind, const char *code, const char *label) {
    const char *s = ads_value_as_string(v);
    if (s) cJSON_AddStringToObject(r->raw, raw_key, s);
    push_item(r, kind, code, label, s ? s : "");
    ads_value_free(v);
}

void ads_fmt_callsign(ads_decode_result_t *r, ads_value_t *v)        { push_string(r, v, "callsign", "callsign", "CALLSIGN", "Callsign"); }
void ads_fmt_flight_number(ads_decode_result_t *r, ads_value_t *v) {
    /* TS no-ops on empty flight numbers. */
    const char *s = ads_value_as_string(v);
    if (!s || !*s) { ads_value_free(v); return; }
    push_string(r, v, "flight_number", "flight_number", "FLIGHT", "Flight Number");
}
void ads_fmt_tail(ads_decode_result_t *r, ads_value_t *v)            { push_string(r, v, "tail", "tail", "TAIL", "Tail"); }
void ads_fmt_departure_airport(ads_decode_result_t *r, ads_value_t *v) { push_string(r, v, "departure_icao", "icao", "ORG", "Origin"); }
void ads_fmt_arrival_airport(ads_decode_result_t *r, ads_value_t *v)   { push_string(r, v, "arrival_icao", "icao", "DST", "Destination"); }

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

/* TS DateTimeUtils.timestampToString:
 *   < 86400   → "HH:MM:SS" (time of day)
 *   < 2678400 → "YYYY-MM-DDTHH:MM:SS" (day known, year/month masked)
 *   otherwise → full ISO-8601 without millis + "Z"
 * Caller frees. */
char *ads_fmt_time_of_day_str(int64_t seconds) {
    char *out = malloc(32);
    if (!out) return NULL;
    if (seconds >= 0 && seconds < 86400) {
        int h = (int)(seconds / 3600);
        int m = (int)((seconds % 3600) / 60);
        int s = (int)(seconds % 60);
        snprintf(out, 32, "%02d:%02d:%02d", h, m, s);
        return out;
    }
    /* Civil-from-days (Howard Hinnant) to render ISO without time.h UTC pain. */
    int64_t days = seconds / 86400;
    int64_t rem = seconds % 86400;
    if (rem < 0) { rem += 86400; days -= 1; }
    int h = (int)(rem / 3600), mi = (int)((rem % 3600) / 60), s = (int)(rem % 60);
    int64_t z = days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    int64_t d = doy - (153 * mp + 2) / 5 + 1;
    int64_t mo = mp < 10 ? mp + 3 : mp - 9;
    if (mo <= 2) y += 1;
    if (seconds < 2678400) {
        snprintf(out, 32, "YYYY-MM-%02lldT%02d:%02d:%02d", (long long)d, h, mi, s);
    } else {
        snprintf(out, 32, "%04lld-%02lld-%02lldT%02d:%02d:%02dZ",
                 (long long)y, (long long)mo, (long long)d, h, mi, s);
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
    push_tod(r, v, "in_time", "time", "IN", "In Gate Time");
}
void ads_fmt_out(ads_decode_result_t *r, ads_value_t *v) {
    push_tod(r, v, "out_time", "time", "OUT", "Out of Gate Time");
}
void ads_fmt_timestamp(ads_decode_result_t *r, ads_value_t *v) {
    push_tod(r, v, "message_timestamp", "time", "TIMESTAMP", "Message Timestamp");
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
void ads_fmt_month(ads_decode_result_t *r, ads_value_t *v)          { push_int_item(r, v, "month", "month", "MSG_MON", "Month of Year"); }

/* ─── Velocity / atmosphere formatters ───────────────────────────────────── */

void ads_fmt_mach(ads_decode_result_t *r, ads_value_t *v)            { push_numeric(r, v, "mach", "mach", "MACH", "Mach Number", "mach"); }
void ads_fmt_groundspeed(ads_decode_result_t *r, ads_value_t *v)     { push_numeric(r, v, "groundspeed", "aircraft_groundspeed", "GSPD", "Aircraft Groundspeed", "knots"); }
void ads_fmt_airspeed(ads_decode_result_t *r, ads_value_t *v)        { push_numeric(r, v, "airspeed", "airspeed", "ASPD", "True Airspeed", "knots"); }
/* TS temperature/totalAirTemp take a STRING and convert M→- / P→+ before
 * Number(); no-op on empty input. Mirrored here. */
void ads_fmt_temperature(ads_decode_result_t *r, const char *value) {
    if (!r || !value || !*value) return;
    char buf[32];
    size_t j = 0;
    for (size_t i = 0; value[i] && j < sizeof(buf) - 1; i++) {
        buf[j++] = value[i] == 'M' ? '-' : (value[i] == 'P' ? '+' : value[i]);
    }
    buf[j] = '\0';
    push_numeric(r, ads_value_from_double(atof(buf)),
                 "outside_air_temperature", "outside_air_temperature",
                 "OATEMP", "Outside Air Temperature (C)", "degrees");
}
void ads_fmt_total_air_temp(ads_decode_result_t *r, const char *value) {
    if (!r || !value || !*value) return;
    char buf[32];
    size_t j = 0;
    for (size_t i = 0; value[i] && j < sizeof(buf) - 1; i++) {
        buf[j++] = value[i] == 'M' ? '-' : (value[i] == 'P' ? '+' : value[i]);
    }
    buf[j] = '\0';
    /* TS uses item type 'temperature' (not 'total_air_temperature') here. */
    push_numeric(r, ads_value_from_double(atof(buf)),
                 "total_air_temperature", "temperature",
                 "TATEMP", "Total Air Temperature (C)", "degrees");
}

/* ─── Fuel formatters ────────────────────────────────────────────────────── */

void ads_fmt_current_fuel(ads_decode_result_t *r, ads_value_t *v)   { push_numeric(r, v, "fuel_on_board", "fuel_on_board", "FOB", "Fuel On Board", ""); }
/* The leading space in " FUEL_REM" is present in the TS source; preserved
 * for byte parity until fixed upstream. */
void ads_fmt_remaining_fuel(ads_decode_result_t *r, ads_value_t *v) { push_numeric(r, v, "fuel_remaining", "fuel_remaining", " FUEL_REM", "Fuel Remaining", ""); }

/* ─── Routing formatters ─────────────────────────────────────────────────── */

void ads_fmt_alternate_airport(ads_decode_result_t *r, ads_value_t *v) { push_string(r, v, "alternate_icao", "icao", "ALT_DST", "Alternate Destination"); }
void ads_fmt_arrival_runway(ads_decode_result_t *r, ads_value_t *v)    { push_string(r, v, "arrival_runway", "runway", "ARWY", "Arrival Runway"); }
void ads_fmt_alternate_runway(ads_decode_result_t *r, ads_value_t *v)  { push_string(r, v, "alternate_runway", "runway", "ALT_ARWY", "Alternate Runway"); }

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
    /* TS: '0x' + ('0000' + hex).slice(-4) — always exactly the last 4
     * lowercase hex chars, zero-padded. */
    char hex[24];
    snprintf(hex, sizeof(hex), "%04llx", (unsigned long long)n);
    size_t len = strlen(hex);
    const char *tail4 = len > 4 ? hex + (len - 4) : hex;
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%s", tail4);
    push_item(r, "message_checksum", "CHECKSUM", "Message Checksum", buf);
    ads_value_free(v);
}

/* TS stores the raw field but pushes NO formatted item (the item block is
 * commented out in the reference). Mirrored exactly. */
void ads_fmt_checksum_algorithm(ads_decode_result_t *r, const char *value) {
    if (!r || !value) return;
    cJSON_AddStringToObject(r->raw, "checksum_algorithm", value);
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
