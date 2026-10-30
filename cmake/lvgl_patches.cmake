# Fixes for LVGL bugs that upstream has not shipped yet, applied at configure time without
# touching the managed copy of LVGL: the fixed source is written to the build directory and
# compiled in place of the original. Used by the firmware (main/CMakeLists.txt) and the
# simulator (sim/CMakeLists.txt).
#
# lvgl_patch_sources(<lvgl_dir> <out_dir> <originals_var> <patched_var>) sets the two lists:
# the LVGL sources to leave out and the fixed copies to compile instead. The caller marks the
# originals HEADER_FILE_ONLY and compiles each copy with -iquote<the original's directory>
# (the files include their neighbours by relative path), both in the scope of the LVGL target.

# LVGL 9.3.0 never frees the radius mask of an image drawn with clip_radius (radius_only in
# lv_draw_sw_img.c), so every such draw pins a circle-cache entry for good. The face reveal
# draws one per frame with a growing radius, so after a few swipes the cache is full and each
# reveal frame leaks a temporary entry.
set(LVGL_FIX_IMG_FILE "src/draw/sw/lv_draw_sw_img.c")
set(LVGL_FIX_IMG_BEFORE "        blend_area.y2 ++;\n    }\n    lv_free(mask_buf);\n\n}\n#endif /*LV_DRAW_SW_COMPLEX*/")
set(LVGL_FIX_IMG_AFTER
    "        blend_area.y2 ++;\n    }\n    lv_draw_sw_mask_free_param(&mask_param);\n    lv_free(mask_buf);\n\n}\n#endif /*LV_DRAW_SW_COMPLEX*/")

function(lvgl_patch_sources lvgl_dir out_dir originals_var patched_var)
    set(original "${lvgl_dir}/${LVGL_FIX_IMG_FILE}")
    set(originals "")
    set(patched "")

    if(EXISTS "${original}")
        file(READ "${original}" text)
        string(FIND "${text}" "${LVGL_FIX_IMG_AFTER}" already_fixed)
        string(FIND "${text}" "${LVGL_FIX_IMG_BEFORE}" fixable)
        if(already_fixed GREATER -1)
            message(STATUS "LVGL already frees the clip_radius mask; no patch needed")
        elseif(fixable EQUAL -1)
            message(FATAL_ERROR "LVGL clip_radius mask fix does not apply to ${original}; check whether LVGL "
                            "frees the mask in radius_only() and update cmake/lvgl_patches.cmake")
        else()
            string(REPLACE "${LVGL_FIX_IMG_BEFORE}" "${LVGL_FIX_IMG_AFTER}" text "${text}")
            set(copy "${out_dir}/lv_draw_sw_img.c")
            # Rewrite only on change, so an unchanged LVGL does not rebuild the file.
            set(old "")
            if(EXISTS "${copy}")
                file(READ "${copy}" old)
            endif()
            if(NOT old STREQUAL text)
                file(WRITE "${copy}" "${text}")
            endif()
            list(APPEND originals "${original}")
            list(APPEND patched "${copy}")
        endif()
    endif()
    set(${originals_var} "${originals}" PARENT_SCOPE)
    set(${patched_var} "${patched}" PARENT_SCOPE)
endfunction()
