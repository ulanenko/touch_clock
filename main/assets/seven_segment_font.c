#include "assets/seven_segment_font.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define SEG_FONT_LINE_HEIGHT 124
#define SEG_FONT_DIGIT_W 76
#define SEG_FONT_DIGIT_H 124
#define SEG_FONT_DIGIT_ADV 82
#define SEG_FONT_COLON_W 26
#define SEG_FONT_COLON_ADV 30
#define SEG_FONT_DASH_W 48
#define SEG_FONT_DASH_ADV 56
#define SEG_FONT_SPACE_ADV SEG_FONT_DIGIT_ADV
#define SEG_FONT_THICK 13
#define SEG_FONT_DOT 13

typedef struct {
    uint32_t codepoint;
    uint16_t adv_w;
    uint16_t box_w;
    uint16_t box_h;
    int16_t ofs_x;
    int16_t ofs_y;
    uint8_t *bitmap;
    bool generated;
} seven_segment_glyph_t;

static const uint8_t s_digit_masks[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

static uint8_t s_digit_bitmap[10][SEG_FONT_DIGIT_W * SEG_FONT_DIGIT_H];
static uint8_t s_colon_bitmap[SEG_FONT_COLON_W * SEG_FONT_DIGIT_H];
static uint8_t s_dash_bitmap[SEG_FONT_DASH_W * SEG_FONT_DIGIT_H];

static seven_segment_glyph_t s_glyphs[] = {
    {.codepoint = ' ', .adv_w = SEG_FONT_SPACE_ADV, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0, .bitmap = NULL, .generated = true},
    {.codepoint = '-', .adv_w = SEG_FONT_DASH_ADV, .box_w = SEG_FONT_DASH_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_dash_bitmap, .generated = false},
    {.codepoint = ':', .adv_w = SEG_FONT_COLON_ADV, .box_w = SEG_FONT_COLON_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_colon_bitmap, .generated = false},
    {.codepoint = '0', .adv_w = SEG_FONT_DIGIT_ADV, .box_w = SEG_FONT_DIGIT_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_digit_bitmap[0], .generated = false},
    {.codepoint = '1', .adv_w = SEG_FONT_DIGIT_ADV, .box_w = SEG_FONT_DIGIT_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_digit_bitmap[1], .generated = false},
    {.codepoint = '2', .adv_w = SEG_FONT_DIGIT_ADV, .box_w = SEG_FONT_DIGIT_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_digit_bitmap[2], .generated = false},
    {.codepoint = '3', .adv_w = SEG_FONT_DIGIT_ADV, .box_w = SEG_FONT_DIGIT_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_digit_bitmap[3], .generated = false},
    {.codepoint = '4', .adv_w = SEG_FONT_DIGIT_ADV, .box_w = SEG_FONT_DIGIT_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_digit_bitmap[4], .generated = false},
    {.codepoint = '5', .adv_w = SEG_FONT_DIGIT_ADV, .box_w = SEG_FONT_DIGIT_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_digit_bitmap[5], .generated = false},
    {.codepoint = '6', .adv_w = SEG_FONT_DIGIT_ADV, .box_w = SEG_FONT_DIGIT_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_digit_bitmap[6], .generated = false},
    {.codepoint = '7', .adv_w = SEG_FONT_DIGIT_ADV, .box_w = SEG_FONT_DIGIT_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_digit_bitmap[7], .generated = false},
    {.codepoint = '8', .adv_w = SEG_FONT_DIGIT_ADV, .box_w = SEG_FONT_DIGIT_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_digit_bitmap[8], .generated = false},
    {.codepoint = '9', .adv_w = SEG_FONT_DIGIT_ADV, .box_w = SEG_FONT_DIGIT_W, .box_h = SEG_FONT_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_digit_bitmap[9], .generated = false},
};

static void set_px(uint8_t *bitmap, uint16_t width, uint16_t height, int x, int y, uint8_t value)
{
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    bitmap[y * width + x] = value;
}

static void fill_hseg(uint8_t *bitmap, uint16_t width, uint16_t height, int x, int y, int seg_w, int seg_h)
{
    int slope = seg_h / 2;

    for (int row = 0; row < seg_h; ++row) {
        int inset = slope - row;
        int x1;
        int x2;

        if (inset < 0) {
            inset = -inset;
        }
        x1 = x + inset;
        x2 = x + seg_w - inset - 1;
        for (int col = x1; col <= x2; ++col) {
            set_px(bitmap, width, height, col, y + row, 0xFF);
        }
    }
}

static void fill_vseg(uint8_t *bitmap, uint16_t width, uint16_t height, int x, int y, int seg_w, int seg_h)
{
    int slope = seg_w / 2;

    for (int row = 0; row < seg_h; ++row) {
        int inset = slope - row;
        int y_rel = row;

        if (y_rel >= seg_h - slope) {
            inset = y_rel - (seg_h - slope) + 1;
        } else if (inset < 0) {
            inset = 0;
        }

        for (int col = inset; col < seg_w - inset; ++col) {
            set_px(bitmap, width, height, x + col, y + row, 0xFF);
        }
    }
}

static void draw_digit_segments(uint8_t *bitmap, uint8_t mask)
{
    const int margin_x = 6;
    const int margin_top = 2;
    const int inner_w = SEG_FONT_DIGIT_W - margin_x * 2;
    const int middle_y = (SEG_FONT_DIGIT_H - SEG_FONT_THICK) / 2;
    const int bottom_y = SEG_FONT_DIGIT_H - SEG_FONT_THICK - margin_top;
    const int upper_y = margin_top + SEG_FONT_THICK / 2 + 5;
    const int lower_y = middle_y + SEG_FONT_THICK / 2 + 5;
    const int vert_h = middle_y - upper_y - 2;
    const int left_x = 0;
    const int right_x = SEG_FONT_DIGIT_W - SEG_FONT_THICK;

    memset(bitmap, 0, SEG_FONT_DIGIT_W * SEG_FONT_DIGIT_H);

    if ((mask & 0x01U) != 0) {
        fill_hseg(bitmap, SEG_FONT_DIGIT_W, SEG_FONT_DIGIT_H, margin_x, margin_top, inner_w, SEG_FONT_THICK);
    }
    if ((mask & 0x02U) != 0) {
        fill_vseg(bitmap, SEG_FONT_DIGIT_W, SEG_FONT_DIGIT_H, right_x, upper_y, SEG_FONT_THICK, vert_h);
    }
    if ((mask & 0x04U) != 0) {
        fill_vseg(bitmap, SEG_FONT_DIGIT_W, SEG_FONT_DIGIT_H, right_x, lower_y, SEG_FONT_THICK, vert_h);
    }
    if ((mask & 0x08U) != 0) {
        fill_hseg(bitmap, SEG_FONT_DIGIT_W, SEG_FONT_DIGIT_H, margin_x, bottom_y, inner_w, SEG_FONT_THICK);
    }
    if ((mask & 0x10U) != 0) {
        fill_vseg(bitmap, SEG_FONT_DIGIT_W, SEG_FONT_DIGIT_H, left_x, lower_y, SEG_FONT_THICK, vert_h);
    }
    if ((mask & 0x20U) != 0) {
        fill_vseg(bitmap, SEG_FONT_DIGIT_W, SEG_FONT_DIGIT_H, left_x, upper_y, SEG_FONT_THICK, vert_h);
    }
    if ((mask & 0x40U) != 0) {
        fill_hseg(bitmap, SEG_FONT_DIGIT_W, SEG_FONT_DIGIT_H, margin_x, middle_y, inner_w, SEG_FONT_THICK);
    }
}

static void draw_colon(uint8_t *bitmap)
{
    const int dot_x = (SEG_FONT_COLON_W - SEG_FONT_DOT) / 2;
    const int top_y = SEG_FONT_DIGIT_H / 2 - 24;
    const int bottom_y = SEG_FONT_DIGIT_H / 2 + 12;

    memset(bitmap, 0, SEG_FONT_COLON_W * SEG_FONT_DIGIT_H);
    for (int y = 0; y < SEG_FONT_DOT; ++y) {
        for (int x = 0; x < SEG_FONT_DOT; ++x) {
            set_px(bitmap, SEG_FONT_COLON_W, SEG_FONT_DIGIT_H, dot_x + x, top_y + y, 0xFF);
            set_px(bitmap, SEG_FONT_COLON_W, SEG_FONT_DIGIT_H, dot_x + x, bottom_y + y, 0xFF);
        }
    }
}

static void draw_dash(uint8_t *bitmap)
{
    memset(bitmap, 0, SEG_FONT_DASH_W * SEG_FONT_DIGIT_H);
    fill_hseg(bitmap,
              SEG_FONT_DASH_W,
              SEG_FONT_DIGIT_H,
              0,
              (SEG_FONT_DIGIT_H - SEG_FONT_THICK) / 2,
              SEG_FONT_DASH_W,
              SEG_FONT_THICK);
}

static seven_segment_glyph_t *find_glyph(uint32_t letter)
{
    for (uint32_t i = 0; i < (sizeof(s_glyphs) / sizeof(s_glyphs[0])); ++i) {
        if (s_glyphs[i].codepoint == letter) {
            return &s_glyphs[i];
        }
    }
    return NULL;
}

static void ensure_glyph_generated(seven_segment_glyph_t *glyph)
{
    if (glyph == NULL || glyph->generated) {
        return;
    }

    if (glyph->codepoint >= '0' && glyph->codepoint <= '9') {
        draw_digit_segments(glyph->bitmap, s_digit_masks[glyph->codepoint - '0']);
    } else if (glyph->codepoint == ':') {
        draw_colon(glyph->bitmap);
    } else if (glyph->codepoint == '-') {
        draw_dash(glyph->bitmap);
    }

    glyph->generated = true;
}

static bool seven_segment_get_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc_out, uint32_t letter, uint32_t letter_next)
{
    seven_segment_glyph_t *glyph = find_glyph(letter);

    LV_UNUSED(letter_next);

    if (glyph == NULL) {
        return false;
    }

    dsc_out->resolved_font = font;
    dsc_out->adv_w = glyph->adv_w;
    dsc_out->box_w = glyph->box_w;
    dsc_out->box_h = glyph->box_h;
    dsc_out->ofs_x = glyph->ofs_x;
    dsc_out->ofs_y = glyph->ofs_y;
    dsc_out->stride = glyph->box_w;
    dsc_out->format = LV_FONT_GLYPH_FORMAT_A8;
    dsc_out->is_placeholder = 0;
    dsc_out->req_raw_bitmap = 0;
    dsc_out->outline_stroke_width = 0;
    dsc_out->gid.index = letter;
    dsc_out->entry = NULL;
    return true;
}

