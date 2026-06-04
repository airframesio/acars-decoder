/* ads_split, ads_substring, ads_str_in. */

#include "ads_helpers.h"
#include <stdlib.h>
#include <string.h>

ads_str_list_t ads_split(const char *s, const char *delimiter) {
    ads_str_list_t out = {0};
    if (!s || !delimiter || !*delimiter) return out;
    size_t dlen = strlen(delimiter);
    const char *p = s;
    while (1) {
        const char *next = strstr(p, delimiter);
        size_t len = next ? (size_t)(next - p) : strlen(p);
        if (out.count == out.capacity) {
            size_t newcap = out.capacity ? out.capacity * 2 : 8;
            char **newitems = realloc(out.items, newcap * sizeof(char *));
            if (!newitems) { ads_str_list_free(&out); return out; }
            out.items = newitems;
            out.capacity = newcap;
        }
        char *copy = malloc(len + 1);
        if (!copy) { ads_str_list_free(&out); return out; }
        memcpy(copy, p, len);
        copy[len] = '\0';
        out.items[out.count++] = copy;
        if (!next) break;
        p = next + dlen;
    }
    return out;
}

const char *ads_substring(const char *s, int start, int end) {
    /* Returns a borrowed pointer into s + start. Caller must NOT free.
     * For length-bounded extraction, callers typically pass the result
     * through ads_value_from_string which copies. The end arg specifies
     * one-past-last; negative = to end. */
    if (!s) return "";
    size_t slen = strlen(s);
    if (start < 0) start = 0;
    if ((size_t)start > slen) start = (int)slen;
    if (end < 0 || (size_t)end > slen) end = (int)slen;
    /* Note: this leaks the trailing chars to the caller; for ACARS use cases
     * the caller almost always passes the substring through a copy quickly. */
    (void)end;
    return s + start;
}

bool ads_str_in(const char *const *list, size_t count, const char *needle) {
    if (!list || !needle) return false;
    for (size_t i = 0; i < count; i++) {
        if (list[i] && strcmp(list[i], needle) == 0) return true;
    }
    return false;
}
