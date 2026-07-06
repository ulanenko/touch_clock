#include "ui/clock_ui_private.h"

static const lv_font_t *ui_l10n_default_font_for_label(lv_obj_t *label)
{
    const lv_font_t *font;

    if (label == NULL) {
        return &ui_i18n_24;
    }

    font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    if (font == &ui_i18n_20) {
        return &ui_i18n_20;
    }
    if (font == &ui_i18n_24) {
        return &ui_i18n_24;
    }
    if (font == &lv_font_montserrat_20 || font == LV_FONT_DEFAULT) {
        return &ui_i18n_20;
    }

    return &ui_i18n_24;
}

void ui_l10n_reset(clock_ui_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->l10n_binding_count = 0;
    ctx->cached_language = (clock_language_t)CLOCK_LANGUAGE_COUNT;
}

const char *ui_l10n_text(const clock_ui_context_t *ctx, l10n_text_id_t text_id)
{
    clock_language_t language = CLOCK_LANGUAGE_ENGLISH;

    if (ctx != NULL && ctx->settings != NULL) {
        language = (clock_language_t)ctx->settings->language;
    }

    return l10n_text(language, text_id);
}

void ui_l10n_bind_label_with_font(clock_ui_context_t *ctx,
                                  lv_obj_t *label,
                                  l10n_text_id_t text_id,
                                  const lv_font_t *font)
{
    if (ctx == NULL || label == NULL || ctx->l10n_binding_count >= UI_L10N_BINDING_MAX) {
        return;
    }

    lv_obj_set_style_text_font(label, font != NULL ? font : ui_l10n_default_font_for_label(label), 0);
    ctx->l10n_bindings[ctx->l10n_binding_count].label = label;
    ctx->l10n_bindings[ctx->l10n_binding_count].text_id = text_id;
    ctx->l10n_binding_count++;
    lv_label_set_text(label, ui_l10n_text(ctx, text_id));
}

void ui_l10n_bind_label(clock_ui_context_t *ctx, lv_obj_t *label, l10n_text_id_t text_id)
{
    ui_l10n_bind_label_with_font(ctx, label, text_id, NULL);
}

void ui_l10n_bind_button_label(clock_ui_context_t *ctx, lv_obj_t *button, l10n_text_id_t text_id)
{
    if (button == NULL) {
        return;
    }

    ui_l10n_bind_label(ctx, lv_obj_get_child(button, 0), text_id);
}

void ui_l10n_apply(clock_ui_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    for (uint16_t i = 0; i < ctx->l10n_binding_count; ++i) {
        if (ctx->l10n_bindings[i].label != NULL) {
            lv_label_set_text(ctx->l10n_bindings[i].label,
                              ui_l10n_text(ctx, ctx->l10n_bindings[i].text_id));
        }
    }
}
