cmake_minimum_required(VERSION 3.20)

project(touch_clock_sim C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS ON)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_compile_definitions(SIM_COST_MODEL="${CMAKE_CURRENT_SOURCE_DIR}/cost_model.txt")
set(LVGL_DIR ${REPO_ROOT}/managed_components/lvgl__lvgl CACHE PATH "LVGL 9.3 source (managed component)")
set(SDKCONFIG ${REPO_ROOT}/sdkconfig CACHE FILEPATH "Project sdkconfig to mirror")

option(SIM_ASAN "Build with AddressSanitizer and UBSan" OFF)
if(SIM_ASAN)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(ZLIB REQUIRED)

set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen)
add_custom_command(
    OUTPUT ${GEN_DIR}/sim_sdkconfig.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN_DIR}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_sdkconfig.py ${SDKCONFIG} ${GEN_DIR}/sim_sdkconfig.h
    DEPENDS ${SDKCONFIG} ${CMAKE_CURRENT_SOURCE_DIR}/gen_sdkconfig.py
)
add_custom_target(sim_sdkconfig DEPENDS ${GEN_DIR}/sim_sdkconfig.h)

set(SIM_DEFS LV_CONF_KCONFIG_EXTERNAL_INCLUDE="sim_sdkconfig.h" LV_LVGL_H_INCLUDE_SIMPLE)
set(SIM_INCLUDES ${GEN_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/stubs ${LVGL_DIR} ${LVGL_DIR}/src)

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS ${LVGL_DIR}/src/*.c)
# The firmware's LVGL fixes (cmake/lvgl_patches.cmake), so the simulator runs the same LVGL.
include(${REPO_ROOT}/cmake/lvgl_patches.cmake)
lvgl_patch_sources(${LVGL_DIR} ${CMAKE_CURRENT_BINARY_DIR}/lvgl_patched LVGL_ORIGINALS LVGL_PATCHED)
foreach(original patched IN ZIP_LISTS LVGL_ORIGINALS LVGL_PATCHED)
    get_filename_component(original_dir ${original} DIRECTORY)
    list(REMOVE_ITEM LVGL_SOURCES ${original})
    list(APPEND LVGL_SOURCES ${patched})
    set_source_files_properties(${patched} PROPERTIES COMPILE_OPTIONS "-iquote${original_dir}")
endforeach()
add_library(lvgl STATIC ${LVGL_SOURCES})
add_dependencies(lvgl sim_sdkconfig)
target_compile_definitions(lvgl PUBLIC ${SIM_DEFS})
target_include_directories(lvgl PUBLIC ${SIM_INCLUDES})
target_compile_options(lvgl PRIVATE -O2 -w)

file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
    ${REPO_ROOT}/main/ui/*.c
    ${REPO_ROOT}/main/assets/*.c
    ${REPO_ROOT}/main/domain/*.c
)
add_executable(sim
    sim_main.c
    ${UI_SOURCES}
    ${REPO_ROOT}/main/clock_ui.c
    ${REPO_ROOT}/main/clock_model.c
    ${REPO_ROOT}/main/localization.c
)
target_include_directories(sim PRIVATE ${REPO_ROOT}/main)
target_link_libraries(sim PRIVATE lvgl ZLIB::ZLIB m)
target_compile_options(sim PRIVATE -O1 -g -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable)
set_source_files_properties(${UI_SOURCES} ${REPO_ROOT}/main/clock_ui.c
    PROPERTIES COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/stubs/sim_time.h")
