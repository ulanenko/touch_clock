file(GLOB_RECURSE UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/skin_fonts/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/ui_assets/*.c
)

idf_component_register(
    SRCS
        alarm_audio.c
        alarm_tones.c
        alarm_logic.c
        ota_update.c
        telemetry_update.c
        app/app_actions.c
        app/app_controller_core.c
        app/app_settings_storage.c
        app_settings.c
        clock_ui.c
        clock_model.c
        localization.c
        domain/alarm_scheduler.c
        domain/brightness_policy.c
        domain/face_catalog.c
        domain/settings_policy.c
        domain/timezone_rules.c
        main.c
        app_controller.c
        platform/platform_esp_services.c
        wifi_time.c
        assets/alarm_pcm.c
        assets/dseg7_classic_italic_112.c
        assets/dseg7_classic_italic_56.c
        assets/dseg14_classic_italic_36.c
        assets/dseg14_classic_italic_24.c
        assets/dseg14_classic_italic_20.c
        assets/jost_regular_34.c
        assets/jost_medium_20.c
        assets/jost_regular_11.c
        assets/jost_medium_10.c
        assets/avenir_book_22.c
        assets/avenir_book_8.c
        assets/avenir_book_6.c
        assets/avenir_book_9.c
        assets/avenir_book_7.c
        assets/helvetica_neue_8.c
        assets/helvetica_neue_5.c
        assets/helvetica_neue_10.c
        assets/helvetica_neue_6.c
        assets/montserrat_digits_56.c
        assets/montserrat_math_72.c
        assets/ui_i18n_20.c
        assets/ui_i18n_24.c
        ${UI_SOURCES}
    INCLUDE_DIRS .
    REQUIRES esp_wifi
    PRIV_REQUIRES app_update esp_driver_ppa esp_http_client esp_hw_support esp_mm esp_timer mbedtls nvs_flash)

# LVGL 9.3.0 never frees the radius mask of an image drawn with clip_radius
# (lv_draw_sw_img.c, end of radius_only), so every such draw pins a circle-cache entry for
# good. The face reveal draws one per frame with a growing radius; after a few reveals the
# cache is full and every rounded corner is recomputed each frame (a 500x160 card: 1 ms ->
# 17 ms on the device). The managed copy carries the one-line fix
# (lv_draw_sw_mask_free_param(&mask_param) before lv_free(mask_buf)); warn when a fresh
# download of LVGL lacks it.
idf_build_get_property(project_dir PROJECT_DIR)
set(lv_img_src "${project_dir}/managed_components/lvgl__lvgl/src/draw/sw/lv_draw_sw_img.c")
if(EXISTS "${lv_img_src}")
    file(READ "${lv_img_src}" lv_img_text)
    string(FIND "${lv_img_text}" "lv_draw_sw_mask_free_param(&mask_param);" lv_img_fixed)
    if(lv_img_fixed EQUAL -1)
        message(WARNING "LVGL lacks the clip_radius mask fix: add lv_draw_sw_mask_free_param(&mask_param); "
                        "before the last lv_free(mask_buf) in ${lv_img_src}. Without it rounded corners "
                        "get much slower after a few face swipes.")
    endif()
endif()
