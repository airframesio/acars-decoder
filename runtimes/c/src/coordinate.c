/* Coordinate parsers: combined NS/EW + decimal-minutes.
 * Mirrors runtimes/typescript/utils/coordinate_utils.ts. */

#include "ads_helpers.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

struct ads_value { cJSON *node; bool owns; };

static ads_value_t *combined_to_value(double lat, double lon) {
    cJSON *node = cJSON_CreateObject();
    cJSON_AddNumberToObject(node, "latitude", lat);
    cJSON_AddNumberToObject(node, "longitude", lon);
    ads_value_t *v = calloc(1, sizeof(*v));
    v->node = node;
    v->owns = true;
    return v;
}

static bool parse_combined(const char *s, double *lat_out, double *lon_out, bool dec_min) {
    size_t slen = strlen(s);
    if (slen < 13) return false;
    char first = s[0];
    int middle_idx = (s[6] == ' ') ? 7 : 6;
    int lon_start = (s[6] == ' ') ? 8 : 7;
    if (slen < (size_t)(lon_start + 6)) return false;
    char middle = s[middle_idx];
    if ((first != 'N' && first != 'S') || (middle != 'E' && middle != 'W')) return false;
    char lat_buf[6] = {0}, lon_buf[7] = {0};
    memcpy(lat_buf, s + 1, 5);
    memcpy(lon_buf, s + lon_start, 6);
    char *endp;
    double lat_n = strtod(lat_buf, &endp);
    if (*endp != '\0') return false;
    double lon_n = strtod(lon_buf, &endp);
    if (*endp != '\0') return false;
    double lat_sign = (first == 'N') ? 1.0 : -1.0;
    double lon_sign = (middle == 'E') ? 1.0 : -1.0;
    if (dec_min) {
        double lat_deg = (int64_t)(lat_n / 1000.0);
        double lat_min = fmod(lat_n, 1000.0) / 10.0;
        double lon_deg = (int64_t)(lon_n / 1000.0);
        double lon_min = fmod(lon_n, 1000.0) / 10.0;
        *lat_out = (lat_deg + lat_min / 60.0) * lat_sign;
        *lon_out = (lon_deg + lon_min / 60.0) * lon_sign;
    } else {
        *lat_out = (lat_n / 1000.0) * lat_sign;
        *lon_out = (lon_n / 1000.0) * lon_sign;
    }
    return true;
}

ads_value_t *ads_decode_coordinate(const char *value, const char *args_json) {
    cJSON *args = args_json ? cJSON_Parse(args_json) : NULL;
    const cJSON *style = args ? cJSON_GetObjectItemCaseSensitive(args, "style") : NULL;
    if (style && cJSON_IsString(style) && strcmp(style->valuestring, "combined") == 0) {
        double lat, lon;
        bool ok = parse_combined(value, &lat, &lon, false);
        cJSON_Delete(args);
        return ok ? combined_to_value(lat, lon) : ads_value_null();
    }
    /* Single-axis: <prefix><digits> / divisor */
    double divisor = 1000.0;
    if (args) {
        cJSON *d = cJSON_GetObjectItemCaseSensitive(args, "divisor");
        if (cJSON_IsNumber(d)) divisor = d->valuedouble;
    }
    cJSON_Delete(args);
    if (!value || !*value) return ads_value_null();
    char prefix = value[0];
    double sign = (prefix == 'N' || prefix == 'E') ? 1.0 : -1.0;
    /* Skip whitespace then parse the digit run. */
    const char *p = value + 1;
    while (*p == ' ') p++;
    char *endp;
    double n = strtod(p, &endp);
    if (endp == p) return ads_value_null();
    return ads_value_from_double(sign * n / divisor);
}

ads_value_t *ads_decode_coordinate_decimal_minutes(const char *value, const char *args_json) {
    (void)args_json;
    double lat, lon;
    if (!parse_combined(value, &lat, &lon, true)) return ads_value_null();
    return combined_to_value(lat, lon);
}
