#include "ui/kit/ui_skin.h"

#include "assets/skin_fonts/skin_fonts.h"

static const ui_skin_t s_instrument = {
    .style = CLOCK_UI_STYLE_INSTRUMENT,
    .bg = LV_COLOR_MAKE(0x18, 0x14, 0x10),
    .well = LV_COLOR_MAKE(0x10, 0x0C, 0x08),
    .surface = LV_COLOR_MAKE(0x20, 0x1C, 0x18),
    .sheet = LV_COLOR_MAKE(0x20, 0x1C, 0x18),
    .raised = LV_COLOR_MAKE(0x30, 0x2C, 0x28),
    .line = LV_COLOR_MAKE(0x38, 0x34, 0x30),
    .line_strong = LV_COLOR_MAKE(0x48, 0x44, 0x40),
    .text = LV_COLOR_MAKE(0xE8, 0xE4, 0xD8),
    .text_2 = LV_COLOR_MAKE(0xA8, 0xA4, 0x98),
    .text_3 = LV_COLOR_MAKE(0x88, 0x84, 0x78),
    .label = LV_COLOR_MAKE(0x88, 0x84, 0x78),
    .value = LV_COLOR_MAKE(0xE8, 0xE4, 0xD8),
    .icon_fg = LV_COLOR_MAKE(0xE8, 0xE4, 0xD8),
    .accent = LV_COLOR_MAKE(0xF0, 0x58, 0x18),
    .accent_text = LV_COLOR_MAKE(0x18, 0x14, 0x10),
    .accent_soft = LV_COLOR_MAKE(0x38, 0x20, 0x10),
    .accent_deep = LV_COLOR_MAKE(0x78, 0x2C, 0x08),
    .danger = LV_COLOR_MAKE(0xF0, 0x58, 0x18),
    .danger_text = LV_COLOR_MAKE(0xF0, 0x58, 0x18),
    .danger_soft = LV_COLOR_MAKE(0x38, 0x20, 0x10),
    .night = LV_COLOR_MAKE(0xA0, 0x3C, 0x18),
    .tint_wifi = LV_COLOR_MAKE(0xF0, 0x58, 0x18),
    .tint_night = LV_COLOR_MAKE(0xF0, 0x58, 0x18),
    .tint_alarms = LV_COLOR_MAKE(0xF0, 0x58, 0x18),
    .tint_other = LV_COLOR_MAKE(0xF0, 0x58, 0x18),
    .tint_themes = LV_COLOR_MAKE(0xF0, 0x58, 0x18),
    .tint_brightness = LV_COLOR_MAKE(0xF0, 0x58, 0x18),
    .tint_sound = LV_COLOR_MAKE(0xF0, 0x58, 0x18),
    .sheet_dim_opa = LV_OPA_70,
    .font_display = &it_light_144,
    .font_time_l = &it_light_88,
    .font_time_m = &it_light_56,
    .font_title = &it_medium_34,
    .font_value = &it_medium_24,
    .font_heading = &it_regular_24,
    .font_body = &it_regular_22,
    .font_button = &it_medium_24,
    .font_label = &it_mono_medium_18,
    .font_caption = &it_mono_18,
    .label_caps = true,
    .button_caps = false,
    .label_letter_space = 3,
    .button_letter_space = 0,
    .content_x = 104,
    .content_w = 512,
    .page_top = 84,
    .nav_x = 164,
    .title_y = 95,
    .eyebrow_y = 57,
    .content_top = 168,
    .row_h = 80,
    .row_h_2 = 88,
    .radius_s = 10,
    .radius_m = 20,
    .radius_l = 20,
    .gap_xs = 8,
    .gap_s = 16,
    .gap_m = 24,
    .gap_l = 32,
    .gap_xl = 48,
    .button_h = 72,
    .round_button = 64,
    .switch_w = 88,
    .switch_h = 48,
    .icon = 28,
    .tap_min = 72,
};

