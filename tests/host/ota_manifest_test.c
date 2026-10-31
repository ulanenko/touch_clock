#include "domain/ota_manifest.h"
#include "test_support.h"

#define SHA "d835ad6d317c22697922b29f065d60f115966c741e6285f6022fc24580511cb5"

static int order_of(const char *a, const char *b)
{
    int order = 99;

    if (!ota_manifest_version_compare(a, b, &order)) {
        return 99;
    }
    return order;
}

static int test_versions(void)
{
    EXPECT_TRUE(ota_manifest_version_is_newer("1.3", "1.2"));
    EXPECT_TRUE(ota_manifest_version_is_newer("1.10", "1.9"));
    EXPECT_TRUE(ota_manifest_version_is_newer("2", "1.99.99"));
    EXPECT_FALSE(ota_manifest_version_is_newer("1.2", "1.2"));
    EXPECT_FALSE(ota_manifest_version_is_newer("1.1", "1.2"));
    EXPECT_EQ_INT(0, order_of("1.3", "1.3.0"));
    EXPECT_EQ_INT(0, order_of("v1.3", "1.3.0.0"));

    /* A pre-release is older than its release, newer than the one before. */
    EXPECT_EQ_INT(-1, order_of("1.3-rc1", "1.3"));
    EXPECT_EQ_INT(1, order_of("1.3", "1.3-rc1"));
    EXPECT_TRUE(ota_manifest_version_is_newer("1.3-rc1", "1.2"));
    EXPECT_FALSE(ota_manifest_version_is_newer("1.3-rc1", "1.3"));
    EXPECT_FALSE(ota_manifest_version_is_newer("1.3.0-beta", "1.3"));

    /* Pre-releases among themselves, as semantic versioning orders them. */
    EXPECT_EQ_INT(-1, order_of("1.3-alpha", "1.3-alpha.1"));
    EXPECT_EQ_INT(-1, order_of("1.3-alpha.1", "1.3-alpha.beta"));
    EXPECT_EQ_INT(-1, order_of("1.3-alpha.beta", "1.3-beta"));
    EXPECT_EQ_INT(-1, order_of("1.3-beta.2", "1.3-beta.11"));
    EXPECT_EQ_INT(-1, order_of("1.3-rc.1", "1.3-rc.2"));
    EXPECT_EQ_INT(-1, order_of("1.3-rc1", "1.3-rc2"));
    EXPECT_EQ_INT(0, order_of("1.3-rc.1", "1.3-rc.1"));

    /* Build metadata is ignored. */
    EXPECT_EQ_INT(0, order_of("1.3.0+abc", "1.3"));
    EXPECT_EQ_INT(0, order_of("1.3+abc", "1.3+def"));
    EXPECT_EQ_INT(-1, order_of("1.3-rc1+abc", "1.3"));
    EXPECT_FALSE(ota_manifest_version_is_newer("1.3+build.7", "1.3"));

    /* Not versions: never newer. */
    EXPECT_FALSE(ota_manifest_version_is_valid(""));
    EXPECT_FALSE(ota_manifest_version_is_valid("latest"));
    EXPECT_FALSE(ota_manifest_version_is_valid("1..3"));
    EXPECT_FALSE(ota_manifest_version_is_valid("1.3."));
    EXPECT_FALSE(ota_manifest_version_is_valid("1.2.3.4.5"));
    EXPECT_FALSE(ota_manifest_version_is_valid("1.3-"));
    EXPECT_FALSE(ota_manifest_version_is_valid("1.3-rc..1"));
    EXPECT_FALSE(ota_manifest_version_is_valid("1.3+"));
    EXPECT_FALSE(ota_manifest_version_is_valid("1.3 "));
    EXPECT_FALSE(ota_manifest_version_is_valid("1.3_rc1"));
    EXPECT_FALSE(ota_manifest_version_is_valid("1234567890.0"));
    EXPECT_FALSE(ota_manifest_version_is_valid(NULL));
    EXPECT_FALSE(ota_manifest_version_is_newer("9.9 beta", "1.2"));
    EXPECT_FALSE(ota_manifest_version_is_newer("1.3", "garbage"));
    EXPECT_EQ_INT(99, order_of("x", "1.2"));
    return 0;
}

