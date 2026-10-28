/*
 * Boot: a ring of light fills round the rim while the faces are built (each step is drawn as
 * it finishes: the build holds the UI thread), then, once all is ready, the screen opens on
 * the face like an iris: the ring blooms and fades, a black ring with a soft inner edge and a
 * glint widens from the centre out past the rim, and the start chime sounds as it begins.
 */
#include "ui/clock_ui_private.h"
#include "ui/ui_areas.h"

#define BOOT_RING_R 318      /* the progress ring, inside the rim */
#define BOOT_INTRO_MS 1150   /* from the end of the build to the face */
#define BOOT_CHIME_MS 120    /* the chime starts with the bloom */
#define BOOT_IRIS_FROM_MS 230
#define BOOT_IRIS_MS 880
#define BOOT_IRIS_END_R 400  /* past the screen's circle, with the feather */
#define BOOT_COVER_R 520     /* the black reaches the square's corners */
#define BOOT_CX 360
#define BOOT_CY 360

static float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static float ease_out_cubic(float t)
{
    float u = 1.0f - t;

    return 1.0f - u * u * u;
}

static float ease_in_out(float t)
{
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

static void arc(lv_layer_t *layer, int32_t radius, int32_t width, float a0, float a1, lv_color_t color, lv_opa_t opa,
                bool rounded)
{
    lv_draw_arc_dsc_t d;

    if (opa <= LV_OPA_MIN || width <= 0 || radius <= 0) {
        return;
    }
    lv_draw_arc_dsc_init(&d);
    d.center.x = BOOT_CX;
    d.center.y = BOOT_CY;
    d.radius = (uint16_t)radius;
    d.width = (uint16_t)LV_MIN(width, radius);
    d.start_angle = a0;
    d.end_angle = a1;
    d.color = color;
    d.opa = opa;
    d.rounded = rounded;
    lv_draw_arc(layer, &d);
}

static void dot(lv_layer_t *layer, float angle_deg, int32_t r_ring, int32_t size, lv_color_t color, lv_opa_t opa)
{
    lv_draw_rect_dsc_t d;
    float a = angle_deg * (float)M_PI / 180.0f;
    int32_t x = BOOT_CX + (int32_t)lroundf(cosf(a) * (float)r_ring);
    int32_t y = BOOT_CY + (int32_t)lroundf(sinf(a) * (float)r_ring);
    lv_area_t area = {x - size / 2, y - size / 2, x + size / 2, y + size / 2};

    if (opa <= LV_OPA_MIN) {
        return;
    }
    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    d.bg_opa = opa;
    d.radius = LV_RADIUS_CIRCLE;
    lv_draw_rect(layer, &d, &area);
}

static void boot_draw_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    clock_ui_boot_state_t *b = &ctx->boot;
    float t = b->intro ? (float)b->intro_ms : 0.0f;
    float progress = b->steps > 0 ? (float)b->step / (float)b->steps : 0.0f;
    float bloom = b->intro ? sinf(clamp01(t / 420.0f) * (float)M_PI) : 0.0f; /* swells and settles */
    float ring = 1.0f - clamp01((t - 260.0f) / 420.0f);
    float iris = ease_in_out(clamp01((t - BOOT_IRIS_FROM_MS) / BOOT_IRIS_MS));
    lv_color_t warm = lv_color_hex(0xFFC690);
    lv_color_t light = lv_color_hex(0xF6EEE4);
    float end;

    if (b->intro) {
        /* The ring closes over the first part of the bloom, whatever the build reached. */
        progress += (1.0f - progress) * ease_out_cubic(clamp01(t / 240.0f));
    }
    if (iris > 0.0f) {
        int32_t open = (int32_t)lroundf(iris * (float)BOOT_IRIS_END_R);

        /* Black from the iris out; a soft inner edge and a glint fading as it widens. */
        arc(layer, BOOT_COVER_R, BOOT_COVER_R - open, 0, 360, lv_color_black(), LV_OPA_COVER, false);
        if (open > 24) {
            float soft = 1.0f - iris;

            arc(layer, open, 7, 0, 360, lv_color_black(), (lv_opa_t)(190.0f * soft), false);
            arc(layer, open - 7, 7, 0, 360, lv_color_black(), (lv_opa_t)(110.0f * soft), false);
            arc(layer, open - 14, 8, 0, 360, lv_color_black(), (lv_opa_t)(45.0f * soft), false);
            arc(layer, open + 2, 2, 0, 360, warm, (lv_opa_t)(170.0f * soft * soft), false);
        }
    } else {
        lv_draw_rect_dsc_t d;
        lv_area_t all = {0, 0, BOOT_CX * 2 - 1, BOOT_CY * 2 - 1};

        lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_black();
        d.bg_opa = LV_OPA_COVER;
        lv_draw_rect(layer, &d, &all);
    }
    if (ring <= 0.0f) {
        return;
    }
    /* The track, the glow under the filled part, the fill and its bright head. */
    end = 270.0f + 360.0f * progress;
    arc(layer, BOOT_RING_R + 1, 2, 0, 360, lv_color_hex(0x1C1F24), (lv_opa_t)(255.0f * ring), false);
    if (progress > 0.002f) {
        arc(layer, BOOT_RING_R + 8, 18, 270.0f, end, warm, (lv_opa_t)((22.0f + 70.0f * bloom) * ring), true);
        arc(layer, BOOT_RING_R + 2, 4, 270.0f, end, light, (lv_opa_t)(235.0f * ring), true);
        if (progress < 0.999f) {
            dot(layer, end, BOOT_RING_R, 22, warm, (lv_opa_t)(60.0f * ring));
            dot(layer, end, BOOT_RING_R, 8, lv_color_white(), (lv_opa_t)(255.0f * ring));
        }
    }
}

