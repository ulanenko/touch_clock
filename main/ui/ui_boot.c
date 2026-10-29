/*
 * Boot: the wordmark on black with a thin line filling under it while the faces are built
 * (each step is drawn as it finishes: the build holds the UI thread). Once all is ready the
 * wordmark fades, the backlight goes dark, the face is drawn, and the backlight brings it up
 * out of the dark as the start chime plays (the app fades the backlight on a timer of its
 * own, so the reveal costs no frames at all).
 */
#include "ui/clock_ui_private.h"
#include "ui/ui_areas.h"
#include "ui/ui_face_chrome.h"

#define BOOT_BAR_W 168
#define BOOT_BAR_Y 34      /* under the wordmark's centre */
#define BOOT_FADE_MS 320   /* the wordmark and line fade */
#define BOOT_REVEAL_MS 1400 /* the backlight up from dark */

void ui_boot_create(clock_ui_context_t *ctx, uint8_t steps)
{
    clock_ui_boot_state_t *b = &ctx->boot;
    lv_obj_t *o;

    ctx->screen = lv_screen_active();
    lv_obj_set_style_bg_color(ctx->screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(ctx->screen, 0, 0);
    lv_obj_remove_flag(ctx->screen, LV_OBJ_FLAG_SCROLLABLE);

    o = lv_obj_create(lv_layer_top());
    b->overlay = o;
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(o, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE); /* nothing under it takes a touch before the face is up */

    b->title = lv_label_create(o);
    lv_obj_set_style_text_font(b->title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(b->title, lv_color_hex(0xB8BEC6), 0);
    lv_obj_set_style_text_letter_space(b->title, 7, 0);
    lv_label_set_text(b->title, "TOUCH CLOCK");
    lv_obj_align(b->title, LV_ALIGN_CENTER, 0, -6);

    b->bar = lv_obj_create(o);
    lv_obj_remove_style_all(b->bar);
    lv_obj_set_size(b->bar, BOOT_BAR_W, 2);
    lv_obj_align(b->bar, LV_ALIGN_CENTER, 0, BOOT_BAR_Y);
    lv_obj_set_style_radius(b->bar, 1, 0);
    lv_obj_set_style_bg_color(b->bar, lv_color_hex(0x24282E), 0);
    lv_obj_set_style_bg_opa(b->bar, LV_OPA_COVER, 0);

    b->fill = lv_obj_create(b->bar);
    lv_obj_remove_style_all(b->fill);
    lv_obj_set_size(b->fill, 0, 2);
    lv_obj_set_style_radius(b->fill, 1, 0);
    lv_obj_set_style_bg_color(b->fill, lv_color_hex(0xFFD2A6), 0);
    lv_obj_set_style_bg_opa(b->fill, LV_OPA_COVER, 0);

    b->step = 0;
    b->steps = steps;
    b->revealing = false;
    lv_refr_now(NULL);
}

void ui_boot_step(clock_ui_context_t *ctx)
{
    clock_ui_boot_state_t *b = &ctx->boot;

    if (b->overlay == NULL) {
        return;
    }
    if (b->step < b->steps) {
        b->step++;
    }
    lv_obj_set_width(b->fill, b->steps > 0 ? BOOT_BAR_W * b->step / b->steps : BOOT_BAR_W);
    lv_refr_now(NULL);
}

static void fade_exec(void *var, int32_t v)
{
    clock_ui_boot_state_t *b = &((clock_ui_context_t *)var)->boot;
    lv_opa_t opa = (lv_opa_t)(255 - v);

    lv_obj_set_style_text_opa(b->title, opa, 0);
    lv_obj_set_style_bg_opa(b->bar, opa, 0);
    lv_obj_set_style_bg_opa(b->fill, opa, 0);
}

/* Black on the backlight too, the face drawn under the boot screen's black, then up. */
static void fade_done(lv_anim_t *a)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_anim_get_user_data(a);
    clock_ui_boot_state_t *b = &ctx->boot;
    bool backlight = ctx->callbacks.on_boot_backlight != NULL;

    if (backlight) {
        ctx->callbacks.on_boot_backlight(ctx->user_ctx, 0);
    }
    if (b->overlay != NULL) {
        lv_obj_delete(b->overlay);
    }
    b->overlay = NULL;
    b->title = NULL;
    b->bar = NULL;
    b->fill = NULL;
    b->revealing = false;
    lv_refr_now(NULL);
    if (backlight) {
        ctx->callbacks.on_boot_backlight(ctx->user_ctx, BOOT_REVEAL_MS);
    }
    if (ctx->callbacks.on_boot_chime != NULL) {
        ctx->callbacks.on_boot_chime(ctx->user_ctx);
    }
}

void ui_boot_finish(clock_ui_context_t *ctx)
{
    clock_ui_boot_state_t *b = &ctx->boot;
    lv_anim_t a;

    if (b->overlay == NULL || b->revealing) {
        return;
    }
    b->step = b->steps;
    lv_obj_set_width(b->fill, BOOT_BAR_W);
    b->revealing = true;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ctx);
    lv_anim_set_user_data(&a, ctx);
    lv_anim_set_exec_cb(&a, fade_exec);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, BOOT_FADE_MS);
    lv_anim_set_delay(&a, 120); /* the full line is seen */
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&a, fade_done);
    lv_anim_start(&a);
}

/* Debug call: the boot screen again, its line filling, then the reveal. */
static void replay_step_cb(lv_timer_t *t)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_timer_get_user_data(t);
    clock_ui_boot_state_t *b = &ctx->boot;

    if (b->overlay == NULL) {
        lv_timer_delete(t);
        return;
    }
    if (b->step + 1 >= b->steps) {
        lv_timer_delete(t);
        ui_boot_finish(ctx);
        return;
    }
    b->step++;
    lv_obj_set_width(b->fill, BOOT_BAR_W * b->step / b->steps);
}

void ui_boot_replay(clock_ui_context_t *ctx)
{
    if (ctx->boot.overlay != NULL) {
        return;
    }
    ui_face_chrome_close_all(ctx);
    face_controls_hide(ctx);
    ui_boot_create(ctx, 14);
    lv_timer_create(replay_step_cb, 90, ctx);
}
