#include "ui/ui_areas.h"

#include <string.h>

#include "ui/clock_ui_private.h"
#include "ui/kit/ui_kit.h"
#include "ui/kit/ui_skin.h"

#define UI_DEBUG_CALL_MAX 160

typedef struct {
    const char *name;
    ui_debug_call_t fn;
} ui_debug_entry_t;

static ui_debug_entry_t s_debug_calls[UI_DEBUG_CALL_MAX];
static size_t s_debug_call_count;

lv_obj_t *ui_chrome_root(const clock_ui_context_t *ctx)
{
    return ctx->chrome_root;
}

lv_timer_t *ui_chrome_timer_create(clock_ui_context_t *ctx, lv_timer_cb_t cb, uint32_t period_ms, void *user_data)
{
    lv_timer_t *timer;

    if (ctx->chrome_timer_count >= UI_CHROME_TIMER_MAX) {
        LV_LOG_ERROR("chrome timer table full");
        return NULL;
    }
    timer = lv_timer_create(cb, period_ms, user_data);
    ctx->chrome_timers[ctx->chrome_timer_count++] = timer;
    return timer;
}

void ui_chrome_build(clock_ui_context_t *ctx)
{
    ctx->applied_style = ctx->settings->ui_style;
    ui_skin_set_style((clock_ui_style_t)ctx->applied_style);
    ctx->l10n_chrome_start = ctx->l10n_binding_count;

    /* A plain full-screen container: touches fall through it to the faces. */
    ctx->chrome_root = lv_obj_create(ctx->screen);
    lv_obj_remove_style_all(ctx->chrome_root);
    lv_obj_set_size(ctx->chrome_root, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_remove_flag(ctx->chrome_root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    ui_face_chrome_create(ctx);
    ui_alarms_create(ctx);
    ui_settings_create(ctx);
    ui_face_chrome_create_top(ctx);
    ui_debug_register("kit_specimen", ui_kit_specimen_show);
}

void ui_chrome_refresh(clock_ui_context_t *ctx)
{
    clock_face_id_t face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));

    ui_settings_refresh(ctx);
    ui_alarms_refresh(ctx);
    ui_face_chrome_refresh(ctx);
    ui_face_chrome_on_face_changed(ctx, face);
    ui_alarms_on_face_changed(ctx, face);
}

static void chrome_rebuild_cb(void *user_data)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)user_data;

    ui_settings_page_t settings_page = ui_settings_current_page(ctx);

    ctx->chrome_rebuild_pending = false;

    set_root_ui_hidden(ctx, false);
    ui_face_chrome_destroy(ctx);
    ui_settings_destroy(ctx);
    ui_alarms_destroy(ctx);
    for (uint8_t i = 0; i < ctx->chrome_timer_count; i++) {
        lv_timer_delete(ctx->chrome_timers[i]);
    }
    ctx->chrome_timer_count = 0;
    kit_dial_cache_flush();
    if (ctx->chrome_root != NULL) {
        lv_obj_delete(ctx->chrome_root);
        ctx->chrome_root = NULL;
    }

    /* The chrome state below only points into the deleted tree. */
    memset(&ctx->brightness, 0, sizeof(ctx->brightness));
    memset(&ctx->face_theme, 0, sizeof(ctx->face_theme));
    memset(&ctx->face_chrome, 0, sizeof(ctx->face_chrome));
    memset(&ctx->settings_ui, 0, sizeof(ctx->settings_ui));
    memset(&ctx->alarms, 0, sizeof(ctx->alarms));
    memset(ctx->page_dots, 0, sizeof(ctx->page_dots));
    ctx->settings_button = NULL;
    ctx->update_button = NULL;
    ctx->update_button_label = NULL;
    ctx->affordance_hide_timer = NULL;
    ctx->affordances_visible = false;
    ctx->l10n_binding_count = ctx->l10n_chrome_start;
    ui_face_chrome_set_hidden(ctx, false);

    ui_chrome_build(ctx);
    ui_l10n_apply(ctx);
    ui_chrome_refresh(ctx);
    /* The style is changed from settings: come back to the same page in the new style. */
    if (settings_page != UI_SETTINGS_PAGE_NONE) {
        ui_settings_open_page(ctx, settings_page);
    }
    lv_obj_invalidate(ctx->screen);
}

void ui_chrome_rebuild_async(clock_ui_context_t *ctx)
{
    if (ctx->chrome_rebuild_pending) {
        return;
    }
    ctx->chrome_rebuild_pending = true;
    lv_async_call(chrome_rebuild_cb, ctx);
}

void ui_debug_register(const char *name, ui_debug_call_t fn)
{
    for (size_t i = 0; i < s_debug_call_count; i++) {
        if (strcmp(s_debug_calls[i].name, name) == 0) {
            s_debug_calls[i].fn = fn;
            return;
        }
    }
    if (s_debug_call_count < UI_DEBUG_CALL_MAX) {
        s_debug_calls[s_debug_call_count++] = (ui_debug_entry_t) {.name = name, .fn = fn};
    } else {
        LV_LOG_ERROR("debug call table full, dropping %s", name);
    }
}

bool ui_debug_call(clock_ui_context_t *ctx, const char *name)
{
    for (size_t i = 0; i < s_debug_call_count; i++) {
        if (strcmp(s_debug_calls[i].name, name) == 0) {
            s_debug_calls[i].fn(ctx);
            return true;
        }
    }
    return false;
}
