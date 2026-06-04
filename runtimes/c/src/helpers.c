/* Decode-fn helpers (numerics, strings, identifiers, encodings, bitfields).
 *
 * Uniform signature: every helper accepts (value, args_json). Args-free
 * helpers ignore the second arg. Matches the Rust runtime's pattern.
 */

#include "ads_helpers.h"
#include <cjson/cJSON.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ads_value { cJSON *node; bool owns; };

static char *substr_copy(const char *s, size_t start, size_t len) {
    size_t slen = strlen(s);
    if (start >= slen) return strdup("");
    if (start + len > slen) len = slen - start;
    char *out = malloc(len + 1);
    memcpy(out, s + start, len);
    out[len] = '\0';
    return out;
}

ads_value_t *ads_decode_integer(const char *value, const char *args_json) {
    cJSON *args = args_json ? cJSON_Parse(args_json) : NULL;
    size_t start = 0;
    long len = -1;
    double mult = 1.0;
    if (args) {
        cJSON *ss = cJSON_GetObjectItemCaseSensitive(args, "substring_start");
        cJSON *sl = cJSON_GetObjectItemCaseSensitive(args, "substring_length");
        cJSON *m  = cJSON_GetObjectItemCaseSensitive(args, "multiplier");
        if (cJSON_IsNumber(ss)) start = (size_t)ss->valuedouble;
        if (cJSON_IsNumber(sl)) len = (long)sl->valuedouble;
        if (cJSON_IsNumber(m))  mult = m->valuedouble;
    }
    cJSON_Delete(args);
    if (!value) return ads_value_from_double(0.0);
    char *s = len >= 0 ? substr_copy(value, start, (size_t)len)
                       : substr_copy(value, start, strlen(value));
    double n = atof(s) * mult;
    free(s);
    return ads_value_from_double(n);
}

ads_value_t *ads_decode_float(const char *value, const char *args_json) {
    (void)args_json;
    return ads_value_from_double(value ? atof(value) : 0.0);
}

ads_value_t *ads_decode_string(const char *value, const char *args_json) {
    (void)args_json;
    return ads_value_from_string(value ? value : "");
}

ads_value_t *ads_decode_callsign(const char *value, const char *args_json) {
    return ads_decode_string(value, args_json);
}

ads_value_t *ads_decode_flight_number(const char *value, const char *args_json) {
    return ads_decode_string(value, args_json);
}

static char *trim_inplace(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t end = strlen(s);
    while (end > 0 && isspace((unsigned char)s[end - 1])) s[--end] = '\0';
    return s;
}

ads_value_t *ads_decode_trim(const char *value, const char *args_json) {
    (void)args_json;
    char *copy = strdup(value ? value : "");
    trim_inplace(copy);
    ads_value_t *v = ads_value_from_string(copy);
    free(copy);
    return v;
}

ads_value_t *ads_decode_uppercase(const char *value, const char *args_json) {
    (void)args_json;
    char *copy = strdup(value ? value : "");
    for (char *p = copy; *p; p++) *p = (char)toupper((unsigned char)*p);
    ads_value_t *v = ads_value_from_string(copy);
    free(copy);
    return v;
}

ads_value_t *ads_decode_lowercase(const char *value, const char *args_json) {
    (void)args_json;
    char *copy = strdup(value ? value : "");
    for (char *p = copy; *p; p++) *p = (char)tolower((unsigned char)*p);
    ads_value_t *v = ads_value_from_string(copy);
    free(copy);
    return v;
}

ads_value_t *ads_decode_airport(const char *value, const char *args_json) {
    (void)args_json;
    char *copy = strdup(value ? value : "");
    trim_inplace(copy);
    for (char *p = copy; *p; p++) *p = (char)toupper((unsigned char)*p);
    ads_value_t *v = ads_value_from_string(copy);
    free(copy);
    return v;
}

ads_value_t *ads_decode_tail_number(const char *value, const char *args_json) {
    char *copy = strdup(value ? value : "");
    trim_inplace(copy);
    cJSON *args = args_json ? cJSON_Parse(args_json) : NULL;
    const cJSON *strip = args ? cJSON_GetObjectItemCaseSensitive(args, "strip_chars") : NULL;
    if (strip && cJSON_IsString(strip)) {
        for (const char *p = strip->valuestring; *p; p++) {
            char *src = copy, *dst = copy;
            while (*src) {
                if (*src != *p) *dst++ = *src;
                src++;
            }
            *dst = '\0';
        }
    }
    cJSON_Delete(args);
    ads_value_t *v = ads_value_from_string(copy);
    free(copy);
    return v;
}

static int64_t hhmmss_to_tod(const char *s) {
    if (!s || strlen(s) < 6) return 0;
    char buf[3] = {0};
    memcpy(buf, s, 2); int h = atoi(buf);
    memcpy(buf, s + 2, 2); int m = atoi(buf);
    memcpy(buf, s + 4, 2); int sec = atoi(buf);
    return (int64_t)h * 3600 + (int64_t)m * 60 + sec;
}

ads_value_t *ads_decode_timestamp_hhmmss(const char *value, const char *args_json) {
    cJSON *args = args_json ? cJSON_Parse(args_json) : NULL;
    const cJSON *app = args ? cJSON_GetObjectItemCaseSensitive(args, "append") : NULL;
    char *combined;
    if (app && cJSON_IsString(app)) {
        size_t n = strlen(value ? value : "") + strlen(app->valuestring) + 1;
        combined = malloc(n);
        snprintf(combined, n, "%s%s", value ? value : "", app->valuestring);
    } else {
        combined = strdup(value ? value : "");
    }
    cJSON_Delete(args);
    int64_t tod = hhmmss_to_tod(combined);
    free(combined);
    return ads_value_from_int(tod);
}

/* ─── bitslice + concat_bits ─────────────────────────────────────────────── */

uint32_t ads_bitslice(uint32_t byte, uint8_t bit_start, uint8_t bit_end) {
    uint8_t width = bit_end - bit_start + 1;
    int shift = 8 - bit_end - 1;
    uint32_t mask = (1u << width) - 1u;
    return (byte >> shift) & mask;
}

uint32_t ads_concat_bits(const uint32_t *values, size_t count) {
    uint32_t acc = 0;
    for (size_t i = 0; i < count; i++) acc = (acc << 8) | values[i];
    return acc;
}
