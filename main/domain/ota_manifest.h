#pragma once

#include <stdbool.h>
#include <stddef.h>

/* The OTA manifest: a JSON object with the firmware's version, the https URL it is downloaded
 * from and its SHA-256, all three required:
 *
 *     {"version": "1.3", "url": "https://host/touch_clock-1.3.bin", "sha256": "<64 hex>"}
 *
 * Versions: an optional "v", one to four dot-separated numbers ("1.3" is "1.3.0"), then
 * optionally "-" and a pre-release, then optionally "+" and build metadata. A pre-release is
 * older than its release ("1.3-rc1" < "1.3"); two pre-releases of the same version compare as
 * semantic versioning does (dot-separated identifiers, numbers by value and below words, which
 * compare as ASCII; more identifiers is newer when the rest are equal: "rc.1" < "rc.2" <
 * "rc.10"). Build metadata is ignored ("1.3+abc" equals "1.3"). Anything else is not a
 * version, and is never newer. */

#define OTA_MANIFEST_VERSION_MAX_LEN 24
#define OTA_MANIFEST_URL_MAX_LEN 256
#define OTA_MANIFEST_SHA256_HEX_LEN 64

typedef struct {
    char version[OTA_MANIFEST_VERSION_MAX_LEN];
    char firmware_url[OTA_MANIFEST_URL_MAX_LEN];
    char sha256[OTA_MANIFEST_SHA256_HEX_LEN + 1];
} ota_manifest_t;

typedef enum {
    OTA_MANIFEST_OK = 0,
    OTA_MANIFEST_MISSING_FIELD, /* absent, not a string, or longer than its field */
    OTA_MANIFEST_BAD_VERSION,
    OTA_MANIFEST_INSECURE_URL,  /* not https:// */
    OTA_MANIFEST_BAD_SHA256,    /* not 64 hex digits */
} ota_manifest_result_t;

/* The string value of a key; false when absent, not a string, or too long for out (never a
 * value cut short). Escapes \" \\ \/ only: the manifest's fields need no others. */
bool ota_manifest_json_get_string(const char *json, const char *key, char *out, size_t out_size);
bool ota_manifest_sha256_is_valid(const char *hex);
bool ota_manifest_url_is_https(const char *url);
bool ota_manifest_version_is_valid(const char *version);
/* <0, 0 or >0 as a is older than, the same as or newer than b; false when either is not a
 * version (then *order is 0). */
bool ota_manifest_version_compare(const char *a, const char *b, int *order);
bool ota_manifest_version_is_newer(const char *candidate, const char *current);
/* A whole manifest, all fields checked; out is cleared first. */
ota_manifest_result_t ota_manifest_parse(const char *json, ota_manifest_t *out);
