#include "ota_update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_version.h"
#include "domain/ota_manifest.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "sdkconfig.h"

#define OTA_MANIFEST_MAX_BYTES 3072
#define OTA_DOWNLOAD_CHUNK_BYTES 4096
#define OTA_TASK_STACK_BYTES 12288

typedef enum {
    OTA_REQUEST_CHECK = 0,
    OTA_REQUEST_INSTALL = 1,
} ota_request_t;

typedef struct {
    SemaphoreHandle_t lock;
    bool initialized;
    bool configured;
    bool busy;
    bool update_available;
    bool reboot_pending;
    uint8_t progress;
    char status[96];
    char available_version[OTA_MANIFEST_VERSION_MAX_LEN];
    char running_partition[16];
} ota_update_state_t;

static const char *TAG = "ota_update";
static ota_update_state_t s_ota;

static void set_status_locked(const char *status)
{
    snprintf(s_ota.status, sizeof(s_ota.status), "%s", status != NULL ? status : "");
}

static void set_status(const char *status)
{
    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    set_status_locked(status);
    xSemaphoreGive(s_ota.lock);
}

static void set_busy(bool busy, uint8_t progress, const char *status)
{
    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    s_ota.busy = busy;
    s_ota.progress = progress;
    if (status != NULL) {
        set_status_locked(status);
    }
    xSemaphoreGive(s_ota.lock);
}

static void set_progress(uint8_t progress, const char *status)
{
    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    s_ota.progress = progress;
    if (status != NULL) {
        set_status_locked(status);
    }
    xSemaphoreGive(s_ota.lock);
}

/* Only an https manifest: it names the firmware to flash and its hash, which over plain http
 * anyone on the way could replace. */
static bool ota_manifest_url_configured(void)
{
    return ota_manifest_url_is_https(CONFIG_TOUCH_CLOCK_OTA_MANIFEST_URL);
}

static void bytes_to_hex(const uint8_t *bytes, size_t len, char *out, size_t out_size)
{
    static const char s_hex[] = "0123456789abcdef";
    size_t pos = 0;

    if (out_size == 0) {
        return;
    }

    for (size_t i = 0; i < len && pos + 2 < out_size; ++i) {
        out[pos++] = s_hex[(bytes[i] >> 4) & 0x0F];
        out[pos++] = s_hex[bytes[i] & 0x0F];
    }
    out[pos] = '\0';
}