void ui_boot_create(clock_ui_context_t *ctx, uint8_t steps)
{
    lv_obj_t *o;

    ctx->screen = lv_screen_active();
    lv_obj_set_style_bg_color(ctx->screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(ctx->screen, 0, 0);
    lv_obj_remove_flag(ctx->screen, LV_OBJ_FLAG_SCROLLABLE);

    o = lv_obj_create(lv_layer_top());
    ctx->boot.overlay = o;
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE); /* nothing under it takes a touch before the face is up */
    lv_obj_add_event_cb(o, boot_draw_cb, LV_EVENT_DRAW_MAIN, ctx);

    ctx->boot.title = lv_label_create(o);
    lv_obj_set_style_text_font(ctx->boot.title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ctx->boot.title, lv_color_hex(0x8A9099), 0);
    lv_obj_set_style_text_letter_space(ctx->boot.title, 7, 0);
    lv_label_set_text(ctx->boot.title, "TOUCH CLOCK");
    lv_obj_center(ctx->boot.title);

    ctx->boot.step = 0;
    ctx->boot.steps = steps;
    ctx->boot.intro = false;
    ctx->boot.chimed = false;
    lv_refr_now(NULL);
}

void ui_boot_step(clock_ui_context_t *ctx)
{
    if (ctx->boot.overlay == NULL) {
        return;
    }
    if (ctx->boot.step < ctx->boot.steps) {
        ctx->boot.step++;
    }
    lv_obj_invalidate(ctx->boot.overlay);
    lv_refr_now(NULL);
}

static void intro_exec(void *var, int32_t v)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)var;

    ctx->boot.intro_ms = (uint16_t)v;
    if (ctx->boot.title != NULL) {
        lv_obj_set_style_text_opa(ctx->boot.title, (lv_opa_t)(255.0f * (1.0f - clamp01((float)v / 220.0f))), 0);
    }
    if (!ctx->boot.chimed && v >= BOOT_CHIME_MS) {
        ctx->boot.chimed = true;
        if (ctx->callbacks.on_boot_chime != NULL) {
            ctx->callbacks.on_boot_chime(ctx->user_ctx);
        }
    }
    lv_obj_invalidate(ctx->boot.overlay);
}

static void intro_done(lv_anim_t *a)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_anim_get_user_data(a);

    if (ctx->boot.overlay != NULL) {
        lv_obj_delete(ctx->boot.overlay);
    }
    ctx->boot.overlay = NULL;
    ctx->boot.title = NULL;
    ctx->boot.intro = false;
    ui_face_chrome_show_controls(ctx);
}

void ui_boot_finish(clock_ui_context_t *ctx)
{
    lv_anim_t a;

    if (ctx->boot.overlay == NULL) {
        ui_face_chrome_show_controls(ctx);
        return;
    }
    ctx->boot.step = ctx->boot.steps;
    ctx->boot.intro = true;
    ctx->boot.intro_ms = 0;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ctx);
    lv_anim_set_user_data(&a, ctx);
    lv_anim_set_exec_cb(&a, intro_exec);
    lv_anim_set_values(&a, 0, BOOT_INTRO_MS);
    lv_anim_set_duration(&a, BOOT_INTRO_MS);
    lv_anim_set_completed_cb(&a, intro_done);
    lv_anim_start(&a);
}

/* Debug call: the boot screen again, its ring filling, then the intro. */
static void replay_step_cb(lv_timer_t *t)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_timer_get_user_data(t);

    if (ctx->boot.overlay == NULL) {
        lv_timer_delete(t);
        return;
    }
    if (ctx->boot.step + 1 >= ctx->boot.steps) {
        lv_timer_delete(t);
        ui_boot_finish(ctx);
        return;
    }
    ctx->boot.step++;
    lv_obj_invalidate(ctx->boot.overlay);
}

void ui_boot_replay(clock_ui_context_t *ctx)
{
    if (ctx->boot.overlay != NULL) {
        return;
    }
    ui_face_chrome_close_all(ctx);
    ui_boot_create(ctx, 14);
    lv_timer_create(replay_step_cb, 90, ctx);
}
