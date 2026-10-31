#include "domain/ota_manifest.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define VERSION_MAX_PARTS 4
#define VERSION_PART_MAX_DIGITS 9

typedef struct {
    unsigned long parts[VERSION_MAX_PARTS];
    const char *pre; /* the pre-release identifiers; NULL for a release */
    size_t pre_len;
} version_t;

static bool is_json_ws(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

/* Where the key's value starts, or NULL: a match counts only where a key can stand, after
 * '{' or ',' (not "url" inside some other string). */
static const char *find_value(const char *json, const char *key)
{
    char pattern[40];
    int pattern_len = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = json;

    if (pattern_len <= 0 || (size_t)pattern_len >= sizeof(pattern)) {
        return NULL;
    }
    while ((pos = strstr(pos, pattern)) != NULL) {
        const char *before = pos;
        const char *after = pos + pattern_len;

        while (before > json && is_json_ws(before[-1])) {
            --before;
        }
        while (is_json_ws(*after)) {
            ++after;
        }
        if (before > json && (before[-1] == '{' || before[-1] == ',') && *after == ':') {
            ++after;
            while (is_json_ws(*after)) {
                ++after;
            }
            return after;
        }
        pos += pattern_len;
    }
    return NULL;
}

bool ota_manifest_json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
    const char *value;
    size_t len = 0;

    if (json == NULL || key == NULL || out == NULL || out_size == 0) {
        return false;
    }
    out[0] = '\0';

    value = find_value(json, key);
    if (value == NULL || *value != '"') {
        return false;
    }
    for (const char *p = value + 1; *p != '"'; ++p) {
        char c = *p;

        if ((unsigned char)c < 0x20) { /* the end of the text, or a raw control character */
            out[0] = '\0';
            return false;
        }
        if (c == '\\') {
            ++p;
            if (*p != '"' && *p != '\\' && *p != '/') {
                out[0] = '\0';
                return false;
            }
            c = *p;
        }
        if (len + 1 >= out_size) {
            out[0] = '\0';
            return false;
        }
        out[len++] = c;
    }
    out[len] = '\0';
    return true;
}

bool ota_manifest_sha256_is_valid(const char *hex)
{
    if (hex == NULL || strlen(hex) != OTA_MANIFEST_SHA256_HEX_LEN) {
        return false;
    }
    for (size_t i = 0; i < OTA_MANIFEST_SHA256_HEX_LEN; ++i) {
        if (!isxdigit((unsigned char)hex[i])) {
            return false;
        }
    }
    return true;
}

bool ota_manifest_url_is_https(const char *url)
{
    static const char s_scheme[] = "https://";
    size_t scheme_len = sizeof(s_scheme) - 1;

    if (url == NULL) {
        return false;
    }
    for (size_t i = 0; i < scheme_len; ++i) {
        if (tolower((unsigned char)url[i]) != s_scheme[i]) {
            return false;
        }
    }
    if (url[scheme_len] == '\0' || url[scheme_len] == '/') {
        return false; /* no host */
    }
    for (const char *p = url; *p != '\0'; ++p) {
        if ((unsigned char)*p <= 0x20 || (unsigned char)*p >= 0x7F) {
            return false;
        }
    }
    return true;
}

/* Dot-separated identifiers of [0-9A-Za-z-], none empty. */
static bool scan_identifiers(const char **cursor)
{
    const char *s = *cursor;

    for (;;) {
        const char *start = s;

        while (isalnum((unsigned char)*s) || *s == '-') {
            ++s;
        }
        if (s == start) {
            return false;
        }
        if (*s != '.') {
            break;
        }
        ++s;
    }
    *cursor = s;
    return true;
}

static bool parse_version(const char *s, version_t *v)
{
    int count = 0;

    memset(v, 0, sizeof(*v));
    if (s == NULL) {
        return false;
    }
    if (*s == 'v' || *s == 'V') {
        ++s;
    }
    for (;;) {
        unsigned long value = 0;
        int digits = 0;

        if (count == VERSION_MAX_PARTS) {
            return false;
        }
        while (isdigit((unsigned char)*s)) {
            if (++digits > VERSION_PART_MAX_DIGITS) {
                return false;
            }
            value = value * 10UL + (unsigned long)(*s - '0');
            ++s;
        }
        if (digits == 0) {
            return false;
        }
        v->parts[count++] = value;
        if (*s != '.') {
            break;
        }
        ++s;
    }
    if (*s == '-') {
        ++s;
        v->pre = s;
        if (!scan_identifiers(&s)) {
            return false;
        }
        v->pre_len = (size_t)(s - v->pre);
    }
    if (*s == '+') {
        ++s;
        if (!scan_identifiers(&s)) {
            return false;
        }
    }
    return *s == '\0';
}