static const ui_skin_t s_orbit = {
    .style = CLOCK_UI_STYLE_ORBIT,
    .bg = LV_COLOR_MAKE(0x00, 0x00, 0x00),
    .well = LV_COLOR_MAKE(0x38, 0x38, 0x38),
    .surface = LV_COLOR_MAKE(0x18, 0x1C, 0x18),
    .sheet = LV_COLOR_MAKE(0x18, 0x1C, 0x18),
    .raised = LV_COLOR_MAKE(0x28, 0x2C, 0x28),
    .line = LV_COLOR_MAKE(0x28, 0x2C, 0x28),
    .line_strong = LV_COLOR_MAKE(0x38, 0x38, 0x38),
    .text = LV_COLOR_MAKE(0xF8, 0xFC, 0xF8),
    .text_2 = LV_COLOR_MAKE(0xA8, 0xAC, 0xB0),
    .text_3 = LV_COLOR_MAKE(0x68, 0x6C, 0x70),
    .label = LV_COLOR_MAKE(0xA8, 0xAC, 0xB0),
    .value = LV_COLOR_MAKE(0xF8, 0xFC, 0xF8),
    .icon_fg = LV_COLOR_MAKE(0xF8, 0xFC, 0xF8),
    .accent = LV_COLOR_MAKE(0x08, 0x84, 0xF8),
    .accent_text = LV_COLOR_MAKE(0xF8, 0xFC, 0xF8),
    .accent_soft = LV_COLOR_MAKE(0x18, 0x34, 0x50),
    .accent_deep = LV_COLOR_MAKE(0x00, 0x5C, 0xB8),
    .danger = LV_COLOR_MAKE(0xF8, 0x44, 0x38),
    .danger_text = LV_COLOR_MAKE(0xF8, 0x44, 0x38),
    .danger_soft = LV_COLOR_MAKE(0x48, 0x24, 0x20),
    .night = LV_COLOR_MAKE(0x88, 0x84, 0xD8),
    .tint_wifi = LV_COLOR_MAKE(0x08, 0x84, 0xF8),
    .tint_night = LV_COLOR_MAKE(0x58, 0x5C, 0xE0),
    .tint_alarms = LV_COLOR_MAKE(0xF8, 0x9C, 0x08),
    .tint_other = LV_COLOR_MAKE(0x60, 0x64, 0x68),
    .tint_themes = LV_COLOR_MAKE(0xB8, 0x58, 0xF0),
    .tint_brightness = LV_COLOR_MAKE(0xF8, 0xD4, 0x08),
    .tint_sound = LV_COLOR_MAKE(0xF8, 0x34, 0x58),
    .sheet_dim_opa = LV_OPA_70,
    .font_display = &or_semibold_168,
    .font_time_l = &or_semibold_112,
    .font_time_m = &or_semibold_56,
    .font_title = &or_bold_40,
    .font_value = &or_medium_22,
    .font_heading = &or_semibold_26,
    .font_body = &or_regular_22,
    .font_button = &or_semibold_26,
    .font_label = &or_semibold_18,
    .font_caption = &or_semibold_18,
    .label_caps = false,
    .button_caps = false,
    .label_letter_space = 0,
    .button_letter_space = 0,
    .content_x = 110,
    .content_w = 500,
    .page_top = 24,
    .nav_x = 328,
    .title_y = 99,
    .eyebrow_y = 148,
    .content_top = 172,
    .row_h = 88,
    .row_h_2 = 104,
    .radius_s = 32,
    .radius_m = 44,
    .radius_l = 64,
    .gap_xs = 8,
    .gap_s = 12,
    .gap_m = 20,
    .gap_l = 28,
    .gap_xl = 40,
    .button_h = 64,
    .round_button = 64,
    .switch_w = 76,
    .switch_h = 44,
    .icon = 28,
    .tap_min = 72,
};
/* Liquid Glass: Orbit's metrics and fonts. The surface tokens are markers, not colours: the
 * glass pass (ui_glass.c) draws every surface painted with one as glass of that kind, over the
 * face behind it. They are colours nothing else uses. */