static esp_err_t fetch_text_url(const char *url, char *buffer, size_t buffer_size, size_t *out_len)
{
    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
        .buffer_size = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    int status;
    int total = 0;
    esp_err_t err;

    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    (void)esp_http_client_fetch_headers(client);
    status = esp_http_client_get_status_code(client);
    if (status < 200 || status >= 300) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    while (total + 1 < (int)buffer_size) {
        int read_len = esp_http_client_read(client, buffer + total, (int)buffer_size - total - 1);

        if (read_len < 0) {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        if (read_len == 0) {
            break;
        }
        total += read_len;
    }
    /* A body that does not fit is an error: its first part could still read as a manifest. */
    if (total + 1 >= (int)buffer_size) {
        char more;

        if (esp_http_client_read(client, &more, 1) != 0) {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_ERR_INVALID_SIZE;
        }
    }

    buffer[total] = '\0';
    if (out_len != NULL) {
        *out_len = (size_t)total;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_OK;
}

static esp_err_t fetch_manifest(ota_manifest_t *manifest)
{
    char *json;
    size_t json_len = 0;
    ota_manifest_result_t parsed;
    esp_err_t err;

    if (!ota_manifest_url_configured()) {
        return ESP_ERR_INVALID_STATE;
    }

    json = heap_caps_calloc(1, OTA_MANIFEST_MAX_BYTES, MALLOC_CAP_SPIRAM);
    if (json == NULL) {
        return ESP_ERR_NO_MEM;
    }

    set_status("Checking manifest...");
    err = fetch_text_url(CONFIG_TOUCH_CLOCK_OTA_MANIFEST_URL, json, OTA_MANIFEST_MAX_BYTES, &json_len);
    if (err != ESP_OK) {
        free(json);
        return err;
    }
    if (json_len == 0) {
        free(json);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Version, https URL and SHA-256 all required: a firmware is never flashed unchecked. */
    parsed = ota_manifest_parse(json, manifest);
    free(json);
    switch (parsed) {
    case OTA_MANIFEST_OK:
        return ESP_OK;
    case OTA_MANIFEST_BAD_SHA256:
        ESP_LOGW(TAG, "Manifest without a valid sha256");
        return ESP_ERR_INVALID_CRC;
    case OTA_MANIFEST_INSECURE_URL:
        ESP_LOGW(TAG, "Manifest firmware URL is not https");
        return ESP_ERR_INVALID_ARG;
    case OTA_MANIFEST_BAD_VERSION:
        ESP_LOGW(TAG, "Manifest version is not a version");
        return ESP_ERR_INVALID_RESPONSE; /* ESP_ERR_INVALID_VERSION means "not newer" here */
    case OTA_MANIFEST_MISSING_FIELD:
    default:
        ESP_LOGW(TAG, "Manifest lacks version, url or sha256 (or one is too long)");
        return ESP_ERR_INVALID_RESPONSE;
    }
}

static void cache_manifest_result(const ota_manifest_t *manifest)
{
    bool available = ota_manifest_version_is_newer(manifest->version, TOUCH_CLOCK_FIRMWARE_VERSION);

#if CONFIG_TOUCH_CLOCK_OTA_ALLOW_DOWNGRADE
    available = true;
#endif

    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    s_ota.update_available = available;
    snprintf(s_ota.available_version, sizeof(s_ota.available_version), "%s", manifest->version);
    set_status_locked(available ? "Update available" : "Already up to date");
    xSemaphoreGive(s_ota.lock);
}

static esp_err_t run_check(void)
{
    ota_manifest_t manifest;
    esp_err_t err = fetch_manifest(&manifest);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Manifest check failed: %s", esp_err_to_name(err));
        set_status("Update check failed");
        return err;
    }

    cache_manifest_result(&manifest);
    return ESP_OK;
}

static esp_err_t verify_manifest_for_install(ota_manifest_t *manifest)
{
    esp_err_t err;
    bool available;

    err = fetch_manifest(manifest);
    if (err != ESP_OK) {
        set_status("Manifest failed");
        return err;
    }

    available = ota_manifest_version_is_newer(manifest->version, TOUCH_CLOCK_FIRMWARE_VERSION);
#if CONFIG_TOUCH_CLOCK_OTA_ALLOW_DOWNGRADE
    available = true;
#endif
    if (!available) {
        cache_manifest_result(manifest);
        return ESP_ERR_INVALID_VERSION;
    }

    cache_manifest_result(manifest);
    return ESP_OK;
}

static esp_err_t run_install(void)
{
    ota_manifest_t manifest;
    const esp_partition_t *partition;
    esp_ota_handle_t ota_handle = 0;
    esp_http_client_handle_t client = NULL;
    mbedtls_sha256_context sha_ctx;
    uint8_t digest[32];
    char digest_hex[OTA_MANIFEST_SHA256_HEX_LEN + 1];
    uint8_t *buffer = NULL;
    int content_length;
    int written = 0;
    esp_err_t err;
    bool sha_initialized = false;

    err = verify_manifest_for_install(&manifest);
    if (err != ESP_OK) {
        if (err == ESP_ERR_INVALID_VERSION) {
            set_status("No newer update");
        }
        return err;
    }

    partition = esp_ota_get_next_update_partition(NULL);
    if (partition == NULL) {
        set_status("OTA slots missing");
        return ESP_ERR_NOT_FOUND;
    }

    esp_http_client_config_t config = {
        .url = manifest.firmware_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
        .buffer_size = OTA_DOWNLOAD_CHUNK_BYTES,
    };

    buffer = malloc(OTA_DOWNLOAD_CHUNK_BYTES);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    set_progress(0, "Downloading...");
    client = esp_http_client_init(&config);
    if (client == NULL) {
        err = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        goto cleanup;
    }

    content_length = (int)esp_http_client_fetch_headers(client);
    if (esp_http_client_get_status_code(client) < 200 || esp_http_client_get_status_code(client) >= 300) {
        err = ESP_FAIL;
        goto cleanup;
    }
    if (content_length <= 0 || (uint32_t)content_length > partition->size) {
        err = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }

    err = esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        goto cleanup;
    }

    mbedtls_sha256_init(&sha_ctx);
    sha_initialized = true;
    if (mbedtls_sha256_starts(&sha_ctx, 0) != 0) {
        err = ESP_FAIL;
        goto cleanup;
    }
    while (written < content_length) {
        int read_len = esp_http_client_read(client, (char *)buffer, OTA_DOWNLOAD_CHUNK_BYTES);

        if (read_len < 0) {
            err = ESP_FAIL;
            goto cleanup;
        }
        if (read_len == 0) {
            break;
        }

        if (mbedtls_sha256_update(&sha_ctx, buffer, (size_t)read_len) != 0) {
            err = ESP_FAIL;
            goto cleanup;
        }
        err = esp_ota_write(ota_handle, buffer, (size_t)read_len);
        if (err != ESP_OK) {
            goto cleanup;
        }

        written += read_len;
        set_progress((uint8_t)((written * 100) / content_length), "Downloading...");
    }

    if (written != content_length) {
        err = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }

    if (mbedtls_sha256_finish(&sha_ctx, digest) != 0) {
        err = ESP_FAIL;
        goto cleanup;
    }
    bytes_to_hex(digest, sizeof(digest), digest_hex, sizeof(digest_hex));
    if (strcasecmp(digest_hex, manifest.sha256) != 0) { /* the hash is never absent: parse requires it */
        ESP_LOGE(TAG, "SHA mismatch expected=%s actual=%s", manifest.sha256, digest_hex);
        err = ESP_ERR_INVALID_CRC;
        goto cleanup;
    }

    set_progress(100, "Validating...");
    err = esp_ota_end(ota_handle);
    ota_handle = 0;
    if (err != ESP_OK) {
        goto cleanup;
    }

    err = esp_ota_set_boot_partition(partition);
    if (err != ESP_OK) {
        goto cleanup;
    }

    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    s_ota.busy = false;
    s_ota.progress = 100;
    s_ota.reboot_pending = true;
    s_ota.update_available = false;
    set_status_locked("Update installed; rebooting");
    xSemaphoreGive(s_ota.lock);

    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    err = ESP_OK;

cleanup:
    if (sha_initialized) {
        mbedtls_sha256_free(&sha_ctx);
    }
    if (ota_handle != 0) {
        (void)esp_ota_abort(ota_handle);
    }
    if (client != NULL) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    free(buffer);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Install failed: %s", esp_err_to_name(err));
        set_status("Update failed");
    }
    return err;
}

