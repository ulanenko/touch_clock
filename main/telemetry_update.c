#include "telemetry_update.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_version.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define TELEMETRY_TASK_STACK_BYTES 8192
#define TELEMETRY_PAYLOAD_MAX_BYTES 1024
#define TELEMETRY_DEVICE_ID_MAX_LEN 64
#define TELEMETRY_LABEL_MAX_LEN 64

typedef struct {
    char device_id[TELEMETRY_DEVICE_ID_MAX_LEN];
    char label[TELEMETRY_LABEL_MAX_LEN];
    char ip[sizeof(((app_runtime_state_t *)0)->wifi_ip)];
    char ota_status[sizeof(((app_runtime_state_t *)0)->ota_status)];
    char ota_partition[sizeof(((app_runtime_state_t *)0)->ota_running_partition)];
    int wifi_rssi;
    uint8_t timezone_id;
    bool time_synced;
    bool alarm_ringing;
    time_t next_alarm_epoch;
    uint32_t uptime_sec;
    uint32_t free_heap;
} telemetry_snapshot_t;

typedef struct {
    SemaphoreHandle_t lock;
    bool initialized;
    bool configured;
    bool busy;
    int64_t next_send_ms;
} telemetry_state_t;

static const char *TAG = "telemetry";
static telemetry_state_t s_telemetry;

static bool telemetry_url_configured(void)
{
    return CONFIG_TOUCH_CLOCK_TELEMETRY_URL[0] != '\0';
}

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0) {
        return;
    }
    snprintf(dst, dst_size, "%s", src != NULL ? src : "");
}

void telemetry_update_get_device_id(char *out, size_t out_size)
{
    uint8_t mac[6] = {0};
    esp_err_t err;

    if (CONFIG_TOUCH_CLOCK_TELEMETRY_DEVICE_ID[0] != '\0') {
        copy_text(out, out_size, CONFIG_TOUCH_CLOCK_TELEMETRY_DEVICE_ID);
        return;
    }

    err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        err = esp_read_mac(mac, ESP_MAC_BASE);
    }
    if (err != ESP_OK) {
        err = esp_efuse_mac_get_default(mac);
    }

    if (err == ESP_OK) {
        snprintf(out,
                 out_size,
                 "clock-%02x%02x%02x%02x%02x%02x",
                 mac[0],
                 mac[1],
                 mac[2],
                 mac[3],
                 mac[4],
                 mac[5]);
        return;
    }

    copy_text(out, out_size, "clock-unknown");
}

static size_t append_json_string(char *buffer, size_t buffer_size, const char *value)
{
    size_t written = 0;

    if (buffer_size == 0) {
        return 0;
    }

    for (const char *cursor = value != NULL ? value : ""; *cursor != '\0' && written + 1 < buffer_size; ++cursor) {
        unsigned char c = (unsigned char)*cursor;

        if ((c == '"' || c == '\\') && written + 2 < buffer_size) {
            buffer[written++] = '\\';
            buffer[written++] = (char)c;
        } else if (c >= 0x20 && c <= 0x7E) {
            buffer[written++] = (char)c;
        }
    }
    buffer[written] = '\0';
    return written;
}

static int build_payload(const telemetry_snapshot_t *snapshot, char *payload, size_t payload_size)
{
    char escaped_device_id[TELEMETRY_DEVICE_ID_MAX_LEN * 2];
    char escaped_label[TELEMETRY_LABEL_MAX_LEN * 2];
    char escaped_ip[sizeof(snapshot->ip) * 2];
    char escaped_ota_status[sizeof(snapshot->ota_status) * 2];
    char escaped_partition[sizeof(snapshot->ota_partition) * 2];

    append_json_string(escaped_device_id, sizeof(escaped_device_id), snapshot->device_id);
    append_json_string(escaped_label, sizeof(escaped_label), snapshot->label);
    append_json_string(escaped_ip, sizeof(escaped_ip), snapshot->ip);
    append_json_string(escaped_ota_status, sizeof(escaped_ota_status), snapshot->ota_status);
    append_json_string(escaped_partition, sizeof(escaped_partition), snapshot->ota_partition);

    return snprintf(payload,
                    payload_size,
                    "{"
                    "\"device_id\":\"%s\","
                    "\"label\":\"%s\","
                    "\"firmware\":\"%s\","
                    "\"ota_partition\":\"%s\","
                    "\"ip\":\"%s\","
                    "\"wifi_rssi\":%d,"
                    "\"time_synced\":%s,"
                    "\"uptime_sec\":%" PRIu32 ","
                    "\"free_heap\":%" PRIu32 ","
                    "\"alarm_ringing\":%s,"
                    "\"next_alarm_epoch\":%" PRId64 ","
                    "\"timezone_id\":%u,"
                    "\"ota_status\":\"%s\""
                    "}",
                    escaped_device_id,
                    escaped_label,
                    TOUCH_CLOCK_FIRMWARE_VERSION,
                    escaped_partition,
                    escaped_ip,
                    snapshot->wifi_rssi,
                    snapshot->time_synced ? "true" : "false",
                    snapshot->uptime_sec,
                    snapshot->free_heap,
                    snapshot->alarm_ringing ? "true" : "false",
                    (int64_t)snapshot->next_alarm_epoch,
                    (unsigned int)snapshot->timezone_id,
                    escaped_ota_status);
}

