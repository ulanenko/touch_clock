/*******************************************************************************
 * Size: 8 px
 * Bpp: 4
 * Opts: --size 8 --bpp 4 --format lvgl --font /tmp/HelveticaNeue.ttf --symbols ABCDEFGHIJKLMNOPQRSTUVWXYZ  --no-kerning --no-compress --lv-include lvgl.h --lv-font-name helvetica_neue_8 -o main/assets/helvetica_neue_8.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef HELVETICA_NEUE_8
#define HELVETICA_NEUE_8 1
#endif

#if HELVETICA_NEUE_8

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0041 "A" */
    0x0, 0x1e, 0x30, 0x0, 0x7, 0x5a, 0x0, 0x0,
    0xc0, 0xb2, 0x0, 0x6c, 0x9b, 0x90, 0xc, 0x10,
    0xc, 0x10,

    /* U+0042 "B" */
    0x5c, 0x9a, 0x70, 0x56, 0x0, 0xb0, 0x5c, 0x9b,
    0x70, 0x56, 0x0, 0xb1, 0x5c, 0x9a, 0x90,

    /* U+0043 "C" */
    0x8, 0x9a, 0x90, 0x66, 0x0, 0x42, 0x92, 0x0,
    0x0, 0x76, 0x0, 0x64, 0x9, 0x99, 0x80,

    /* U+0044 "D" */
    0x5c, 0x9a, 0x60, 0x56, 0x0, 0xa2, 0x56, 0x0,
    0x74, 0x56, 0x0, 0xa2, 0x5c, 0x9a, 0x60,

    /* U+0045 "E" */
    0x5c, 0x99, 0x55, 0x60, 0x0, 0x5c, 0x99, 0x35,
    0x60, 0x0, 0x5c, 0x99, 0x50,

    /* U+0046 "F" */
    0x5c, 0x99, 0x35, 0x60, 0x0, 0x5c, 0x99, 0x5,
    0x60, 0x0, 0x56, 0x0, 0x0,

    /* U+0047 "G" */
    0x8, 0x99, 0x90, 0x66, 0x0, 0x33, 0x92, 0x8,
    0x97, 0x66, 0x0, 0x59, 0x8, 0x99, 0x99,

    /* U+0048 "H" */
    0x56, 0x0, 0x92, 0x56, 0x0, 0x92, 0x5c, 0x99,
    0xd2, 0x56, 0x0, 0x92, 0x56, 0x0, 0x92,

    /* U+0049 "I" */
    0x56, 0x56, 0x56, 0x56, 0x56,

    /* U+004A "J" */
    0x0, 0x38, 0x0, 0x38, 0x0, 0x38, 0x90, 0x48,
    0x6a, 0xb3,

    /* U+004B "K" */
    0x56, 0x5, 0x90, 0x56, 0x77, 0x0, 0x5d, 0xb4,
    0x0, 0x56, 0xb, 0x20, 0x56, 0x1, 0xc1,

    /* U+004C "L" */
    0x56, 0x0, 0x5, 0x60, 0x0, 0x56, 0x0, 0x5,
    0x60, 0x0, 0x5c, 0x99, 0x30,

    /* U+004D "M" */
    0x5e, 0x0, 0xe, 0x55, 0xb5, 0x5, 0xb5, 0x56,
    0xa0, 0xa6, 0x55, 0x57, 0x67, 0x65, 0x55, 0x1e,
    0x16, 0x50,

    /* U+004E "N" */
    0x6c, 0x0, 0x82, 0x69, 0x80, 0x82, 0x65, 0x84,
    0x82, 0x65, 0xa, 0xa2, 0x65, 0x1, 0xe2,

    /* U+004F "O" */
    0x8, 0x99, 0x90, 0x75, 0x0, 0x48, 0xa1, 0x0,
    0xb, 0x75, 0x0, 0x48, 0x8, 0x99, 0x90,

    /* U+0050 "P" */
    0x5c, 0x9a, 0x75, 0x60, 0xc, 0x5c, 0x9a, 0x45,
    0x60, 0x0, 0x56, 0x0, 0x0,

    /* U+0051 "Q" */
    0x8, 0x99, 0x90, 0x75, 0x0, 0x48, 0xa1, 0x0,
    0xb, 0x75, 0x4, 0x57, 0x8, 0x9b, 0xd3, 0x0,
    0x0, 0x1,

    /* U+0052 "R" */
    0x5c, 0x99, 0x90, 0x56, 0x0, 0xc0, 0x5c, 0x9b,
    0x80, 0x56, 0x0, 0xc0, 0x56, 0x0, 0xb0,

    /* U+0053 "S" */
    0x2a, 0x9b, 0x27, 0x50, 0x14, 0x6, 0x88, 0x37,
    0x10, 0xc, 0x2a, 0x9a, 0x50,

    /* U+0054 "T" */
    0x9a, 0xd9, 0x50, 0x1a, 0x0, 0x1, 0xa0, 0x0,
    0x1a, 0x0, 0x1, 0xa0, 0x0,

    /* U+0055 "U" */
    0x65, 0x0, 0x92, 0x65, 0x0, 0x92, 0x65, 0x0,
    0x92, 0x57, 0x0, 0xa1, 0xa, 0x9a, 0x80,

    /* U+0056 "V" */
    0xc0, 0x1, 0xa6, 0x60, 0x74, 0xb, 0xb, 0x0,
    0x95, 0x70, 0x3, 0xe1, 0x0,

    /* U+0057 "W" */
    0xb0, 0xd, 0x40, 0x92, 0x74, 0x28, 0x80, 0xb0,
    0x28, 0x72, 0xa2, 0x80, 0xa, 0x90, 0x79, 0x30,
    0x9, 0x80, 0x2e, 0x0,

    /* U+0058 "X" */
    0x86, 0x7, 0x60, 0xa6, 0x90, 0x3, 0xf1, 0x0,
    0xb4, 0xa0, 0x93, 0x6, 0x70,

    /* U+0059 "Y" */
    0x94, 0x1, 0xb0, 0xb, 0x1a, 0x20, 0x2, 0xd5,
    0x0, 0x0, 0xc0, 0x0, 0x0, 0xc0, 0x0,

    /* U+005A "Z" */
    0x59, 0x9c, 0x80, 0x2, 0xa0, 0x2, 0xb0, 0x1,
    0xb1, 0x0, 0xcb, 0x99, 0x70
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 36, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 83, .box_w = 7, .box_h = 5, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 18, .adv_w = 88, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 33, .adv_w = 92, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 90, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 76, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 89, .adv_w = 97, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 104, .adv_w = 92, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 119, .adv_w = 33, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 66, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 85, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 149, .adv_w = 71, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 162, .adv_w = 111, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 180, .adv_w = 92, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 195, .adv_w = 97, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 210, .adv_w = 83, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 223, .adv_w = 97, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 241, .adv_w = 88, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 256, .adv_w = 83, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 92, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 297, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 310, .adv_w = 119, .box_w = 8, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 330, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 343, .adv_w = 83, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 358, .adv_w = 78, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0}
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
const lv_font_t helvetica_neue_8 = {
#else
lv_font_t helvetica_neue_8 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 6,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if HELVETICA_NEUE_8*/

