/* ads_runtime.h — public API for the ADS C runtime.
 *
 * Generated plugin code (output of `ads-gen --target c`) includes this header
 * plus ads_helpers.h and ads_escape_hatches.h:
 *
 *   #include "ads_runtime.h"
 *   #include "ads_helpers.h"
 *   #include "ads_escape_hatches.h"
 *
 *   ads_decode_result_t *label_10_pos_decode(const ads_message_t *msg,
 *                                            const ads_options_t *opts);
 *
 * Source of truth: acars-decoder-typescript. Behavior MUST match the TS
 * reference byte-for-byte; the shared corpus enforces this.
 */
#ifndef ADS_RUNTIME_H
#define ADS_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Core types ─────────────────────────────────────────────────────────── */

typedef struct {
    const char *label;     /* required */
    const char *sublabel;  /* optional, may be NULL */
    const char *text;      /* required */
} ads_message_t;

typedef struct {
    bool debug;
} ads_options_t;

typedef struct {
    const char *const *labels;     /* NULL-terminated array of label literals */
    const char *const *preambles;  /* NULL-terminated; may itself be NULL */
} ads_qualifiers_t;

typedef enum {
    ADS_DECODE_LEVEL_NONE = 0,
    ADS_DECODE_LEVEL_PARTIAL,
    ADS_DECODE_LEVEL_FULL,
} ads_decode_level_t;

/* Opaque value type. Backed by cJSON internally so plugins can store
 * arbitrary nested data. Use ads_value_from_* / ads_value_to_* helpers. */
typedef struct ads_value ads_value_t;

/* Opaque dynamic string list. Holds parts produced by ads_split / regex
 * matches. Items accessed via .items[N]; size via .count. */
typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} ads_str_list_t;

/* Opaque dynamic byte buffer (ads_bytes_t). */
typedef struct {
    uint8_t *data;
    size_t len;
} ads_bytes_t;

/* Regex match result. Backed by POSIX regex_t + match buffer. */
typedef struct ads_regex_match ads_regex_match_t;

/* DecodeResult — the structured output of a plugin's decode(). */
typedef struct ads_decode_result ads_decode_result_t;

/* Plugin descriptor used by the dispatcher's registry. */
typedef struct {
    const char *name;
    ads_qualifiers_t (*qualifiers)(void);
    ads_decode_result_t *(*decode)(const ads_message_t *msg, const ads_options_t *opts);
} ads_plugin_descriptor_t;

/* ─── DecodeResult API ───────────────────────────────────────────────────── */

ads_decode_result_t *ads_result_new(const char *plugin_name,
                                    const char *description,
                                    const ads_message_t *msg);
void ads_result_free(ads_decode_result_t *result);

void ads_result_set_decoded(ads_decode_result_t *result, bool decoded);
ads_decode_result_t *ads_result_fail_unknown(ads_decode_result_t *result, const char *text);

void ads_result_raw_set(ads_decode_result_t *result, const char *key, ads_value_t *value);

/* Accessors for callers (mainly tests, escape hatches, formatters). */
ads_value_t *ads_result_raw_get(const ads_decode_result_t *result, const char *key);
char *ads_result_to_json(const ads_decode_result_t *result); /* malloc'd; caller frees */

/* ─── Value API ──────────────────────────────────────────────────────────── */

ads_value_t *ads_value_from_string(const char *s);
ads_value_t *ads_value_from_double(double n);
ads_value_t *ads_value_from_int(int64_t n);
ads_value_t *ads_value_from_bool(bool b);
ads_value_t *ads_value_null(void);
void ads_value_free(ads_value_t *v);

bool   ads_value_as_double(const ads_value_t *v, double *out);
bool   ads_value_as_int(const ads_value_t *v, int64_t *out);
const char *ads_value_as_string(const ads_value_t *v);

/* ─── Buffer / string helpers ────────────────────────────────────────────── */

void ads_str_list_free(ads_str_list_t *list);
void ads_bytes_free(ads_bytes_t *b);

#ifdef __cplusplus
}
#endif

#endif /* ADS_RUNTIME_H */