static const ui_skin_t s_glass = {
    .style = CLOCK_UI_STYLE_ORBIT,
    .glass = true,
    .bg = LV_COLOR_MAKE(0x00, 0x00, 0x08),
    .well = LV_COLOR_MAKE(0x08, 0x00, 0x08),
    .surface = LV_COLOR_MAKE(0x08, 0x08, 0x00),
    .sheet = LV_COLOR_MAKE(0x00, 0x08, 0x00),
    .raised = LV_COLOR_MAKE(0x08, 0x08, 0x08),
    .line = LV_COLOR_MAKE(0x40, 0x44, 0x48),
    .line_strong = LV_COLOR_MAKE(0x58, 0x5C, 0x60),
    .text = LV_COLOR_MAKE(0xF8, 0xFC, 0xF8),
    .text_2 = LV_COLOR_MAKE(0xC0, 0xC4, 0xC8),
    .text_3 = LV_COLOR_MAKE(0x88, 0x8C, 0x90),
    .label = LV_COLOR_MAKE(0xC0, 0xC4, 0xC8),
    .value = LV_COLOR_MAKE(0xF8, 0xFC, 0xF8),
    .icon_fg = LV_COLOR_MAKE(0xF8, 0xFC, 0xF8),
    .accent = LV_COLOR_MAKE(0x08, 0x84, 0xF8),
    .accent_text = LV_COLOR_MAKE(0xF8, 0xFC, 0xF8),
    .accent_soft = LV_COLOR_MAKE(0x10, 0x38, 0x60),
    .accent_deep = LV_COLOR_MAKE(0x00, 0x5C, 0xB8),
    .danger = LV_COLOR_MAKE(0xF8, 0x44, 0x38),
    .danger_text = LV_COLOR_MAKE(0xF8, 0x44, 0x38),
    .danger_soft = LV_COLOR_MAKE(0x48, 0x24, 0x20),
    .night = LV_COLOR_MAKE(0x88, 0x84, 0xD8),
    .tint_wifi = LV_COLOR_MAKE(0x08, 0x84, 0xF8),
    .tint_night = LV_COLOR_MAKE(0x58, 0x5C, 0xE0),
    .tint_alarms = LV_COLOR_MAKE(0xF8, 0x9C, 0x08),
    .tint_other = LV_COLOR_MAKE(0x60, 0x64, 0x68),
    .tint_themes = LV_COLOR_MAKE(0xB8, 0x58, 0xF0),
    .tint_brightness = LV_COLOR_MAKE(0xF8, 0xD4, 0x08),
    .tint_sound = LV_COLOR_MAKE(0xF8, 0x34, 0x58),
    .sheet_dim_opa = LV_OPA_TRANSP, /* the face stays in view around and through the glass */
    .font_display = &or_semibold_168,
    .font_time_l = &or_semibold_112,
    .font_time_m = &or_semibold_56,
    .font_title = &or_bold_40,
    .font_value = &or_medium_22,
    .font_heading = &or_semibold_26,
    .font_body = &or_regular_22,
    .font_button = &or_semibold_26,
    .font_label = &or_semibold_18,
    .font_caption = &or_semibold_18,
    .label_caps = false,
    .button_caps = false,
    .label_letter_space = 0,
    .button_letter_space = 0,
    .content_x = 110,
    .content_w = 500,
    .page_top = 24,
    .nav_x = 328,
    .title_y = 99,
    .eyebrow_y = 148,
    .content_top = 172,
    .row_h = 88,
    .row_h_2 = 104,
    .radius_s = 32,
    .radius_m = 44,
    .radius_l = 64,
    .gap_xs = 8,
    .gap_s = 12,
    .gap_m = 20,
    .gap_l = 28,
    .gap_xl = 40,
    .button_h = 64,
    .round_button = 64,
    .switch_w = 76,
    .switch_h = 44,
    .icon = 28,
    .tap_min = 72,
};

