/*******************************************************************************
 * Size: 5 px
 * Bpp: 4
 * Opts: --size 5 --bpp 4 --format lvgl --font /tmp/HelveticaNeue.ttf --symbols ABCDEFGHIJKLMNOPQRSTUVWXYZ  --no-kerning --no-compress --lv-include lvgl.h --lv-font-name helvetica_neue_5 -o main/assets/helvetica_neue_5.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef HELVETICA_NEUE_5
#define HELVETICA_NEUE_5 1
#endif

#if HELVETICA_NEUE_5

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0041 "A" */
    0x0, 0xb1, 0x0, 0x25, 0x60, 0x7, 0x79, 0x0,
    0x70, 0x61,

    /* U+0042 "B" */
    0x87, 0x90, 0x87, 0x90, 0x70, 0x52, 0x87, 0x90,

    /* U+0043 "C" */
    0x37, 0x91, 0x70, 0x1, 0x70, 0x12, 0x47, 0x81,

    /* U+0044 "D" */
    0x87, 0x80, 0x70, 0x34, 0x70, 0x33, 0x87, 0x80,

    /* U+0045 "E" */
    0x87, 0x68, 0x65, 0x70, 0x8, 0x76,

    /* U+0046 "F" */
    0x87, 0x58, 0x63, 0x70, 0x7, 0x0,

    /* U+0047 "G" */
    0x37, 0x72, 0x70, 0x65, 0x70, 0x17, 0x37, 0x96,

    /* U+0048 "H" */
    0x70, 0x33, 0x86, 0x93, 0x70, 0x33, 0x70, 0x33,

    /* U+0049 "I" */
    0x77, 0x77,

    /* U+004A "J" */
    0x4, 0x30, 0x43, 0x24, 0x38, 0x91,

    /* U+004B "K" */
    0x70, 0x80, 0x79, 0x10, 0x85, 0x50, 0x70, 0x71,

    /* U+004C "L" */
    0x70, 0x7, 0x0, 0x70, 0x8, 0x75,

    /* U+004D "M" */
    0x93, 0xc, 0x76, 0x3a, 0x66, 0x67, 0x63, 0x77,

    /* U+004E "N" */
    0x93, 0x33, 0x78, 0x33, 0x62, 0x83, 0x60, 0x93,

    /* U+004F "O" */
    0x37, 0x81, 0x70, 0x7, 0x70, 0x7, 0x47, 0x81,

    /* U+0050 "P" */
    0x87, 0x90, 0x70, 0x70, 0x87, 0x60, 0x70, 0x0,

    /* U+0051 "Q" */
    0x37, 0x81, 0x70, 0x7, 0x70, 0x17, 0x47, 0xc4,
    0x0, 0x0,

    /* U+0052 "R" */
    0x87, 0x90, 0x70, 0x51, 0x87, 0xa0, 0x70, 0x51,

    /* U+0053 "S" */
    0x57, 0x70, 0x75, 0x20, 0x22, 0x90, 0x67, 0x80,

    /* U+0054 "T" */
    0x7b, 0x60, 0x70, 0x7, 0x0, 0x70,

    /* U+0055 "U" */
    0x70, 0x33, 0x70, 0x33, 0x70, 0x43, 0x47, 0x80,

    /* U+0056 "V" */
    0x80, 0x70, 0x70, 0x70, 0x36, 0x40, 0xc, 0x0,

    /* U+0057 "W" */
    0x71, 0xa0, 0x67, 0x47, 0x33, 0x57, 0x38, 0x2,
    0x90, 0xa0,

    /* U+0058 "X" */
    0x80, 0x80, 0x1b, 0x10, 0x2b, 0x20, 0x80, 0x80,

    /* U+0059 "Y" */
    0x80, 0x70, 0x28, 0x50, 0x8, 0x0, 0x7, 0x0,

    /* U+005A "Z" */
    0x57, 0xb0, 0x62, 0x26, 0xb, 0x76
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 22, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 52, .box_w = 5, .box_h = 4, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 10, .adv_w = 55, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 18, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 26, .adv_w = 56, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 34, .adv_w = 49, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 40, .adv_w = 46, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 46, .adv_w = 61, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 54, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 62, .adv_w = 21, .box_w = 1, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 64, .adv_w = 42, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 70, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 44, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 84, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 92, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 100, .adv_w = 61, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 52, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 61, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 126, .adv_w = 55, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 52, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 46, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 148, .adv_w = 58, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 49, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 164, .adv_w = 74, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 49, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 52, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 190, .adv_w = 49, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 65, .range_length = 26, .glyph_id_start = 2,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 2,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t helvetica_neue_5 = {
#else
lv_font_t helvetica_neue_5 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 5,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if HELVETICA_NEUE_5*/