static int test_json_strings(void)
{
    char out[16];
    char tiny[4];

    EXPECT_TRUE(ota_manifest_json_get_string("{\"a\" : \"x\\\"y\\\\z\\/\"}", "a", out, sizeof(out)));
    EXPECT_STR_EQ("x\"y\\z/", out);
    EXPECT_TRUE(ota_manifest_json_get_string("{\n\t\"b\": 1, \"a\":\"\"}", "a", out, sizeof(out)));
    EXPECT_STR_EQ("", out);

    /* The key's name as another key's value is not the key. */
    EXPECT_TRUE(ota_manifest_json_get_string("{\"n\":\"a\",\"a\":\"good\"}", "a", out, sizeof(out)));
    EXPECT_STR_EQ("good", out);
    EXPECT_FALSE(ota_manifest_json_get_string("{\"n\":\"a\"}", "a", out, sizeof(out)));

    EXPECT_FALSE(ota_manifest_json_get_string("{\"a\":1}", "a", out, sizeof(out)));
    EXPECT_FALSE(ota_manifest_json_get_string("{\"b\":\"x\"}", "a", out, sizeof(out)));
    EXPECT_FALSE(ota_manifest_json_get_string("{\"a\":\"unterminated", "a", out, sizeof(out)));
    EXPECT_STR_EQ("", out);
    EXPECT_FALSE(ota_manifest_json_get_string("{\"a\":\"\\u0041\"}", "a", out, sizeof(out)));
    EXPECT_FALSE(ota_manifest_json_get_string("{\"a\":\"line\nbreak\"}", "a", out, sizeof(out)));

    /* A value too long for its field is refused, never cut short. */
    EXPECT_TRUE(ota_manifest_json_get_string("{\"a\":\"abc\"}", "a", tiny, sizeof(tiny)));
    EXPECT_STR_EQ("abc", tiny);
    EXPECT_FALSE(ota_manifest_json_get_string("{\"a\":\"abcd\"}", "a", tiny, sizeof(tiny)));
    EXPECT_STR_EQ("", tiny);
    return 0;
}

static int test_urls_and_hashes(void)
{
    EXPECT_TRUE(ota_manifest_url_is_https("https://example.com/fw.bin"));
    EXPECT_TRUE(ota_manifest_url_is_https("HTTPS://example.com/fw.bin"));
    EXPECT_FALSE(ota_manifest_url_is_https("http://example.com/fw.bin"));
    EXPECT_FALSE(ota_manifest_url_is_https("https://"));
    EXPECT_FALSE(ota_manifest_url_is_https("https:///fw.bin"));
    EXPECT_FALSE(ota_manifest_url_is_https("https://example.com/a b.bin"));
    EXPECT_FALSE(ota_manifest_url_is_https("ftp://example.com/fw.bin"));
    EXPECT_FALSE(ota_manifest_url_is_https(""));
    EXPECT_FALSE(ota_manifest_url_is_https(NULL));

    EXPECT_TRUE(ota_manifest_sha256_is_valid(SHA));
    EXPECT_TRUE(ota_manifest_sha256_is_valid("D835AD6D317C22697922B29F065D60F115966C741E6285F6022FC24580511CB5"));
    EXPECT_FALSE(ota_manifest_sha256_is_valid(""));
    EXPECT_FALSE(ota_manifest_sha256_is_valid(NULL));
    EXPECT_FALSE(ota_manifest_sha256_is_valid("d835ad6d"));
    EXPECT_FALSE(ota_manifest_sha256_is_valid("g835ad6d317c22697922b29f065d60f115966c741e6285f6022fc24580511cb5"));
    return 0;
}