static const ui_skin_t s_atelier = {
    .style = CLOCK_UI_STYLE_ATELIER,
    .bg = LV_COLOR_MAKE(0x10, 0x0C, 0x18),
    .well = LV_COLOR_MAKE(0x18, 0x14, 0x20),
    .surface = LV_COLOR_MAKE(0x18, 0x14, 0x20),
    .sheet = LV_COLOR_MAKE(0x10, 0x0C, 0x18),
    .raised = LV_COLOR_MAKE(0x20, 0x1C, 0x28),
    .line = LV_COLOR_MAKE(0x28, 0x24, 0x30),
    .line_strong = LV_COLOR_MAKE(0x38, 0x30, 0x40),
    .text = LV_COLOR_MAKE(0xF0, 0xE8, 0xD8),
    .text_2 = LV_COLOR_MAKE(0xA8, 0x9C, 0x88),
    .text_3 = LV_COLOR_MAKE(0x68, 0x60, 0x58),
    .label = LV_COLOR_MAKE(0xB8, 0x98, 0x68),
    .value = LV_COLOR_MAKE(0xE8, 0xD8, 0xB8),
    .icon_fg = LV_COLOR_MAKE(0xE8, 0xD8, 0xB8),
    .accent = LV_COLOR_MAKE(0xD8, 0xB8, 0x80),
    .accent_text = LV_COLOR_MAKE(0x10, 0x0C, 0x18),
    .accent_soft = LV_COLOR_MAKE(0x38, 0x2C, 0x20),
    .accent_deep = LV_COLOR_MAKE(0x80, 0x68, 0x48),
    .danger = LV_COLOR_MAKE(0xB8, 0x30, 0x48),
    .danger_text = LV_COLOR_MAKE(0xE0, 0x70, 0x88),
    .danger_soft = LV_COLOR_MAKE(0x28, 0x0C, 0x10),
    .night = LV_COLOR_MAKE(0xA0, 0x68, 0x30),
    .tint_wifi = LV_COLOR_MAKE(0xD8, 0xB8, 0x80),
    .tint_night = LV_COLOR_MAKE(0xD8, 0xB8, 0x80),
    .tint_alarms = LV_COLOR_MAKE(0xD8, 0xB8, 0x80),
    .tint_other = LV_COLOR_MAKE(0xD8, 0xB8, 0x80),
    .tint_themes = LV_COLOR_MAKE(0xD8, 0xB8, 0x80),
    .tint_brightness = LV_COLOR_MAKE(0xD8, 0xB8, 0x80),
    .tint_sound = LV_COLOR_MAKE(0xD8, 0xB8, 0x80),
    .sheet_dim_opa = LV_OPA_70,
    .font_display = &at_serif_168,
    .font_time_l = &at_serif_104,
    .font_time_m = &at_serif_56,
    .font_title = &at_serif_40,
    .font_value = &at_serif_30,
    .font_heading = &at_sans_26,
    .font_body = &at_sans_22,
    .font_button = &at_sans_medium_20,
    .font_label = &at_sans_medium_18,
    .font_caption = &at_sans_22,
    .label_caps = true,
    .button_caps = true,
    .label_letter_space = 3,
    .button_letter_space = 2,
    .content_x = 144,
    .content_w = 432,
    .page_top = 88,
    .nav_x = 148,
    .title_y = 96,
    .eyebrow_y = 55,
    .content_top = 184,
    .row_h = 76,
    .row_h_2 = 100,
    .radius_s = 14,
    .radius_m = 28,
    .radius_l = 28,
    .gap_xs = 8,
    .gap_s = 16,
    .gap_m = 24,
    .gap_l = 32,
    .gap_xl = 48,
    .button_h = 64,
    .round_button = 64,
    .switch_w = 72,
    .switch_h = 40,
    .icon = 28,
    .tap_min = 72,
};

static const ui_skin_t *s_skin = &s_orbit;

const ui_skin_t *ui_skin(void)
{
    return s_skin;
}

clock_ui_style_t ui_skin_style(void)
{
    return s_skin->style;
}

bool ui_skin_is_glass(void)
{
    return s_skin->glass;
}

void ui_skin_set_style(clock_ui_style_t style)
{
    switch (style) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        s_skin = &s_instrument;
        break;
    case CLOCK_UI_STYLE_ATELIER:
        s_skin = &s_atelier;
        break;
    case CLOCK_UI_STYLE_GLASS:
        s_skin = &s_glass;
        break;
    case CLOCK_UI_STYLE_ORBIT:
    default:
        s_skin = &s_orbit;
        break;
    }
}

const char *ui_skin_style_name(clock_ui_style_t style)
{
    /* Style names are proper names and stay untranslated. */
    switch (style) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        return "Instrument";
    case CLOCK_UI_STYLE_ATELIER:
        return "Atelier";
    case CLOCK_UI_STYLE_GLASS:
        return "Liquid Glass";
    case CLOCK_UI_STYLE_ORBIT:
    default:
        return "Orbit";
    }
}
