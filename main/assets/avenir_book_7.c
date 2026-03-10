/*******************************************************************************
 * Size: 7 px
 * Bpp: 4
 * Opts: --size 7 --bpp 4 --format lvgl --font /tmp/Avenir-Book.ttf --symbols 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ  --no-kerning --no-compress --lv-include lvgl.h --lv-font-name avenir_book_7 -o main/assets/avenir_book_7.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef AVENIR_BOOK_7
#define AVENIR_BOOK_7 1
#endif

#if AVENIR_BOOK_7

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0030 "0" */
    0x47, 0x72, 0x80, 0x7, 0x80, 0x7, 0x47, 0x72,

    /* U+0031 "1" */
    0x8, 0x70, 0x17, 0x1, 0x70, 0x17,

    /* U+0032 "2" */
    0x38, 0x73, 0x0, 0x44, 0x5, 0x40, 0x79, 0x63,

    /* U+0033 "3" */
    0x26, 0x82, 0x3, 0xa0, 0x0, 0x25, 0x57, 0x81,

    /* U+0034 "4" */
    0x3, 0xb0, 0x24, 0x80, 0x76, 0xb4, 0x0, 0x80,

    /* U+0035 "5" */
    0x48, 0x61, 0x36, 0x81, 0x0, 0x26, 0x47, 0x81,

    /* U+0036 "6" */
    0x4, 0x30, 0x29, 0x71, 0x70, 0x7, 0x47, 0x73,

    /* U+0037 "7" */
    0x46, 0xa3, 0x0, 0x80, 0x6, 0x20, 0x8, 0x0,

    /* U+0038 "8" */
    0x47, 0x82, 0x28, 0x91, 0x70, 0x6, 0x47, 0x72,

    /* U+0039 "9" */
    0x47, 0x73, 0x80, 0x7, 0x27, 0x91, 0x5, 0x30,

    /* U+0041 "A" */
    0x2, 0xa0, 0x0, 0x61, 0x50, 0x38, 0x69, 0x8,
    0x0, 0x7,

    /* U+0042 "B" */
    0x58, 0x67, 0x57, 0x76, 0x52, 0x9, 0x58, 0x67,

    /* U+0043 "C" */
    0x7, 0x66, 0x37, 0x10, 0x0, 0x71, 0x0, 0x0,
    0x76, 0x64,

    /* U+0044 "D" */
    0x67, 0x68, 0x26, 0x10, 0x8, 0x61, 0x0, 0x86,
    0x76, 0x82,

    /* U+0045 "E" */
    0x58, 0x64, 0x58, 0x63, 0x52, 0x0, 0x58, 0x65,

    /* U+0046 "F" */
    0x58, 0x64, 0x58, 0x63, 0x52, 0x0, 0x52, 0x0,

    /* U+0047 "G" */
    0x7, 0x66, 0x47, 0x10, 0x57, 0x71, 0x0, 0x80,
    0x76, 0x67,

    /* U+0048 "H" */
    0x52, 0x2, 0x65, 0x86, 0x76, 0x52, 0x2, 0x65,
    0x20, 0x26,

    /* U+0049 "I" */
    0x52, 0x52, 0x52, 0x52,

    /* U+004A "J" */
    0x0, 0x80, 0x8, 0x0, 0x86, 0x66,

    /* U+004B "K" */
    0x52, 0x45, 0x5, 0xb2, 0x0, 0x53, 0x91, 0x5,
    0x20, 0x91,

    /* U+004C "L" */
    0x52, 0x0, 0x52, 0x0, 0x52, 0x0, 0x58, 0x63,

    /* U+004D "M" */
    0x59, 0x0, 0x68, 0x57, 0x20, 0x78, 0x52, 0x65,
    0x18, 0x52, 0x36, 0x8,

    /* U+004E "N" */
    0x5a, 0x0, 0x85, 0x48, 0x8, 0x52, 0x36, 0x85,
    0x20, 0x4b,

    /* U+004F "O" */
    0x7, 0x66, 0x70, 0x71, 0x0, 0x35, 0x71, 0x0,
    0x35, 0x7, 0x66, 0x70,

    /* U+0050 "P" */
    0x58, 0x67, 0x58, 0x64, 0x52, 0x0, 0x52, 0x0,

    /* U+0051 "Q" */
    0x7, 0x67, 0x60, 0x70, 0x0, 0x53, 0x71, 0x0,
    0x53, 0x8, 0x68, 0xb4,

    /* U+0052 "R" */
    0x58, 0x67, 0x58, 0x94, 0x52, 0x71, 0x52, 0x8,

    /* U+0053 "S" */
    0x47, 0x72, 0x56, 0x10, 0x0, 0x55, 0x36, 0x73,

    /* U+0054 "T" */
    0x58, 0x95, 0x4, 0x40, 0x4, 0x40, 0x4, 0x40,

    /* U+0055 "U" */
    0x61, 0x4, 0x36, 0x10, 0x43, 0x61, 0x5, 0x31,
    0x86, 0x80,

    /* U+0056 "V" */
    0x8, 0x0, 0x61, 0x5, 0x20, 0x70, 0x0, 0x64,
    0x10, 0x0, 0x56, 0x0,

    /* U+0057 "W" */
    0x80, 0x2b, 0x2, 0x66, 0x6, 0x62, 0x71, 0x15,
    0x60, 0x78, 0x0, 0x83, 0x8, 0x50,

    /* U+0058 "X" */
    0x54, 0x8, 0x0, 0x67, 0x10, 0x8, 0x82, 0x7,
    0x20, 0x91,

    /* U+0059 "Y" */
    0x8, 0x10, 0x80, 0x0, 0x88, 0x0, 0x0, 0x44,
    0x0, 0x0, 0x44, 0x0,

    /* U+005A "Z" */
    0x56, 0x88, 0x1, 0x80, 0x8, 0x0, 0xa7, 0x64
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 31, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 8, .adv_w = 62, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 14, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 22, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 30, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 38, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 46, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 54, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 62, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 70, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 77, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 88, .adv_w = 71, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 96, .adv_w = 79, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 106, .adv_w = 83, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 66, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 87, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 81, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 30, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 54, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 162, .adv_w = 71, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 172, .adv_w = 56, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 180, .adv_w = 100, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 192, .adv_w = 87, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 202, .adv_w = 93, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 214, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 222, .adv_w = 93, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 234, .adv_w = 66, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 242, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 250, .adv_w = 64, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 258, .adv_w = 77, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 268, .adv_w = 68, .box_w = 6, .box_h = 4, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 280, .adv_w = 106, .box_w = 7, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 294, .adv_w = 71, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 304, .adv_w = 64, .box_w = 6, .box_h = 4, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 316, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0}
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
        .range_start = 48, .range_length = 10, .glyph_id_start = 2,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 65, .range_length = 26, .glyph_id_start = 12,
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
    .cmap_num = 3,
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
const lv_font_t avenir_book_7 = {
#else
lv_font_t avenir_book_7 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 4,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
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



#endif /*#if AVENIR_BOOK_7*/

