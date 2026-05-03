cmake_minimum_required(VERSION 3.20)

project(touch_clock_host_tests C)

enable_testing()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_library(touch_clock_host_core STATIC
    ../../main/domain/face_catalog.c
    ../../main/domain/settings_policy.c
    ../../main/domain/brightness_policy.c
    ../../main/domain/alarm_scheduler.c
    ../../main/domain/timezone_rules.c
    ../../main/app/app_actions.c
    ../../main/app/app_controller_core.c
    ../../main/app/app_settings_storage.c
)

target_include_directories(touch_clock_host_core PUBLIC ../../main fakes)
target_compile_options(touch_clock_host_core PRIVATE -Wall -Wextra -Werror)

function(add_touch_clock_host_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE touch_clock_host_core)
    target_include_directories(${name} PRIVATE ../../main . fakes)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_touch_clock_host_test(face_catalog_test face_catalog_test.c)
add_touch_clock_host_test(settings_policy_test settings_policy_test.c)
add_touch_clock_host_test(brightness_policy_test brightness_policy_test.c)
add_touch_clock_host_test(alarm_scheduler_test alarm_scheduler_test.c)
add_touch_clock_host_test(timezone_rules_test timezone_rules_test.c)
add_touch_clock_host_test(app_actions_test app_actions_test.c)
add_touch_clock_host_test(app_controller_core_test app_controller_core_test.c)
add_touch_clock_host_test(app_settings_storage_test app_settings_storage_test.c)
