/*******************************************************************************
 * Size: 30 px
 * Bpp: 4
 * Opts: --font Inter[opsz,wght].ttf (fallback glyphs) -r 0xB5 --size 30 --bpp 4 --format lvgl --no-compress --lv-include lvgl.h --lv-font-name at_serif_30_fb -o main/assets/skin_fonts/at_serif_30_fb.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef AT_SERIF_30_FB
#define AT_SERIF_30_FB 1
#endif

#if AT_SERIF_30_FB

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+00B5 "µ" */
    0x2f, 0xff, 0x90, 0x0, 0x0, 0xc, 0xff, 0xf0,
    0x2, 0xff, 0xf9, 0x0, 0x0, 0x0, 0xcf, 0xff,
    0x0, 0x2f, 0xff, 0x90, 0x0, 0x0, 0xc, 0xff,
    0xf0, 0x2, 0xff, 0xf9, 0x0, 0x0, 0x0, 0xcf,
    0xff, 0x0, 0x2f, 0xff, 0x90, 0x0, 0x0, 0xc,
    0xff, 0xf0, 0x2, 0xff, 0xf9, 0x0, 0x0, 0x0,
    0xcf, 0xff, 0x0, 0x2f, 0xff, 0x90, 0x0, 0x0,
    0xc, 0xff, 0xf0, 0x2, 0xff, 0xf9, 0x0, 0x0,
    0x0, 0xcf, 0xff, 0x0, 0x2f, 0xff, 0x90, 0x0,
    0x0, 0xc, 0xff, 0xf0, 0x2, 0xff, 0xf9, 0x0,
    0x0, 0x0, 0xcf, 0xff, 0x0, 0x2f, 0xff, 0xb0,
    0x0, 0x0, 0xe, 0xff, 0xf0, 0x2, 0xff, 0xff,
    0x10, 0x0, 0x3, 0xff, 0xff, 0x0, 0x2f, 0xff,
    0xfc, 0x20, 0x3, 0xdf, 0xff, 0xf2, 0x2, 0xff,
    0xff, 0xff, 0xee, 0xff, 0xef, 0xff, 0xfc, 0x2f,
    0xff, 0xbe, 0xff, 0xff, 0xe2, 0xef, 0xff, 0xe2,
    0xff, 0xf9, 0x2a, 0xff, 0xa2, 0x2, 0xae, 0xfe,
    0x2f, 0xff, 0x90, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x2, 0xff, 0xf9, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x2f, 0xff, 0x90, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x2, 0xff, 0xf9, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x2f, 0xff, 0x90, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x2, 0xff, 0xf9, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 288, .box_w = 17, .box_h = 22, .ofs_x = 1, .ofs_y = -6}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 181, .range_length = 1, .glyph_id_start = 1,
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
    .cmap_num = 1,
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
const lv_font_t at_serif_30_fb = {
#else
lv_font_t at_serif_30_fb = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 22,          /*The maximum line height required by the font*/
    .base_line = 6,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -4,
    .underline_thickness = 3,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if AT_SERIF_30_FB*/

