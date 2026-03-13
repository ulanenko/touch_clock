#include "wifi_time.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"

typedef struct {
    SemaphoreHandle_t lock;
    bool initialized;
    bool has_credentials;
    bool connected;
    bool connecting;
    bool scanning;
    bool time_synced;
    int rssi;
    char ssid[33];
    char password[65];
    char ip[16];
    char status[96];
    uint32_t scan_generation;
    size_t scan_count;
    wifi_scan_result_t scan_results[WIFI_TIME_MAX_SCAN_RESULTS];
} wifi_time_state_t;

static const char *TAG = "wifi_time";
static wifi_time_state_t s_wifi = {0};
static esp_event_handler_instance_t s_wifi_any_id;
static esp_event_handler_instance_t s_ip_got_ip;
static esp_netif_t *s_sta_netif;

static void set_status_locked(const char *status)
{
    snprintf(s_wifi.status, sizeof(s_wifi.status), "%s", status);
}

static void sntp_sync_callback(struct timeval *tv)
{
    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    s_wifi.time_synced = true;
    set_status_locked("Time synchronized");
    xSemaphoreGive(s_wifi.lock);
    ESP_LOGI(TAG, "Time synchronized: %lu", (unsigned long)tv->tv_sec);
}

static void start_sntp(void)
{
    if (esp_sntp_enabled()) {
        esp_sntp_restart();
        return;
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.cloudflare.com");
    esp_sntp_set_time_sync_notification_cb(sntp_sync_callback);
    esp_sntp_init();
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;
    bool reconnect = false;

    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        set_status_locked("Wi-Fi started");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi.connected = false;
        s_wifi.rssi = 0;
        s_wifi.ip[0] = '\0';
        reconnect = s_wifi.has_credentials;
        s_wifi.connecting = reconnect;
        set_status_locked(reconnect ? "Reconnecting..." : "Disconnected");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_wifi.connected = true;
        s_wifi.connecting = false;
        snprintf(s_wifi.ip, sizeof(s_wifi.ip), IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(s_wifi.status, sizeof(s_wifi.status), "Connected: %s", s_wifi.ip);
        xSemaphoreGive(s_wifi.lock);
        start_sntp();
        return;
    }

    xSemaphoreGive(s_wifi.lock);

    if (reconnect) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
    }
}

static void wifi_scan_task(void *arg)
{
    (void)arg;

    wifi_ap_record_t ap_records[WIFI_TIME_MAX_SCAN_RESULTS];
    uint16_t ap_count = WIFI_TIME_MAX_SCAN_RESULTS;
    esp_err_t err;

    memset(ap_records, 0, sizeof(ap_records));
    err = esp_wifi_scan_start(NULL, true);
    if (err == ESP_OK) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
    }

    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    s_wifi.scanning = false;

    if (err != ESP_OK) {
        set_status_locked("Scan failed");
        xSemaphoreGive(s_wifi.lock);
        vTaskDelete(NULL);
        return;
    }

    s_wifi.scan_count = ap_count;
    for (size_t i = 0; i < ap_count; ++i) {
        snprintf(s_wifi.scan_results[i].ssid, sizeof(s_wifi.scan_results[i].ssid), "%s", ap_records[i].ssid);
        s_wifi.scan_results[i].rssi = ap_records[i].rssi;
        s_wifi.scan_results[i].authmode = ap_records[i].authmode;
    }
    s_wifi.scan_generation++;

    if (s_wifi.connected) {
        snprintf(s_wifi.status, sizeof(s_wifi.status), "Connected: %s", s_wifi.ip);
    } else {
        set_status_locked("Scan complete");
    }

    xSemaphoreGive(s_wifi.lock);
    vTaskDelete(NULL);
}

esp_err_t wifi_time_init(const app_settings_t *settings)
{
    esp_err_t err;

    if (s_wifi.initialized) {
        return ESP_OK;
    }

    memset(&s_wifi, 0, sizeof(s_wifi));
    s_wifi.lock = xSemaphoreCreateMutex();
    if (s_wifi.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    set_status_locked("Wi-Fi idle");

    ESP_ERROR_CHECK(esp_netif_init());
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        wifi_event_handler,
                                                        NULL,
                                                        &s_wifi_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        wifi_event_handler,
                                                        NULL,
                                                        &s_ip_got_ip));

    s_wifi.initialized = true;

    if (settings->wifi.ssid[0] != '\0') {
        snprintf(s_wifi.ssid, sizeof(s_wifi.ssid), "%s", settings->wifi.ssid);
        snprintf(s_wifi.password, sizeof(s_wifi.password), "%s", settings->wifi.password);
        s_wifi.has_credentials = true;
        return wifi_time_connect(settings->wifi.ssid, settings->wifi.password);
    }

    return ESP_OK;
}

