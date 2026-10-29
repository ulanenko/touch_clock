#include "bsp/esp-bsp.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "app_controller.h"

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        }
    };
    esp_err_t err;

    /* The adapter's 8 KB left the UI thread 504 bytes at its deepest in a 16-loop soak (snapshots
     * of the faces, Liquid Glass bakes): 2 KB more, from internal RAM that kept ~24 KB free. */
    cfg.lv_adapter_cfg.task_stack_size = 10 * 1024;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(app_controller_start(&cfg));
}