static bool all_digits(const char *s, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if (!isdigit((unsigned char)s[i])) {
            return false;
        }
    }
    return true;
}

static int sign_of(int value)
{
    return (value > 0) - (value < 0);
}

static int compare_identifier(const char *a, size_t a_len, const char *b, size_t b_len)
{
    bool a_number = all_digits(a, a_len);
    bool b_number = all_digits(b, b_len);
    int order;

    if (a_number != b_number) {
        return a_number ? -1 : 1;
    }
    if (a_number) {
        while (a_len > 1 && *a == '0') {
            ++a;
            --a_len;
        }
        while (b_len > 1 && *b == '0') {
            ++b;
            --b_len;
        }
        if (a_len != b_len) {
            return a_len < b_len ? -1 : 1;
        }
        return sign_of(memcmp(a, b, a_len));
    }
    order = memcmp(a, b, a_len < b_len ? a_len : b_len);
    if (order != 0) {
        return sign_of(order);
    }
    return (a_len > b_len) - (a_len < b_len);
}

static int compare_prerelease(const version_t *a, const version_t *b)
{
    const char *pa = a->pre;
    const char *pb = b->pre;
    const char *a_end = a->pre + a->pre_len;
    const char *b_end = b->pre + b->pre_len;

    if (pa == NULL || pb == NULL) {
        return (pa == NULL) - (pb == NULL); /* a release is newer than its pre-releases */
    }
    for (;;) {
        const char *a_dot;
        const char *b_dot;
        int order;

        if (pa >= a_end || pb >= b_end) {
            return (pa < a_end) - (pb < b_end);
        }
        a_dot = memchr(pa, '.', (size_t)(a_end - pa));
        b_dot = memchr(pb, '.', (size_t)(b_end - pb));
        if (a_dot == NULL) {
            a_dot = a_end;
        }
        if (b_dot == NULL) {
            b_dot = b_end;
        }
        order = compare_identifier(pa, (size_t)(a_dot - pa), pb, (size_t)(b_dot - pb));
        if (order != 0) {
            return order;
        }
        pa = a_dot + 1;
        pb = b_dot + 1;
    }
}

bool ota_manifest_version_is_valid(const char *version)
{
    version_t parsed;

    return parse_version(version, &parsed);
}

bool ota_manifest_version_compare(const char *a, const char *b, int *order)
{
    version_t va;
    version_t vb;

    *order = 0;
    if (!parse_version(a, &va) || !parse_version(b, &vb)) {
        return false;
    }
    for (int i = 0; i < VERSION_MAX_PARTS; ++i) {
        if (va.parts[i] != vb.parts[i]) {
            *order = va.parts[i] < vb.parts[i] ? -1 : 1;
            return true;
        }
    }
    *order = compare_prerelease(&va, &vb);
    return true;
}

bool ota_manifest_version_is_newer(const char *candidate, const char *current)
{
    int order;

    return ota_manifest_version_compare(candidate, current, &order) && order > 0;
}

ota_manifest_result_t ota_manifest_parse(const char *json, ota_manifest_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!ota_manifest_json_get_string(json, "version", out->version, sizeof(out->version)) ||
        !ota_manifest_json_get_string(json, "url", out->firmware_url, sizeof(out->firmware_url)) ||
        !ota_manifest_json_get_string(json, "sha256", out->sha256, sizeof(out->sha256))) {
        return OTA_MANIFEST_MISSING_FIELD;
    }
    if (!ota_manifest_version_is_valid(out->version)) {
        return OTA_MANIFEST_BAD_VERSION;
    }
    if (!ota_manifest_url_is_https(out->firmware_url)) {
        return OTA_MANIFEST_INSECURE_URL;
    }
    if (!ota_manifest_sha256_is_valid(out->sha256)) {
        return OTA_MANIFEST_BAD_SHA256;
    }
    return OTA_MANIFEST_OK;
}