static const void *seven_segment_get_glyph_bitmap(lv_font_glyph_dsc_t *g_dsc, lv_draw_buf_t *draw_buf)
{
    seven_segment_glyph_t *glyph = find_glyph(g_dsc->gid.index);

    LV_UNUSED(draw_buf);

    if (glyph == NULL || glyph->bitmap == NULL) {
        return NULL;
    }

    ensure_glyph_generated(glyph);
    return glyph->bitmap;
}

const lv_font_t seven_segment_font_112 = {
    .get_glyph_dsc = seven_segment_get_glyph_dsc,
    .get_glyph_bitmap = seven_segment_get_glyph_bitmap,
    .release_glyph = NULL,
    .line_height = SEG_FONT_LINE_HEIGHT,
    .base_line = 0,
    .subpx = LV_FONT_SUBPX_NONE,
    .kerning = LV_FONT_KERNING_NONE,
    .static_bitmap = 1,
    .underline_position = -6,
    .underline_thickness = 2,
    .dsc = NULL,
    .fallback = NULL,
    .user_data = NULL,
};

#define SEG_FONT_SMALL_LINE_HEIGHT 56
#define SEG_FONT_SMALL_DIGIT_W 34
#define SEG_FONT_SMALL_DIGIT_H 56
#define SEG_FONT_SMALL_DIGIT_ADV 38
#define SEG_FONT_SMALL_COLON_W 14
#define SEG_FONT_SMALL_COLON_ADV 18
#define SEG_FONT_SMALL_DASH_W 22
#define SEG_FONT_SMALL_DASH_ADV 28
#define SEG_FONT_SMALL_SPACE_ADV SEG_FONT_SMALL_DIGIT_ADV
#define SEG_FONT_SMALL_THICK 6
#define SEG_FONT_SMALL_DOT 6

