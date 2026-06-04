/* Pure-C ASCII85 / Base85 decoder + base64 + hex + inflate (zlib). */

#include "ads_helpers.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef ADS_HAVE_ZLIB
#include <zlib.h>
#endif

ads_bytes_t ads_decode_ascii85(const char *s) {
    ads_bytes_t out = {0};
    if (!s) return out;
    const char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "<~", 2) == 0) p += 2;
    size_t cap = strlen(p) * 4 / 5 + 4;
    uint8_t *buf = malloc(cap);
    if (!buf) return out;
    size_t blen = 0;
    uint32_t accum = 0;
    int count = 0;

    while (*p) {
        char ch = *p++;
        if (ch == '~') break; /* trailing ~> */
        if (isspace((unsigned char)ch)) continue;
        if (ch == 'z' && count == 0) {
            buf[blen++] = 0; buf[blen++] = 0; buf[blen++] = 0; buf[blen++] = 0;
            continue;
        }
        if (ch < 33 || ch > 117) { free(buf); return out; }
        accum = accum * 85u + (uint32_t)(ch - 33);
        count++;
        if (count == 5) {
            buf[blen++] = (uint8_t)(accum >> 24);
            buf[blen++] = (uint8_t)(accum >> 16);
            buf[blen++] = (uint8_t)(accum >> 8);
            buf[blen++] = (uint8_t)accum;
            accum = 0; count = 0;
        }
    }
    if (count > 0) {
        for (int i = count; i < 5; i++) accum = accum * 85u + 84u;
        buf[blen++] = (uint8_t)(accum >> 24);
        if (count >= 3) buf[blen++] = (uint8_t)(accum >> 16);
        if (count >= 4) buf[blen++] = (uint8_t)(accum >> 8);
    }
    out.data = buf;
    out.len = blen;
    return out;
}

ads_bytes_t ads_base64_decode(const char *s) {
    static int table[256];
    static int initialized = 0;
    if (!initialized) {
        for (int i = 0; i < 256; i++) table[i] = -1;
        const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; i++) table[(int)alphabet[i]] = i;
        initialized = 1;
    }
    ads_bytes_t out = {0};
    if (!s) return out;
    size_t slen = strlen(s);
    out.data = malloc(slen);
    if (!out.data) return out;
    uint32_t buf = 0;
    int bits = 0;
    for (size_t i = 0; i < slen; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '=') break;
        if (isspace(c)) continue;
        int v = table[c];
        if (v < 0) continue;
        buf = (buf << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.data[out.len++] = (uint8_t)(buf >> bits) & 0xff;
        }
    }
    return out;
}

ads_bytes_t ads_hex_decode(const char *s) {
    ads_bytes_t out = {0};
    if (!s) return out;
    size_t slen = strlen(s);
    out.data = malloc(slen / 2 + 1);
    if (!out.data) return out;
    char tmp[3] = {0};
    int half = 0;
    for (size_t i = 0; i < slen; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!isxdigit(c)) continue;
        tmp[half++] = (char)c;
        if (half == 2) {
            out.data[out.len++] = (uint8_t)strtoul(tmp, NULL, 16);
            half = 0;
            tmp[0] = tmp[1] = 0;
        }
    }
    return out;
}

char *ads_text_decode(ads_bytes_t bytes, const char *encoding) {
    (void)encoding; /* ACARS messages are ASCII / UTF-8 safe */
    char *out = malloc(bytes.len + 1);
    if (!out) return NULL;
    memcpy(out, bytes.data, bytes.len);
    out[bytes.len] = '\0';
    return out;
}

ads_bytes_t ads_inflate(ads_bytes_t input, size_t offset, const char *format) {
    ads_bytes_t out = {0};
#ifdef ADS_HAVE_ZLIB
    if (input.len <= offset) return out;
    const uint8_t *src = input.data + offset;
    size_t src_len = input.len - offset;
    size_t cap = src_len * 8 + 64;
    out.data = malloc(cap);
    if (!out.data) return out;
    z_stream zs = {0};
    int wbits = (strcmp(format ? format : "raw", "gzip") == 0) ? 31
              : (strcmp(format ? format : "raw", "zlib") == 0) ? 15
              : -15;
    if (inflateInit2(&zs, wbits) != Z_OK) { free(out.data); out.data = NULL; return out; }
    zs.next_in = (Bytef *)src;
    zs.avail_in = (uInt)src_len;
    zs.next_out = out.data;
    zs.avail_out = (uInt)cap;
    int rc = inflate(&zs, Z_FINISH);
    out.len = zs.total_out;
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) { free(out.data); out.data = NULL; out.len = 0; }
#else
    (void)input; (void)offset; (void)format;
#endif
    return out;
}
