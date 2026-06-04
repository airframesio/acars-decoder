/* POSIX-regex backed implementation of ads_regex_match / ads_regex_group.
 *
 * Note: POSIX regex does NOT support PCRE-style named capture groups
 * (?<name>...). For ACARS specs that use named groups, the Stage 2 C impl
 * needs to either:
 *   (a) compile with -lpcre2-8 and use PCRE2 instead of POSIX regex, or
 *   (b) preprocess patterns at codegen time to map ?<name>... → ?:... and
 *       maintain a name→index map alongside.
 *
 * This stub uses option (a) shape — assume PCRE2 in Stage 2. For v1, the
 * skeleton supports unnamed captures only and is sufficient for the
 * non-regex reference specs (Label_10_POS, Label_4A, OHMA, ARINC_702).
 */

#include "ads_helpers.h"
#include <regex.h>
#include <stdlib.h>
#include <string.h>

struct ads_regex_match {
    bool ok;
    char *input_copy;
    regex_t compiled;
    regmatch_t *matches;
    size_t match_count;
    /* For named-group support (Stage 2): name table */
};

ads_regex_match_t *ads_regex_match_new(const char *pattern, const char *input) {
    ads_regex_match_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    if (regcomp(&m->compiled, pattern, REG_EXTENDED) != 0) {
        free(m);
        return NULL;
    }
    m->input_copy = strdup(input ? input : "");
    m->match_count = m->compiled.re_nsub + 1;
    m->matches = calloc(m->match_count, sizeof(regmatch_t));
    if (!m->matches) { regfree(&m->compiled); free(m->input_copy); free(m); return NULL; }
    m->ok = regexec(&m->compiled, m->input_copy, m->match_count, m->matches, 0) == 0;
    return m;
}

bool ads_regex_match_ok(const ads_regex_match_t *m) { return m && m->ok; }

const char *ads_regex_group(const ads_regex_match_t *m, const char *name) {
    /* Stage-2 TODO: resolve named group via a name→index map populated at
     * compile time from the pattern. For now, support numeric names ("0".."9"). */
    if (!m || !m->ok || !name) return "";
    size_t idx = (size_t)atoi(name);
    if (idx >= m->match_count) return "";
    regmatch_t mt = m->matches[idx];
    if (mt.rm_so < 0) return "";
    /* Return a borrowed pointer into input_copy at the match start.
     * Length terminator is not enforced; callers should bound by mt.rm_eo. */
    return m->input_copy + mt.rm_so;
}

void ads_regex_match_free(ads_regex_match_t *m) {
    if (!m) return;
    regfree(&m->compiled);
    free(m->matches);
    free(m->input_copy);
    free(m);
}

bool ads_regex_test(const char *pattern, const char *input) {
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) != 0) return false;
    int rc = regexec(&re, input ? input : "", 0, NULL, 0);
    regfree(&re);
    return rc == 0;
}