static uint8_t s_small_digit_bitmap[10][SEG_FONT_SMALL_DIGIT_W * SEG_FONT_SMALL_DIGIT_H];
static uint8_t s_small_colon_bitmap[SEG_FONT_SMALL_COLON_W * SEG_FONT_SMALL_DIGIT_H];
static uint8_t s_small_dash_bitmap[SEG_FONT_SMALL_DASH_W * SEG_FONT_SMALL_DIGIT_H];

static seven_segment_glyph_t s_small_glyphs[] = {
    {.codepoint = ' ', .adv_w = SEG_FONT_SMALL_SPACE_ADV, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0, .bitmap = NULL, .generated = true},
    {.codepoint = '-', .adv_w = SEG_FONT_SMALL_DASH_ADV, .box_w = SEG_FONT_SMALL_DASH_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_dash_bitmap, .generated = false},
    {.codepoint = ':', .adv_w = SEG_FONT_SMALL_COLON_ADV, .box_w = SEG_FONT_SMALL_COLON_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_colon_bitmap, .generated = false},
    {.codepoint = '0', .adv_w = SEG_FONT_SMALL_DIGIT_ADV, .box_w = SEG_FONT_SMALL_DIGIT_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_digit_bitmap[0], .generated = false},
    {.codepoint = '1', .adv_w = SEG_FONT_SMALL_DIGIT_ADV, .box_w = SEG_FONT_SMALL_DIGIT_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_digit_bitmap[1], .generated = false},
    {.codepoint = '2', .adv_w = SEG_FONT_SMALL_DIGIT_ADV, .box_w = SEG_FONT_SMALL_DIGIT_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_digit_bitmap[2], .generated = false},
    {.codepoint = '3', .adv_w = SEG_FONT_SMALL_DIGIT_ADV, .box_w = SEG_FONT_SMALL_DIGIT_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_digit_bitmap[3], .generated = false},
    {.codepoint = '4', .adv_w = SEG_FONT_SMALL_DIGIT_ADV, .box_w = SEG_FONT_SMALL_DIGIT_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_digit_bitmap[4], .generated = false},
    {.codepoint = '5', .adv_w = SEG_FONT_SMALL_DIGIT_ADV, .box_w = SEG_FONT_SMALL_DIGIT_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_digit_bitmap[5], .generated = false},
    {.codepoint = '6', .adv_w = SEG_FONT_SMALL_DIGIT_ADV, .box_w = SEG_FONT_SMALL_DIGIT_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_digit_bitmap[6], .generated = false},
    {.codepoint = '7', .adv_w = SEG_FONT_SMALL_DIGIT_ADV, .box_w = SEG_FONT_SMALL_DIGIT_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_digit_bitmap[7], .generated = false},
    {.codepoint = '8', .adv_w = SEG_FONT_SMALL_DIGIT_ADV, .box_w = SEG_FONT_SMALL_DIGIT_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_digit_bitmap[8], .generated = false},
    {.codepoint = '9', .adv_w = SEG_FONT_SMALL_DIGIT_ADV, .box_w = SEG_FONT_SMALL_DIGIT_W, .box_h = SEG_FONT_SMALL_DIGIT_H, .ofs_x = 0, .ofs_y = 0, .bitmap = s_small_digit_bitmap[9], .generated = false},
};