static esp_err_t post_heartbeat(const telemetry_snapshot_t *snapshot)
{
    char payload[TELEMETRY_PAYLOAD_MAX_BYTES];
    int payload_len = build_payload(snapshot, payload, sizeof(payload));
    esp_http_client_config_t config = {
        .url = CONFIG_TOUCH_CLOCK_TELEMETRY_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
        .buffer_size = 1024,
    };
    esp_http_client_handle_t client;
    esp_err_t err;
    int status;

    if (payload_len < 0 || payload_len >= (int)sizeof(payload)) {
        return ESP_ERR_INVALID_SIZE;
    }

    client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (CONFIG_TOUCH_CLOCK_TELEMETRY_TOKEN[0] != '\0') {
        esp_http_client_set_header(client, "X-Clock-Token", CONFIG_TOUCH_CLOCK_TELEMETRY_TOKEN);
    }
    esp_http_client_set_post_field(client, payload, payload_len);

    err = esp_http_client_perform(client);
    status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK) {
        return err;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "Heartbeat rejected with HTTP %d", status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void telemetry_task(void *arg)
{
    telemetry_snapshot_t snapshot = *(telemetry_snapshot_t *)arg;
    esp_err_t err;

    free(arg);
    err = post_heartbeat(&snapshot);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Heartbeat sent for %s", snapshot.device_id);
    } else {
        ESP_LOGW(TAG, "Heartbeat failed: %s", esp_err_to_name(err));
    }

    xSemaphoreTake(s_telemetry.lock, portMAX_DELAY);
    s_telemetry.busy = false;
    xSemaphoreGive(s_telemetry.lock);
    vTaskDelete(NULL);
}

esp_err_t telemetry_update_init(void)
{
    memset(&s_telemetry, 0, sizeof(s_telemetry));
    s_telemetry.lock = xSemaphoreCreateMutex();
    if (s_telemetry.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_telemetry.configured = telemetry_url_configured();
    s_telemetry.initialized = true;
    s_telemetry.next_send_ms = 0;
    ESP_LOGI(TAG, "Telemetry %s", s_telemetry.configured ? "configured" : "disabled");
    return ESP_OK;
}

void telemetry_update_tick(const app_runtime_state_t *runtime, const app_settings_t *settings, time_t now)
{
    int64_t now_ms;
    telemetry_snapshot_t *snapshot;

    if (!s_telemetry.initialized ||
        !s_telemetry.configured ||
        runtime == NULL ||
        settings == NULL ||
        !runtime->wifi_connected ||
        now <= 1700000000) {
        return;
    }

    now_ms = esp_timer_get_time() / 1000;
    xSemaphoreTake(s_telemetry.lock, portMAX_DELAY);
    if (s_telemetry.busy || now_ms < s_telemetry.next_send_ms) {
        xSemaphoreGive(s_telemetry.lock);
        return;
    }
    s_telemetry.busy = true;
    s_telemetry.next_send_ms = now_ms + ((int64_t)CONFIG_TOUCH_CLOCK_TELEMETRY_INTERVAL_SECONDS * 1000);
    xSemaphoreGive(s_telemetry.lock);

    snapshot = calloc(1, sizeof(*snapshot));
    if (snapshot == NULL) {
        xSemaphoreTake(s_telemetry.lock, portMAX_DELAY);
        s_telemetry.busy = false;
        xSemaphoreGive(s_telemetry.lock);
        return;
    }

    telemetry_update_get_device_id(snapshot->device_id, sizeof(snapshot->device_id));
    copy_text(snapshot->label, sizeof(snapshot->label), snapshot->device_id);
    copy_text(snapshot->ip, sizeof(snapshot->ip), runtime->wifi_ip);
    copy_text(snapshot->ota_status, sizeof(snapshot->ota_status), runtime->ota_status);
    copy_text(snapshot->ota_partition, sizeof(snapshot->ota_partition), runtime->ota_running_partition);
    snapshot->wifi_rssi = runtime->wifi_rssi;
    snapshot->timezone_id = settings->wifi.timezone_id;
    snapshot->time_synced = runtime->time_synced;
    snapshot->alarm_ringing = runtime->alarm_ringing;
    snapshot->next_alarm_epoch = runtime->next_alarm_epoch;
    snapshot->uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000);
    snapshot->free_heap = (uint32_t)esp_get_free_heap_size();

    if (xTaskCreate(telemetry_task,
                    "clock_telemetry",
                    TELEMETRY_TASK_STACK_BYTES,
                    snapshot,
                    tskIDLE_PRIORITY + 2,
                    NULL) != pdPASS) {
        free(snapshot);
        xSemaphoreTake(s_telemetry.lock, portMAX_DELAY);
        s_telemetry.busy = false;
        xSemaphoreGive(s_telemetry.lock);
    }
}
