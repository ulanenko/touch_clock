file(GLOB_RECURSE UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/skin_fonts/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/ui_assets/*.c
)

idf_component_register(
    SRCS
        alarm_audio.c
        alarm_tones.c
        ota_update.c
        telemetry_update.c
        app/app_actions.c
        app/app_controller_core.c
        app/app_settings_storage.c
        app_settings.c
        clock_ui.c
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

# LVGL fixes not yet upstream (cmake/lvgl_patches.cmake): compile fixed copies of the
# affected LVGL sources in place of the managed originals.
idf_build_get_property(project_dir PROJECT_DIR)
include("${project_dir}/cmake/lvgl_patches.cmake")
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
idf_component_get_property(lvgl_dir lvgl__lvgl COMPONENT_DIR)
lvgl_patch_sources("${lvgl_dir}" "${CMAKE_CURRENT_BINARY_DIR}/lvgl_patched" lvgl_originals lvgl_patched)
foreach(original patched IN ZIP_LISTS lvgl_originals lvgl_patched)
    get_filename_component(original_dir "${original}" DIRECTORY)
    set_source_files_properties("${original}" DIRECTORY "${lvgl_dir}" PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties("${patched}" DIRECTORY "${lvgl_dir}" PROPERTIES COMPILE_OPTIONS "-iquote${original_dir}")
    target_sources(${lvgl_lib} PRIVATE "${patched}")
endforeach()

# LVGL's allocator (ui/ui_lv_mem.c, CONFIG_LV_USE_CUSTOM_MALLOC) is called only from LVGL,
# linked after this component: pull it in.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u lv_malloc_core")