static void draw_small_digit_segments(uint8_t *bitmap, uint8_t mask)
{
    const int margin_x = 3;
    const int margin_top = 1;
    const int inner_w = SEG_FONT_SMALL_DIGIT_W - margin_x * 2;
    const int middle_y = (SEG_FONT_SMALL_DIGIT_H - SEG_FONT_SMALL_THICK) / 2;
    const int bottom_y = SEG_FONT_SMALL_DIGIT_H - SEG_FONT_SMALL_THICK - margin_top;
    const int upper_y = margin_top + SEG_FONT_SMALL_THICK / 2 + 2;
    const int lower_y = middle_y + SEG_FONT_SMALL_THICK / 2 + 2;
    const int vert_h = middle_y - upper_y - 1;
    const int left_x = 0;
    const int right_x = SEG_FONT_SMALL_DIGIT_W - SEG_FONT_SMALL_THICK;

    memset(bitmap, 0, SEG_FONT_SMALL_DIGIT_W * SEG_FONT_SMALL_DIGIT_H);

    if ((mask & 0x01U) != 0) {
        fill_hseg(bitmap, SEG_FONT_SMALL_DIGIT_W, SEG_FONT_SMALL_DIGIT_H, margin_x, margin_top, inner_w, SEG_FONT_SMALL_THICK);
    }
    if ((mask & 0x02U) != 0) {
        fill_vseg(bitmap, SEG_FONT_SMALL_DIGIT_W, SEG_FONT_SMALL_DIGIT_H, right_x, upper_y, SEG_FONT_SMALL_THICK, vert_h);
    }
    if ((mask & 0x04U) != 0) {
        fill_vseg(bitmap, SEG_FONT_SMALL_DIGIT_W, SEG_FONT_SMALL_DIGIT_H, right_x, lower_y, SEG_FONT_SMALL_THICK, vert_h);
    }
    if ((mask & 0x08U) != 0) {
        fill_hseg(bitmap, SEG_FONT_SMALL_DIGIT_W, SEG_FONT_SMALL_DIGIT_H, margin_x, bottom_y, inner_w, SEG_FONT_SMALL_THICK);
    }
    if ((mask & 0x10U) != 0) {
        fill_vseg(bitmap, SEG_FONT_SMALL_DIGIT_W, SEG_FONT_SMALL_DIGIT_H, left_x, lower_y, SEG_FONT_SMALL_THICK, vert_h);
    }
    if ((mask & 0x20U) != 0) {
        fill_vseg(bitmap, SEG_FONT_SMALL_DIGIT_W, SEG_FONT_SMALL_DIGIT_H, left_x, upper_y, SEG_FONT_SMALL_THICK, vert_h);
    }
    if ((mask & 0x40U) != 0) {
        fill_hseg(bitmap, SEG_FONT_SMALL_DIGIT_W, SEG_FONT_SMALL_DIGIT_H, margin_x, middle_y, inner_w, SEG_FONT_SMALL_THICK);
    }
}