void wifi_time_snapshot(app_runtime_state_t *runtime)
{
    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    runtime->wifi_connected = s_wifi.connected;
    runtime->wifi_connecting = s_wifi.connecting;
    runtime->wifi_scanning = s_wifi.scanning;
    runtime->time_synced = runtime->time_synced || s_wifi.time_synced;
    runtime->wifi_rssi = s_wifi.rssi;
    runtime->wifi_scan_generation = s_wifi.scan_generation;
    runtime->wifi_scan_count = s_wifi.scan_count;
    snprintf(runtime->wifi_ip, sizeof(runtime->wifi_ip), "%s", s_wifi.ip);
    snprintf(runtime->wifi_status, sizeof(runtime->wifi_status), "%s", s_wifi.status);
    for (size_t i = 0; i < CLOCK_WIFI_SCAN_RESULT_MAX; ++i) {
        if (i < s_wifi.scan_count) {
            snprintf(runtime->wifi_scan_results[i].ssid,
                     sizeof(runtime->wifi_scan_results[i].ssid),
                     "%s",
                     s_wifi.scan_results[i].ssid);
            runtime->wifi_scan_results[i].rssi = s_wifi.scan_results[i].rssi;
            runtime->wifi_scan_results[i].authmode = (int)s_wifi.scan_results[i].authmode;
        } else {
            runtime->wifi_scan_results[i].ssid[0] = '\0';
            runtime->wifi_scan_results[i].rssi = 0;
            runtime->wifi_scan_results[i].authmode = 0;
        }
    }
    xSemaphoreGive(s_wifi.lock);
}

esp_err_t wifi_time_start_scan(void)
{
    if (!s_wifi.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    if (s_wifi.scanning) {
        xSemaphoreGive(s_wifi.lock);
        return ESP_OK;
    }

    s_wifi.scanning = true;
    set_status_locked("Scanning...");
    xSemaphoreGive(s_wifi.lock);

    if (xTaskCreate(wifi_scan_task, "wifi_scan", 4096, NULL, 4, NULL) != pdPASS) {
        xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
        s_wifi.scanning = false;
        set_status_locked("Scan task failed");
        xSemaphoreGive(s_wifi.lock);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t wifi_time_connect(const char *ssid, const char *password)
{
    wifi_config_t config = {0};

    if (!s_wifi.initialized || ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf((char *)config.sta.ssid, sizeof(config.sta.ssid), "%s", ssid);
    snprintf((char *)config.sta.password, sizeof(config.sta.password), "%s", password ? password : "");
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;

    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    s_wifi.has_credentials = true;
    s_wifi.connecting = true;
    s_wifi.connected = false;
    s_wifi.time_synced = false;
    snprintf(s_wifi.ssid, sizeof(s_wifi.ssid), "%s", ssid);
    snprintf(s_wifi.password, sizeof(s_wifi.password), "%s", password ? password : "");
    snprintf(s_wifi.status, sizeof(s_wifi.status), "Connecting to %s", ssid);
    xSemaphoreGive(s_wifi.lock);

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_disconnect());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &config));
    return esp_wifi_connect();
}

esp_err_t wifi_time_forget(void)
{
    if (!s_wifi.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    s_wifi.has_credentials = false;
    s_wifi.connected = false;
    s_wifi.connecting = false;
    s_wifi.time_synced = false;
    s_wifi.ssid[0] = '\0';
    s_wifi.password[0] = '\0';
    s_wifi.ip[0] = '\0';
    set_status_locked("Wi-Fi not configured");
    xSemaphoreGive(s_wifi.lock);

    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }

    return esp_wifi_disconnect();
}

esp_err_t wifi_time_request_sync(void)
{
    if (!s_wifi.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    s_wifi.time_synced = false;
    set_status_locked("Syncing time...");
    xSemaphoreGive(s_wifi.lock);

    start_sntp();
    return ESP_OK;
}

bool wifi_time_is_scanning(void)
{
    bool scanning;

    xSemaphoreTake(s_wifi.lock, portMAX_DELAY);
    scanning = s_wifi.scanning;
    xSemaphoreGive(s_wifi.lock);

    return scanning;
}
