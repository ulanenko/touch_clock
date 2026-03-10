/*******************************************************************************
 * Size: 6 px
 * Bpp: 4
 * Opts: --size 6 --bpp 4 --format lvgl --font /tmp/Avenir-Book.ttf --symbols 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ  --no-kerning --no-compress --lv-include lvgl.h --lv-font-name avenir_book_6 -o main/assets/avenir_book_6.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef AVENIR_BOOK_6
#define AVENIR_BOOK_6 1
#endif

#if AVENIR_BOOK_6

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0030 "0" */
    0x47, 0x70, 0x60, 0x60, 0x60, 0x60, 0x46, 0x70,

    /* U+0031 "1" */
    0x1a, 0x10, 0x51, 0x5, 0x10, 0x51,

    /* U+0032 "2" */
    0x47, 0x70, 0x7, 0x7, 0x18, 0x76,

    /* U+0033 "3" */
    0x26, 0x70, 0x56, 0x0, 0x76, 0x66,

    /* U+0034 "4" */
    0x5, 0x70, 0x33, 0x70, 0x66, 0xa0, 0x0, 0x70,

    /* U+0035 "5" */
    0x67, 0x53, 0x66, 0x0, 0x65, 0x66,

    /* U+0036 "6" */
    0x6, 0x0, 0x38, 0x50, 0x60, 0x60, 0x56, 0x70,

    /* U+0037 "7" */
    0x46, 0x90, 0x25, 0x7, 0x2, 0x50,

    /* U+0038 "8" */
    0x47, 0x70, 0x47, 0x70, 0x60, 0x60, 0x47, 0x70,

    /* U+0039 "9" */
    0x57, 0x70, 0x70, 0x60, 0x37, 0x70, 0x6, 0x0,

    /* U+0041 "A" */
    0x5, 0x60, 0x0, 0x55, 0x0, 0x47, 0x65, 0x7,
    0x0, 0x60,

    /* U+0042 "B" */
    0x66, 0x81, 0x66, 0x90, 0x60, 0x15, 0x66, 0x72,

    /* U+0043 "C" */
    0x17, 0x66, 0x7, 0x0, 0x0, 0x70, 0x0, 0x1,
    0x76, 0x60,

    /* U+0044 "D" */
    0x76, 0x75, 0x7, 0x0, 0x60, 0x70, 0x6, 0x7,
    0x67, 0x50,

    /* U+0045 "E" */
    0x66, 0x61, 0x66, 0x60, 0x60, 0x0, 0x66, 0x61,

    /* U+0046 "F" */
    0x66, 0x60, 0x66, 0x60, 0x60, 0x0, 0x60, 0x0,

    /* U+0047 "G" */
    0x17, 0x66, 0x7, 0x2, 0x81, 0x70, 0x5, 0x11,
    0x76, 0x81,

    /* U+0048 "H" */
    0x60, 0x6, 0x66, 0x68, 0x60, 0x6, 0x60, 0x6,

    /* U+0049 "I" */
    0x60, 0x60, 0x60, 0x60,

    /* U+004A "J" */
    0x1, 0x60, 0x16, 0x1, 0x56, 0x72,

    /* U+004B "K" */
    0x61, 0x62, 0x69, 0x0, 0x63, 0x60, 0x60, 0x45,

    /* U+004C "L" */
    0x60, 0x6, 0x0, 0x60, 0x6, 0x66,

    /* U+004D "M" */
    0x66, 0x1, 0xb6, 0x60, 0x57, 0x62, 0x35, 0x76,
    0x7, 0x7,

    /* U+004E "N" */
    0x66, 0x5, 0x16, 0x43, 0x51, 0x60, 0x76, 0x16,
    0x0, 0xb1,

    /* U+004F "O" */
    0x17, 0x67, 0x17, 0x0, 0x7, 0x70, 0x0, 0x71,
    0x76, 0x71,

    /* U+0050 "P" */
    0x66, 0x81, 0x66, 0x70, 0x60, 0x0, 0x60, 0x0,

    /* U+0051 "Q" */
    0x17, 0x67, 0x7, 0x0, 0x6, 0x70, 0x0, 0x61,
    0x77, 0xb6,

    /* U+0052 "R" */
    0x66, 0x82, 0x67, 0x70, 0x60, 0x70, 0x60, 0x52,

    /* U+0053 "S" */
    0x57, 0x60, 0x64, 0x0, 0x1, 0x80, 0x46, 0x70,

    /* U+0054 "T" */
    0x6a, 0x62, 0x7, 0x0, 0x7, 0x0, 0x7, 0x0,

    /* U+0055 "U" */
    0x70, 0x6, 0x70, 0x6, 0x70, 0x6, 0x27, 0x73,

    /* U+0056 "V" */
    0x7, 0x0, 0x60, 0x50, 0x51, 0x0, 0x55, 0x0,
    0x6, 0x30,

    /* U+0057 "W" */
    0x70, 0x83, 0x7, 0x60, 0x66, 0x42, 0x34, 0x36,
    0x60, 0xa, 0x4, 0x70,

    /* U+0058 "X" */
    0x61, 0x34, 0x6, 0x50, 0x7, 0x70, 0x70, 0x35,

    /* U+0059 "Y" */
    0x8, 0x5, 0x30, 0x17, 0x60, 0x0, 0x70, 0x0,
    0x7, 0x0,

    /* U+005A "Z" */
    0x56, 0xb1, 0x3, 0x40, 0x16, 0x0, 0xa6, 0x61
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 27, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 8, .adv_w = 53, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 14, .adv_w = 53, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 20, .adv_w = 53, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 26, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 34, .adv_w = 53, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 40, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 53, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 54, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 62, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 70, .adv_w = 66, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 80, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 88, .adv_w = 68, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 98, .adv_w = 71, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 57, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 75, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 69, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 25, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 146, .adv_w = 46, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 85, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 75, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 186, .adv_w = 80, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 196, .adv_w = 55, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 204, .adv_w = 80, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 214, .adv_w = 57, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 222, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 230, .adv_w = 55, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 238, .adv_w = 66, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 246, .adv_w = 59, .box_w = 5, .box_h = 4, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 256, .adv_w = 91, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 268, .adv_w = 60, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 276, .adv_w = 55, .box_w = 5, .box_h = 4, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 286, .adv_w = 53, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0}
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
const lv_font_t avenir_book_6 = {
#else
lv_font_t avenir_book_6 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 4,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
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



#endif /*#if AVENIR_BOOK_6*/