static void draw_small_colon(uint8_t *bitmap)
{
    const int dot_x = (SEG_FONT_SMALL_COLON_W - SEG_FONT_SMALL_DOT) / 2;
    const int top_y = SEG_FONT_SMALL_DIGIT_H / 2 - 12;
    const int bottom_y = SEG_FONT_SMALL_DIGIT_H / 2 + 6;

    memset(bitmap, 0, SEG_FONT_SMALL_COLON_W * SEG_FONT_SMALL_DIGIT_H);
    for (int y = 0; y < SEG_FONT_SMALL_DOT; ++y) {
        for (int x = 0; x < SEG_FONT_SMALL_DOT; ++x) {
            set_px(bitmap, SEG_FONT_SMALL_COLON_W, SEG_FONT_SMALL_DIGIT_H, dot_x + x, top_y + y, 0xFF);
            set_px(bitmap, SEG_FONT_SMALL_COLON_W, SEG_FONT_SMALL_DIGIT_H, dot_x + x, bottom_y + y, 0xFF);
        }
    }
}

static void draw_small_dash(uint8_t *bitmap)
{
    memset(bitmap, 0, SEG_FONT_SMALL_DASH_W * SEG_FONT_SMALL_DIGIT_H);
    fill_hseg(bitmap,
              SEG_FONT_SMALL_DASH_W,
              SEG_FONT_SMALL_DIGIT_H,
              0,
              (SEG_FONT_SMALL_DIGIT_H - SEG_FONT_SMALL_THICK) / 2,
              SEG_FONT_SMALL_DASH_W,
              SEG_FONT_SMALL_THICK);
}