static int test_manifests(void)
{
    ota_manifest_t manifest;
    char long_url[400];

    EXPECT_EQ_INT(OTA_MANIFEST_OK,
                  ota_manifest_parse("{\n  \"version\": \"1.3\",\n  \"url\": \"https://host/touch_clock-1.3.bin\",\n"
                                     "  \"sha256\": \"" SHA "\"\n}\n",
                                     &manifest));
    EXPECT_STR_EQ("1.3", manifest.version);
    EXPECT_STR_EQ("https://host/touch_clock-1.3.bin", manifest.firmware_url);
    EXPECT_STR_EQ(SHA, manifest.sha256);

    /* Each field is required. */
    EXPECT_EQ_INT(OTA_MANIFEST_MISSING_FIELD,
                  ota_manifest_parse("{\"version\":\"1.3\",\"url\":\"https://host/fw.bin\"}", &manifest));
    EXPECT_EQ_INT(OTA_MANIFEST_MISSING_FIELD,
                  ota_manifest_parse("{\"version\":\"1.3\",\"sha256\":\"" SHA "\"}", &manifest));
    EXPECT_EQ_INT(OTA_MANIFEST_MISSING_FIELD,
                  ota_manifest_parse("{\"url\":\"https://host/fw.bin\",\"sha256\":\"" SHA "\"}", &manifest));
    EXPECT_EQ_INT(OTA_MANIFEST_MISSING_FIELD, ota_manifest_parse("", &manifest));
    EXPECT_EQ_INT(OTA_MANIFEST_MISSING_FIELD, ota_manifest_parse(NULL, &manifest));

    /* A response cut off mid-field. */
    EXPECT_EQ_INT(OTA_MANIFEST_MISSING_FIELD,
                  ota_manifest_parse("{\"version\":\"1.3\",\"url\":\"https://host/fw.bin\",\"sha256\":\"d835ad6d31",
                                     &manifest));

    EXPECT_EQ_INT(OTA_MANIFEST_BAD_SHA256,
                  ota_manifest_parse("{\"version\":\"1.3\",\"url\":\"https://host/fw.bin\",\"sha256\":\"\"}", &manifest));
    EXPECT_EQ_INT(OTA_MANIFEST_BAD_SHA256,
                  ota_manifest_parse("{\"version\":\"1.3\",\"url\":\"https://host/fw.bin\",\"sha256\":\"abc\"}",
                                     &manifest));
    /* A hash with more after it was once cut to 64 hex digits and taken. */
    EXPECT_EQ_INT(OTA_MANIFEST_MISSING_FIELD,
                  ota_manifest_parse("{\"version\":\"1.3\",\"url\":\"https://host/fw.bin\",\"sha256\":\"" SHA "00\"}",
                                     &manifest));
    EXPECT_EQ_INT(OTA_MANIFEST_INSECURE_URL,
                  ota_manifest_parse("{\"version\":\"1.3\",\"url\":\"http://host/fw.bin\",\"sha256\":\"" SHA "\"}",
                                     &manifest));
    EXPECT_EQ_INT(OTA_MANIFEST_BAD_VERSION,
                  ota_manifest_parse("{\"version\":\"next\",\"url\":\"https://host/fw.bin\",\"sha256\":\"" SHA "\"}",
                                     &manifest));

    /* A URL longer than its field is refused, not downloaded cut short. */
    snprintf(long_url, sizeof(long_url), "{\"version\":\"1.3\",\"sha256\":\"" SHA "\",\"url\":\"https://host/");
    while (strlen(long_url) < sizeof(long_url) - 8) {
        strcat(long_url, "a");
    }
    strcat(long_url, ".bin\"}");
    EXPECT_EQ_INT(OTA_MANIFEST_MISSING_FIELD, ota_manifest_parse(long_url, &manifest));
    return 0;
}

int main(void)
{
    int status;

    status = test_versions();
    if (status != 0) {
        return status;
    }
    status = test_json_strings();
    if (status != 0) {
        return status;
    }
    status = test_urls_and_hashes();
    if (status != 0) {
        return status;
    }
    return test_manifests();
}