static void ota_task(void *arg)
{
    ota_request_t request = (ota_request_t)(uintptr_t)arg;

    if (request == OTA_REQUEST_INSTALL) {
        (void)run_install();
    } else {
        (void)run_check();
    }

    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    if (!s_ota.reboot_pending) {
        s_ota.busy = false;
        s_ota.progress = 0;
    }
    xSemaphoreGive(s_ota.lock);
    if (request == OTA_REQUEST_INSTALL) {
        vTaskDelete(NULL);
    } else {
        vTaskDeleteWithCaps(NULL);
    }
}

static esp_err_t start_ota_task(ota_request_t request)
{
    bool can_start;
    BaseType_t created;

    if (!s_ota.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    can_start = !s_ota.busy && !s_ota.reboot_pending;
    if (can_start) {
        s_ota.busy = true;
        s_ota.progress = 0;
        set_status_locked(request == OTA_REQUEST_INSTALL ? "Starting update..." : "Checking...");
    }
    xSemaphoreGive(s_ota.lock);

    if (!can_start) {
        return ESP_ERR_INVALID_STATE;
    }

    /* A check never writes flash, so its stack can live in PSRAM: the first check after boot
     * otherwise took 12 KB of internal RAM just as the TLS handshake needs it. An install
     * writes the OTA slot, which needs a stack in internal RAM while the cache is off. */
    if (request == OTA_REQUEST_INSTALL) {
        created = xTaskCreate(ota_task, "ota_install", OTA_TASK_STACK_BYTES, (void *)(uintptr_t)request, 4, NULL);
    } else {
        created = xTaskCreateWithCaps(ota_task,
                                      "ota_check",
                                      OTA_TASK_STACK_BYTES,
                                      (void *)(uintptr_t)request,
                                      4,
                                      NULL,
                                      MALLOC_CAP_SPIRAM);
    }
    if (created != pdPASS) {
        set_busy(false, 0, "Update task failed");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/* A new image is kept once it has started its network; one that cannot could never be updated
 * again. It goes back to the image it replaced instead: a single reboot, as the bootloader
 * marks this one aborted. An image already confirmed stays and runs without the network. */
static void confirm_or_roll_back(const esp_partition_t *running, bool network_available)
{
#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    esp_err_t err;

    if (running == NULL || esp_ota_get_state_partition(running, &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        return;
    }
    if (!network_available) {
        ESP_LOGE(TAG, "New firmware started without Wi-Fi: rolling back");
        err = esp_ota_mark_app_invalid_rollback_and_reboot();
        /* Returns only when there is no image to go back to: this one has to do. */
        ESP_LOGE(TAG, "Rollback impossible: %s", esp_err_to_name(err));
    }
    err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Confirming the new firmware failed: %s", esp_err_to_name(err));
    }
#else
    (void)running;
    (void)network_available;
#endif
}

esp_err_t ota_update_init(bool network_available)
{
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (s_ota.initialized) {
        return ESP_OK;
    }

    /* Before anything that can fail here: the image's fate does not hang on this service. */
    confirm_or_roll_back(running, network_available);

    memset(&s_ota, 0, sizeof(s_ota));
    s_ota.lock = xSemaphoreCreateMutex();
    if (s_ota.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_ota.configured = ota_manifest_url_configured();
    set_status_locked(s_ota.configured ? "Ready to check for updates" : "OTA URL not configured");
    if (!s_ota.configured && CONFIG_TOUCH_CLOCK_OTA_MANIFEST_URL[0] != '\0') {
        ESP_LOGW(TAG, "OTA manifest URL is not https: updates are off");
    }

    if (running != NULL) {
        snprintf(s_ota.running_partition, sizeof(s_ota.running_partition), "%s", running->label);
    }

    s_ota.initialized = true;
    return ESP_OK;
}

void ota_update_snapshot(app_runtime_state_t *runtime)
{
    if (runtime == NULL || !s_ota.initialized) {
        return;
    }

    xSemaphoreTake(s_ota.lock, portMAX_DELAY);
    runtime->ota_configured = s_ota.configured;
    runtime->ota_busy = s_ota.busy;
    runtime->ota_update_available = s_ota.update_available;
    runtime->ota_reboot_pending = s_ota.reboot_pending;
    runtime->ota_progress = s_ota.progress;
    snprintf(runtime->ota_status, sizeof(runtime->ota_status), "%s", s_ota.status);
    snprintf(runtime->ota_available_version,
             sizeof(runtime->ota_available_version),
             "%s",
             s_ota.available_version);
    snprintf(runtime->ota_running_partition,
             sizeof(runtime->ota_running_partition),
             "%s",
             s_ota.running_partition);
    xSemaphoreGive(s_ota.lock);
}

esp_err_t ota_update_request_check(void)
{
    if (!s_ota.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!ota_manifest_url_configured()) {
        set_status("OTA URL not configured");
        return ESP_ERR_INVALID_STATE;
    }
    return start_ota_task(OTA_REQUEST_CHECK);
}

esp_err_t ota_update_request_install(void)
{
    if (!s_ota.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!ota_manifest_url_configured()) {
        set_status("OTA URL not configured");
        return ESP_ERR_INVALID_STATE;
    }
    return start_ota_task(OTA_REQUEST_INSTALL);
}