static seven_segment_glyph_t *find_small_glyph(uint32_t letter)
{
    for (uint32_t i = 0; i < (sizeof(s_small_glyphs) / sizeof(s_small_glyphs[0])); ++i) {
        if (s_small_glyphs[i].codepoint == letter) {
            return &s_small_glyphs[i];
        }
    }
    return NULL;
}

static void ensure_small_glyph_generated(seven_segment_glyph_t *glyph)
{
    if (glyph == NULL || glyph->generated) {
        return;
    }

    if (glyph->codepoint >= '0' && glyph->codepoint <= '9') {
        draw_small_digit_segments(glyph->bitmap, s_digit_masks[glyph->codepoint - '0']);
    } else if (glyph->codepoint == ':') {
        draw_small_colon(glyph->bitmap);
    } else if (glyph->codepoint == '-') {
        draw_small_dash(glyph->bitmap);
    }

    glyph->generated = true;
}

static bool seven_segment_small_get_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc_out, uint32_t letter, uint32_t letter_next)
{
    seven_segment_glyph_t *glyph = find_small_glyph(letter);

    LV_UNUSED(letter_next);

    if (glyph == NULL) {
        return false;
    }

    dsc_out->resolved_font = font;
    dsc_out->adv_w = glyph->adv_w;
    dsc_out->box_w = glyph->box_w;
    dsc_out->box_h = glyph->box_h;
    dsc_out->ofs_x = glyph->ofs_x;
    dsc_out->ofs_y = glyph->ofs_y;
    dsc_out->stride = glyph->box_w;
    dsc_out->format = LV_FONT_GLYPH_FORMAT_A8;
    dsc_out->is_placeholder = 0;
    dsc_out->req_raw_bitmap = 0;
    dsc_out->outline_stroke_width = 0;
    dsc_out->gid.index = letter;
    dsc_out->entry = NULL;
    return true;
}

static const void *seven_segment_small_get_glyph_bitmap(lv_font_glyph_dsc_t *g_dsc, lv_draw_buf_t *draw_buf)
{
    seven_segment_glyph_t *glyph = find_small_glyph(g_dsc->gid.index);

    LV_UNUSED(draw_buf);

    if (glyph == NULL || glyph->bitmap == NULL) {
        return NULL;
    }

    ensure_small_glyph_generated(glyph);
    return glyph->bitmap;
}

const lv_font_t seven_segment_font_56 = {
    .get_glyph_dsc = seven_segment_small_get_glyph_dsc,
    .get_glyph_bitmap = seven_segment_small_get_glyph_bitmap,
    .release_glyph = NULL,
    .line_height = SEG_FONT_SMALL_LINE_HEIGHT,
    .base_line = 0,
    .subpx = LV_FONT_SUBPX_NONE,
    .kerning = LV_FONT_KERNING_NONE,
    .static_bitmap = 1,
    .underline_position = -3,
    .underline_thickness = 1,
    .dsc = NULL,
    .fallback = NULL,
    .user_data = NULL,
};
