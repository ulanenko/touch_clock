#include <string.h>

#include "ui/kit/ui_kit_internal.h"

#define KIT_TEXT_MAX 256

/* --- UTF-8 upper case ------------------------------------------------------------------ */

static uint32_t upper_cp(uint32_t c)
{
    if (c >= 'a' && c <= 'z') {
        return c - 0x20;
    }
    if (c < 0xE0) {
        return c; /* ASCII rest, Latin-1 symbols and capitals (µ stays: no Greek in the fonts) */
    }
    if ((c >= 0xE0 && c <= 0xFE && c != 0xF7)) {
        return c - 0x20;
    }
    if (c == 0xFF) {
        return 0x178;
    }
    if (c >= 0x100 && c <= 0x17F) {
        if (c == 0x131) {
            return 'I'; /* dotless i */
        }
        if (c == 0x17F) {
            return 'S'; /* long s */
        }
        if (c == 0x138 || c == 0x149) {
            return c; /* kra, n preceded by apostrophe: no capital */
        }
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) {
            return (c & 1) ? c : c - 1; /* odd code points are capitals here */
        }
        return (c & 1) ? c - 1 : c;
    }
    if (c >= 0x430 && c <= 0x44F) {
        return c - 0x20;
    }
    if (c >= 0x450 && c <= 0x45F) {
        return c - 0x50;
    }
    if (c >= 0x460 && c <= 0x4FF && (c & 1) && !(c >= 0x482 && c <= 0x489)) {
        return c - 1; /* Cyrillic extended pairs (ґ -> Ґ) */
    }
    return c;
}

static size_t utf8_decode(const char *s, uint32_t *cp)
{
    const uint8_t *u = (const uint8_t *)s;

    if (u[0] < 0x80) {
        *cp = u[0];
        return 1;
    }
    if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(u[0] & 0x1F) << 6) | (u[1] & 0x3F);
        return 2;
    }
    if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(u[0] & 0x0F) << 12) | ((uint32_t)(u[1] & 0x3F) << 6) | (u[2] & 0x3F);
        return 3;
    }
    if ((u[0] & 0xF8) == 0xF0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80) {
        *cp = ((uint32_t)(u[0] & 0x07) << 18) | ((uint32_t)(u[1] & 0x3F) << 12) |
              ((uint32_t)(u[2] & 0x3F) << 6) | (u[3] & 0x3F);
        return 4;
    }
    *cp = u[0]; /* invalid byte: copy it through */
    return 1;
}

static size_t utf8_encode(uint32_t cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

void kit_text_upper(char *dst, size_t dst_size, const char *src)
{
    size_t o = 0;

    if (dst == NULL || dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (*src != '\0') {
        uint32_t cp;
        char buf[4];
        size_t n = utf8_decode(src, &cp);
        size_t m;

        src += n;
        m = utf8_encode(upper_cp(cp), buf);
        if (o + m >= dst_size) {
            break;
        }
        memcpy(dst + o, buf, m);
        o += m;
    }
    dst[o] = '\0';
}

/* --- Roles ----------------------------------------------------------------------------- */

const lv_font_t *kit_font(kit_text_role_t role)
{
    const ui_skin_t *sk = ui_skin();

    switch (role) {
    case KIT_TEXT_TITLE:
        return sk->font_title;
    case KIT_TEXT_VALUE:
        return sk->font_value;
    case KIT_TEXT_HEADING:
        return sk->font_heading;
    case KIT_TEXT_BUTTON:
        return sk->font_button;
    case KIT_TEXT_LABEL:
        return sk->font_label;
    case KIT_TEXT_CAPTION:
        return sk->font_caption;
    case KIT_TEXT_TIME_M:
        return sk->font_time_m;
    case KIT_TEXT_TIME_L:
        return sk->font_time_l;
    case KIT_TEXT_DISPLAY:
        return sk->font_display;
    case KIT_TEXT_BODY:
    default:
        return sk->font_body;
    }
}

lv_color_t kit_role_color(kit_text_role_t role)
{
    const ui_skin_t *sk = ui_skin();

    switch (role) {
    case KIT_TEXT_VALUE:
        return sk->value;
    case KIT_TEXT_BODY:
        return sk->text_2;
    case KIT_TEXT_LABEL:
        return sk->label;
    case KIT_TEXT_CAPTION:
        return sk->text_3;
    case KIT_TEXT_BUTTON:
        return ui_skin_is_atelier() ? sk->value : sk->text;
    default:
        return sk->text;
    }
}

static bool role_caps(kit_text_role_t role)
{
    const ui_skin_t *sk = ui_skin();

    return (role == KIT_TEXT_LABEL && sk->label_caps) || (role == KIT_TEXT_BUTTON && sk->button_caps);
}

lv_obj_t *kit_label_raw(lv_obj_t *parent, kit_text_role_t role, const char *text)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, kit_font(role), 0);
    lv_obj_set_style_text_color(label, kit_role_color(role), 0);
    if (role == KIT_TEXT_LABEL && sk->label_letter_space != 0) {
        lv_obj_set_style_text_letter_space(label, sk->label_letter_space, 0);
    } else if (role == KIT_TEXT_BUTTON && sk->button_letter_space != 0) {
        lv_obj_set_style_text_letter_space(label, sk->button_letter_space, 0);
    }
    if (role_caps(role)) {
        lv_obj_add_flag(label, KIT_FLAG_CAPS);
    }
    kit_label_set_text(label, text != NULL ? text : "");
    return label;
}

lv_obj_t *kit_label(lv_obj_t *parent, kit_text_role_t role, const char *text)
{
    return kit_label_raw(parent, role, text);
}

void kit_label_set_text(lv_obj_t *label, const char *text)
{
    if (label == NULL) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    if (lv_obj_has_flag(label, KIT_FLAG_CAPS)) {
        char buf[KIT_TEXT_MAX];

        kit_text_upper(buf, sizeof(buf), text);
        lv_label_set_text(label, buf);
    } else {
        lv_label_set_text(label, text);
    }
}

void kit_label_set_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *cur;

    if (label == NULL) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    cur = lv_label_get_text(label);
    if (lv_obj_has_flag(label, KIT_FLAG_CAPS)) {
        char buf[KIT_TEXT_MAX];

        kit_text_upper(buf, sizeof(buf), text);
        if (cur == NULL || strcmp(cur, buf) != 0) {
            lv_label_set_text(label, buf);
        }
    } else if (cur == NULL || strcmp(cur, text) != 0) {
        lv_label_set_text(label, text);
    }
}
