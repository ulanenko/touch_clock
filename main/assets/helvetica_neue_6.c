/*******************************************************************************
 * Size: 6 px
 * Bpp: 4
 * Opts: --size 6 --bpp 4 --format lvgl --font /tmp/HelveticaNeue.ttf --symbols ABCDEFGHIJKLMNOPQRSTUVWXYZ  --no-kerning --no-compress --lv-include lvgl.h --lv-font-name helvetica_neue_6 -o main/assets/helvetica_neue_6.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef HELVETICA_NEUE_6
#define HELVETICA_NEUE_6 1
#endif

#if HELVETICA_NEUE_6

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0041 "A" */
    0x0, 0x77, 0x0, 0x7, 0x80, 0x6, 0x89, 0x40,
    0x80, 0x9,

    /* U+0042 "B" */
    0x87, 0x77, 0x87, 0x86, 0x80, 0x9, 0x87, 0x76,

    /* U+0043 "C" */
    0x28, 0x78, 0x8, 0x0, 0x20, 0x90, 0x4, 0x2,
    0x87, 0x70,

    /* U+0044 "D" */
    0x87, 0x75, 0x80, 0x9, 0x80, 0x9, 0x87, 0x75,

    /* U+0045 "E" */
    0x87, 0x73, 0x87, 0x61, 0x80, 0x0, 0x87, 0x73,

    /* U+0046 "F" */
    0x87, 0x72, 0x87, 0x60, 0x80, 0x0, 0x80, 0x0,

    /* U+0047 "G" */
    0x28, 0x78, 0x8, 0x4, 0x82, 0x90, 0x7, 0x22,
    0x87, 0xb2,

    /* U+0048 "H" */
    0x80, 0x8, 0x87, 0x6b, 0x80, 0x8, 0x80, 0x8,

    /* U+0049 "I" */
    0x80, 0x80, 0x80, 0x80,

    /* U+004A "J" */
    0x0, 0x80, 0x8, 0x30, 0x98, 0x86,

    /* U+004B "K" */
    0x80, 0x56, 0x8, 0x76, 0x0, 0x84, 0x90, 0x8,
    0x2, 0x90,

    /* U+004C "L" */
    0x80, 0x0, 0x80, 0x0, 0x80, 0x0, 0x87, 0x72,

    /* U+004D "M" */
    0x86, 0x3, 0xb8, 0x80, 0x78, 0x83, 0x56, 0x88,
    0xb, 0x18,

    /* U+004E "N" */
    0x86, 0x8, 0x87, 0x28, 0x80, 0x98, 0x80, 0x2c,

    /* U+004F "O" */
    0x28, 0x77, 0x9, 0x0, 0x53, 0x90, 0x5, 0x32,
    0x87, 0x70,

    /* U+0050 "P" */
    0x87, 0x85, 0x80, 0x9, 0x87, 0x73, 0x80, 0x0,

    /* U+0051 "Q" */
    0x28, 0x77, 0x9, 0x0, 0x53, 0x90, 0x5, 0x32,
    0x89, 0xc0, 0x0, 0x0, 0x0,

    /* U+0052 "R" */
    0x87, 0x77, 0x80, 0x9, 0x87, 0x86, 0x80, 0x9,

    /* U+0053 "S" */
    0x57, 0x93, 0x66, 0x21, 0x31, 0x57, 0x57, 0x75,

    /* U+0054 "T" */
    0x7c, 0x73, 0x8, 0x0, 0x8, 0x0, 0x8, 0x0,

    /* U+0055 "U" */
    0x80, 0x9, 0x80, 0x9, 0x80, 0x9, 0x38, 0x76,

    /* U+0056 "V" */
    0x90, 0x17, 0x72, 0x71, 0x17, 0x80, 0x9, 0x50,

    /* U+0057 "W" */
    0x90, 0xa3, 0x35, 0x71, 0x77, 0x71, 0x38, 0x37,
    0x80, 0xd, 0x6, 0x70,

    /* U+0058 "X" */
    0x82, 0x63, 0xa, 0x70, 0x9, 0x80, 0x81, 0x54,

    /* U+0059 "Y" */
    0x91, 0x27, 0x9, 0x90, 0x5, 0x40, 0x5, 0x30,

    /* U+005A "Z" */
    0x47, 0xb5, 0x2, 0x80, 0x19, 0x0, 0xb7, 0x73
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 27, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 62, .box_w = 5, .box_h = 4, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 10, .adv_w = 66, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 18, .adv_w = 69, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 28, .adv_w = 68, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 36, .adv_w = 59, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 44, .adv_w = 55, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 52, .adv_w = 73, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 62, .adv_w = 69, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 70, .adv_w = 25, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 74, .adv_w = 50, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 80, .adv_w = 64, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 90, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 98, .adv_w = 84, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 69, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 73, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 73, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 147, .adv_w = 66, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 163, .adv_w = 55, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 171, .adv_w = 69, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 59, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 187, .adv_w = 89, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 59, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 215, .adv_w = 59, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0}
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
const lv_font_t helvetica_neue_6 = {
#else
lv_font_t helvetica_neue_6 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 5,          /*The maximum line height required by the font*/
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



#endif /*#if HELVETICA_NEUE_6*/

