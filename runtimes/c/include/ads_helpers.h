/* ads_helpers.h — decode-fn helpers the codegen emits calls into.
 *
 * Each function corresponds to a `decode.fn` value in spec YAML. Mirrors
 * runtimes/typescript/helpers.ts and runtimes/rust/src/helpers.rs.
 *
 * args_json is a JSON-encoded args object; helpers parse on demand. v1.1
 * should move to typed args.
 */
#ifndef ADS_HELPERS_H
#define ADS_HELPERS_H

#include "ads_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Strings + lists + regex ────────────────────────────────────────────── */

ads_str_list_t ads_split(const char *s, const char *delimiter);
const char *ads_substring(const char *s, int start, int end);  /* end < 0 = to end */
bool ads_str_in(const char *const *list, size_t count, const char *needle);

ads_regex_match_t *ads_regex_match_new(const char *pattern, const char *input);
bool ads_regex_match_ok(const ads_regex_match_t *m);
const char *ads_regex_group(const ads_regex_match_t *m, const char *name);
void ads_regex_match_free(ads_regex_match_t *m);
/* Codegen-side convenience returning by value. Wraps _new + ok(). */
#define ads_regex_match(pattern, input) (*ads_regex_match_new(pattern, input))
bool ads_regex_test(const char *pattern, const char *input);

/* ─── Decode-fn helpers ──────────────────────────────────────────────────── */
/* Uniform signature: every decode-fn helper accepts (value, args_json),
 * where args_json is "{}" when the spec specifies no args. Simplifies the
 * emitter and matches the Rust runtime's convergence pass. */

ads_value_t *ads_decode_coordinate(const char *value, const char *args_json);
ads_value_t *ads_decode_coordinate_decimal_minutes(const char *value, const char *args_json);
ads_value_t *ads_decode_integer(const char *value, const char *args_json);
ads_value_t *ads_decode_float(const char *value, const char *args_json);
ads_value_t *ads_decode_string(const char *value, const char *args_json);
ads_value_t *ads_decode_trim(const char *value, const char *args_json);
ads_value_t *ads_decode_uppercase(const char *value, const char *args_json);
ads_value_t *ads_decode_lowercase(const char *value, const char *args_json);
ads_value_t *ads_decode_timestamp_hhmmss(const char *value, const char *args_json);
ads_value_t *ads_decode_callsign(const char *value, const char *args_json);
ads_value_t *ads_decode_tail_number(const char *value, const char *args_json);
ads_value_t *ads_decode_flight_number(const char *value, const char *args_json);
ads_value_t *ads_decode_airport(const char *value, const char *args_json);

/* ─── Binary / encoding ──────────────────────────────────────────────────── */

ads_bytes_t  ads_base64_decode(const char *s);
ads_bytes_t  ads_inflate(ads_bytes_t input, size_t offset, const char *format);
ads_bytes_t  ads_decode_ascii85(const char *s);
ads_bytes_t  ads_hex_decode(const char *s);
char        *ads_text_decode(ads_bytes_t bytes, const char *encoding); /* malloc'd */

uint32_t ads_bitslice(uint32_t byte, uint8_t bit_start, uint8_t bit_end);
uint32_t ads_concat_bits(const uint32_t *values, size_t count);

/* ─── Formatter helpers (push items onto result.formatted.items) ──────── */

void ads_fmt_position(ads_decode_result_t *r, ads_value_t *lat, ads_value_t *lon);
void ads_fmt_position_value(ads_decode_result_t *r, ads_value_t *position);
void ads_fmt_altitude(ads_decode_result_t *r, ads_value_t *v);
void ads_fmt_speed(ads_decode_result_t *r, ads_value_t *v);
void ads_fmt_heading(ads_decode_result_t *r, ads_value_t *v);
void ads_fmt_timestamp(ads_decode_result_t *r, ads_value_t *v);
void ads_fmt_callsign(ads_decode_result_t *r, ads_value_t *v);
void ads_fmt_flight_number(ads_decode_result_t *r, ads_value_t *v);
void ads_fmt_tail(ads_decode_result_t *r, ads_value_t *v);
void ads_fmt_departure_airport(ads_decode_result_t *r, ads_value_t *v);
void ads_fmt_arrival_airport(ads_decode_result_t *r, ads_value_t *v);
void ads_fmt_fuel(ads_decode_result_t *r, ads_value_t *v);
void ads_fmt_unknown_arr(ads_decode_result_t *r, const char *const *values, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* ADS_HELPERS_H */
