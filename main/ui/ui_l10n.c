#include "ui/clock_ui_private.h"
#include "ui/kit/ui_kit.h"

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
    ctx->l10n_bindings[ctx->l10n_binding_count] = (ui_l10n_binding_t) {.label = label, .text_id = text_id};
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
        const ui_l10n_binding_t *binding = &ctx->l10n_bindings[i];
        const char *text;

        if (binding->label == NULL) {
            continue;
        }
        text = binding->str != NULL ? ui_l10n_str(ctx, binding->str) : ui_l10n_text(ctx, binding->text_id);
        if (binding->kit) {
            kit_label_set_text(binding->label, text);
        } else {
            lv_label_set_text(binding->label, text);
        }
    }
}

const char *ui_l10n_str(const clock_ui_context_t *ctx, const ui_l10n_str_t *str)
{
    clock_language_t language = CLOCK_LANGUAGE_ENGLISH;

    if (ctx != NULL && ctx->settings != NULL && ctx->settings->language < CLOCK_LANGUAGE_COUNT) {
        language = (clock_language_t)ctx->settings->language;
    }
    if (str->text[language] != NULL && str->text[language][0] != '\0') {
        return str->text[language];
    }
    return str->text[CLOCK_LANGUAGE_ENGLISH];
}

static void bind_kit(clock_ui_context_t *ctx, lv_obj_t *label, l10n_text_id_t text_id, const ui_l10n_str_t *str)
{
    if (ctx == NULL || label == NULL || ctx->l10n_binding_count >= UI_L10N_BINDING_MAX) {
        return;
    }

    ctx->l10n_bindings[ctx->l10n_binding_count] = (ui_l10n_binding_t) {
        .label = label,
        .text_id = text_id,
        .str = str,
        .kit = true,
    };
    ctx->l10n_binding_count++;
    kit_label_set_text(label, str != NULL ? ui_l10n_str(ctx, str) : ui_l10n_text(ctx, text_id));
}

void ui_l10n_bind_kit(clock_ui_context_t *ctx, lv_obj_t *label, l10n_text_id_t text_id)
{
    bind_kit(ctx, label, text_id, NULL);
}

void ui_l10n_bind_kit_str(clock_ui_context_t *ctx, lv_obj_t *label, const ui_l10n_str_t *str)
{
    bind_kit(ctx, label, (l10n_text_id_t)0, str);
}
