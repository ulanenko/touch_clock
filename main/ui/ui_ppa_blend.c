#include "sdkconfig.h"

#include "ui/clock_ui_private.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "src/draw/lv_draw_private.h"
#include "src/misc/lv_area_private.h"
#include "src/draw/sw/lv_draw_sw.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_to_rgb565.h"

#if CONFIG_SOC_PPA_SUPPORTED
#include "driver/ppa.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_private/esp_cache_private.h"
#include "esp_timer.h"
#endif

/*
 * The RGB565 blend handler. Large full-width blends into the frame buffer (full-screen face
 * images, the screen background, overlay scrims, sheet snapshots) go to the PPA where there
 * is one. Everything else is blended on the CPU: ARGB8888 images by the loops below, the
 * rest by LVGL's software blender.
 *
 * Only whole rows whose bytes start and end on a cache line go to the PPA. The PPA driver
 * writes back and invalidates the cache lines of the rows it touches. With two software
 * draw units, the other unit may be drawing a non-overlapping task at the same time; such a
 * task can only be in other rows, and cache-line-aligned row ranges never share a line with
 * them. The rows at the edges that are not aligned are blended in software.
 */

/* --- Sparse images ----------------------------------------------------------------------- */

/* Mostly transparent ARGB8888 images (the kit's ring and dial tiles) can be registered with
 * the runs of visible pixels in each row. Blending one then touches only those pixels: the
 * Atelier rim's tiles cover about 125k pixels for about 4k visible ones. The kit registers a
 * tile when it builds it and unregisters it before freeing it, both on the UI thread; the
 * draw threads only read. */
#define SPARSE_MAX 192

typedef struct {
    const uint8_t *data; /* NULL for a free slot; published last */
    size_t size;
    uint32_t stride;
    uint32_t *row_start; /* h + 1 indices into runs */
    uint16_t *runs;      /* (x, length) pairs */
} sparse_image_t;

static sparse_image_t s_sparse[SPARSE_MAX];
static int s_sparse_count; /* slots in use or once used: the lookup scans these */

void ui_sparse_image_register(const lv_draw_buf_t *buf)
{
    uint32_t h = buf->header.h;
    uint32_t w = buf->header.w;
    uint32_t runs = 0;
    sparse_image_t *slot = NULL;

    if (buf->header.cf != LV_COLOR_FORMAT_ARGB8888) {
        return;
    }
    for (int i = 0; i < SPARSE_MAX && slot == NULL; i++) {
        if (s_sparse[i].data == NULL) {
            slot = &s_sparse[i];
        }
    }
    if (slot == NULL) {
        return; /* blended in full instead */
    }
    for (uint32_t y = 0; y < h; y++) {
        const lv_color32_t *row = (const lv_color32_t *)(buf->data + (size_t)y * buf->header.stride);

        for (uint32_t x = 0; x < w; x++) {
            if (row[x].alpha != 0 && (x == 0 || row[x - 1].alpha == 0)) {
                runs++;
            }
        }
    }
    slot->row_start = lv_malloc((h + 1) * sizeof(uint32_t));
    slot->runs = lv_malloc((runs ? runs : 1) * 2 * sizeof(uint16_t));
    if (slot->row_start == NULL || slot->runs == NULL) {
        lv_free(slot->row_start);
        lv_free(slot->runs);
        return;
    }
    runs = 0;
    for (uint32_t y = 0; y < h; y++) {
        const lv_color32_t *row = (const lv_color32_t *)(buf->data + (size_t)y * buf->header.stride);

        slot->row_start[y] = runs;
        for (uint32_t x = 0; x < w; x++) {
            if (row[x].alpha == 0) {
                continue;
            }
            if (x == 0 || row[x - 1].alpha == 0) {
                slot->runs[runs * 2] = (uint16_t)x;
                slot->runs[runs * 2 + 1] = 0;
                runs++;
            }
            slot->runs[(runs - 1) * 2 + 1]++;
        }
    }
    slot->row_start[h] = runs;
    slot->size = (size_t)h * buf->header.stride;
    slot->stride = buf->header.stride;
    __atomic_store_n(&slot->data, buf->data, __ATOMIC_RELEASE);
    if (slot - s_sparse >= s_sparse_count) {
        __atomic_store_n(&s_sparse_count, (int)(slot - s_sparse) + 1, __ATOMIC_RELEASE);
    }
}

bool ui_sparse_image_known(const void *data)
{
    for (int i = 0; i < s_sparse_count; i++) {
        if (s_sparse[i].data == data) {
            return true;
        }
    }
    return false;
}

void ui_sparse_image_unregister(const lv_draw_buf_t *buf)
{
    for (int i = 0; i < s_sparse_count; i++) {
        if (s_sparse[i].data == buf->data) {
            __atomic_store_n(&s_sparse[i].data, NULL, __ATOMIC_RELEASE);
            lv_free(s_sparse[i].row_start);
            lv_free(s_sparse[i].runs);
            s_sparse[i].row_start = NULL;
            s_sparse[i].runs = NULL;
            return;
        }
    }
}

static inline uint16_t argb_mix(uint32_t px, uint16_t c)
{
    uint32_t mix = px >> 24;
    uint32_t inv;

    if (mix == 255) {
        return (uint16_t)(((px >> 8) & 0xF800) | ((px >> 5) & 0x07E0) | ((px & 0xF8) >> 3));
    }
    inv = 255 - mix;
    return (uint16_t)((((((px >> 19) & 0x1F) * mix + ((c >> 11) & 0x1F) * inv) << 3) & 0xF800) +
                      (((((px >> 10) & 0x3F) * mix + ((c >> 5) & 0x3F) * inv) >> 3) & 0x07E0) +
                      ((((px >> 3) & 0x1F) * mix + (c & 0x1F) * inv) >> 8));
}

/* An ARGB8888 pixel with its alpha scaled by opa (a fading image). */
static inline uint32_t argb_fade(uint32_t px, lv_opa_t opa)
{
    return (px & 0x00FFFFFFu) | (((px >> 24) * opa / 255u) << 24);
}

/* Blends only the visible runs if the source lies in a registered sparse image. */
static bool sparse_over_rgb565(const lv_draw_sw_blend_image_dsc_t *dsc)
{
    const uint8_t *src = dsc->src_buf;
    lv_opa_t opa = dsc->opa;
    int count = __atomic_load_n(&s_sparse_count, __ATOMIC_ACQUIRE);

    for (int i = 0; i < count; i++) {
        const uint8_t *data = __atomic_load_n(&s_sparse[i].data, __ATOMIC_ACQUIRE);
        const sparse_image_t *img = &s_sparse[i];
        size_t off;
        int32_t sx;
        int32_t sy;
        uint8_t *dest_row = dsc->dest_buf;

        if (data == NULL || src < data || src >= data + img->size || (uint32_t)dsc->src_stride != img->stride) {
            continue;
        }
        off = (size_t)(src - data);
        sy = (int32_t)(off / img->stride);
        sx = (int32_t)((off % img->stride) / 4);
        for (int32_t y = 0; y < dsc->dest_h; y++) {
            const uint32_t *src_px = (const uint32_t *)(data + (size_t)(sy + y) * img->stride);
            uint16_t *dest = (uint16_t *)dest_row - sx;

            for (uint32_t k = img->row_start[sy + y]; k < img->row_start[sy + y + 1]; k++) {
                int32_t lo = LV_MAX((int32_t)img->runs[k * 2], sx);
                int32_t hi = LV_MIN((int32_t)img->runs[k * 2] + (int32_t)img->runs[k * 2 + 1], sx + dsc->dest_w);

                if (opa >= LV_OPA_MAX) {
                    for (int32_t x = lo; x < hi; x++) {
                        dest[x] = argb_mix(src_px[x], dest[x]);
                    }
                } else {
                    for (int32_t x = lo; x < hi; x++) {
                        dest[x] = argb_mix(argb_fade(src_px[x], opa), dest[x]);
                    }
                }
            }
            dest_row += dsc->dest_stride;
        }
        return true;
    }
    return false;
}

/* An opaque ARGB8888 image over RGB565, as LVGL blends it (lv_color_24_16_mix) but without
 * touching the destination under fully transparent pixels. Cached ring and dial tiles are
 * mostly transparent, and LVGL reads and rewrites every destination pixel of them. */
static void argb8888_over_rgb565(const lv_draw_sw_blend_image_dsc_t *dsc)
{
    const uint8_t *src_row = dsc->src_buf;
    uint8_t *dest_row = dsc->dest_buf;

    for (int32_t y = 0; y < dsc->dest_h; y++) {
        const uint32_t *src = (const uint32_t *)src_row;
        uint16_t *dest = (uint16_t *)dest_row;

        for (int32_t x = 0; x < dsc->dest_w; x++) {
            uint32_t px = src[x];
            uint32_t mix = px >> 24;

            if (mix != 0) {
                dest[x] = argb_mix(px, dest[x]);
            }
        }
        src_row += dsc->src_stride;
        dest_row += dsc->dest_stride;
    }
}

static void sw_blend(lv_layer_t *layer, const lv_draw_sw_blend_dsc_t *dsc, const lv_area_t *area)
{
    lv_area_t blend_area = *area;
    uint32_t layer_stride = layer->draw_buf->header.stride;

    if (dsc->src_buf == NULL) {
        lv_draw_sw_blend_fill_dsc_t fill_dsc;

        fill_dsc.dest_w = lv_area_get_width(&blend_area);
        fill_dsc.dest_h = lv_area_get_height(&blend_area);
        fill_dsc.dest_stride = layer_stride;
        fill_dsc.opa = dsc->opa;
        fill_dsc.color = dsc->color;
        fill_dsc.mask_stride = 0;
        fill_dsc.mask_buf = (dsc->mask_buf == NULL || dsc->mask_res == LV_DRAW_SW_MASK_RES_FULL_COVER) ?
                            NULL : dsc->mask_buf;
        fill_dsc.relative_area = blend_area;
        lv_area_move(&fill_dsc.relative_area, -layer->buf_area.x1, -layer->buf_area.y1);
        fill_dsc.dest_buf = lv_draw_layer_go_to_xy(layer, blend_area.x1 - layer->buf_area.x1,
                                                   blend_area.y1 - layer->buf_area.y1);
        if (fill_dsc.mask_buf != NULL) {
            fill_dsc.mask_stride = dsc->mask_stride == 0 ? lv_area_get_width(dsc->mask_area) : dsc->mask_stride;
            fill_dsc.mask_buf += fill_dsc.mask_stride * (blend_area.y1 - dsc->mask_area->y1) +
                                 (blend_area.x1 - dsc->mask_area->x1);
        }
        lv_draw_sw_blend_color_to_rgb565(&fill_dsc);
        return;
    }

    if (dsc->mask_area != NULL && !lv_area_intersect(&blend_area, &blend_area, dsc->mask_area)) {
        return;
    }

    lv_draw_sw_blend_image_dsc_t image_dsc;
    const uint8_t *src_buf = dsc->src_buf;
    uint32_t src_px_size = lv_color_format_get_bpp(dsc->src_color_format);

    image_dsc.dest_w = lv_area_get_width(&blend_area);
    image_dsc.dest_h = lv_area_get_height(&blend_area);
    image_dsc.dest_stride = layer_stride;
    image_dsc.opa = dsc->opa;
    image_dsc.blend_mode = dsc->blend_mode;
    image_dsc.src_stride = dsc->src_stride;
    image_dsc.src_color_format = dsc->src_color_format;
    src_buf += image_dsc.src_stride * (blend_area.y1 - dsc->src_area->y1);
    src_buf += ((blend_area.x1 - dsc->src_area->x1) * src_px_size) >> 3;
    image_dsc.src_buf = src_buf;
    image_dsc.mask_stride = 0;
    image_dsc.mask_buf = (dsc->mask_buf == NULL || dsc->mask_res == LV_DRAW_SW_MASK_RES_FULL_COVER) ?
                         NULL : dsc->mask_buf;
    if (image_dsc.mask_buf != NULL) {
        image_dsc.mask_stride = dsc->mask_stride ? dsc->mask_stride : lv_area_get_width(dsc->mask_area);
        image_dsc.mask_buf += image_dsc.mask_stride * (blend_area.y1 - dsc->mask_area->y1) +
                              (blend_area.x1 - dsc->mask_area->x1);
    }
    image_dsc.relative_area = blend_area;
    lv_area_move(&image_dsc.relative_area, -layer->buf_area.x1, -layer->buf_area.y1);
    image_dsc.src_area = *dsc->src_area;
    lv_area_move(&image_dsc.src_area, -layer->buf_area.x1, -layer->buf_area.y1);
    image_dsc.dest_buf = lv_draw_layer_go_to_xy(layer, blend_area.x1 - layer->buf_area.x1,
                                                blend_area.y1 - layer->buf_area.y1);
    if (image_dsc.src_color_format == LV_COLOR_FORMAT_ARGB8888 && image_dsc.mask_buf == NULL &&
        image_dsc.blend_mode == LV_BLEND_MODE_NORMAL) {
        /* Registered sparse images at any opacity (a fading ring); others only when opaque. */
        if (sparse_over_rgb565(&image_dsc)) {
            return;
        }
        if (image_dsc.opa >= LV_OPA_MAX) {
            argb8888_over_rgb565(&image_dsc);
            return;
        }
    }
    lv_draw_sw_blend_image_to_rgb565(&image_dsc);
}

#if CONFIG_SOC_PPA_SUPPORTED

#define PPA_BLEND_MIN_ROWS 16

static const char *TAG = "ui_ppa_blend";

static ppa_client_handle_t s_blend_client;
static ppa_client_handle_t s_fill_client;
static size_t s_align;
static uint8_t *s_a8_dummy;
static size_t s_a8_dummy_size;
static bool s_failed_logged;
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
/* Time the draw units spend blocked on the PPA, for the perf tour. */
static uint32_t s_stat_ops;
static uint32_t s_stat_us;
#endif

typedef struct {
    uint8_t *buf;
    uint32_t stride;
    uint32_t pic_w;
    uint32_t pic_h;
} ppa_target_t;

static void ppa_log_failure(esp_err_t err)
{
    if (!s_failed_logged) {
        s_failed_logged = true;
        ESP_LOGW(TAG, "PPA blend failed (%s), using software", esp_err_to_name(err));
    }
}

static ppa_out_pic_blk_config_t ppa_out(const ppa_target_t *dst, int32_t y)
{
    return (ppa_out_pic_blk_config_t) {
        .buffer = dst->buf,
        .buffer_size = dst->stride * dst->pic_h,
        .pic_w = dst->pic_w,
        .pic_h = dst->pic_h,
        .block_offset_x = 0,
        .block_offset_y = y,
        .blend_cm = PPA_BLEND_COLOR_MODE_RGB565,
    };
}

static ppa_in_pic_blk_config_t ppa_in_bg(const ppa_target_t *dst, int32_t y, int32_t h)
{
    return (ppa_in_pic_blk_config_t) {
        .buffer = dst->buf,
        .pic_w = dst->pic_w,
        .pic_h = dst->pic_h,
        .block_w = dst->pic_w,
        .block_h = h,
        .block_offset_x = 0,
        .block_offset_y = y,
        .blend_cm = PPA_BLEND_COLOR_MODE_RGB565,
    };
}

static bool ppa_fill_rows(const ppa_target_t *dst, int32_t y, int32_t h, lv_color_t color, lv_opa_t opa)
{
    esp_err_t err;

    if (opa >= LV_OPA_MAX) {
        ppa_fill_oper_config_t cfg = {
            .out = ppa_out(dst, y),
            .fill_block_w = dst->pic_w,
            .fill_block_h = h,
            .fill_argb_color = {.a = 0xFF, .r = color.red, .g = color.green, .b = color.blue},
            .mode = PPA_TRANS_MODE_BLOCKING,
        };

        cfg.out.fill_cm = PPA_FILL_COLOR_MODE_RGB565;
        LV_PROFILER_DRAW_BEGIN_TAG("ppa_fill");
        err = ppa_do_fill(s_fill_client, &cfg);
        LV_PROFILER_DRAW_END_TAG("ppa_fill");
    } else {
        size_t a8_size = (size_t)dst->pic_w * h;

        if (s_a8_dummy == NULL || s_a8_dummy_size < a8_size) {
            return false;
        }

        /* A8 foreground with a fixed colour and fixed alpha: the A8 data itself is unused. */
        ppa_blend_oper_config_t cfg = {
            .in_bg = ppa_in_bg(dst, y, h),
            .in_fg = {
                .buffer = s_a8_dummy,
                .pic_w = dst->pic_w,
                .pic_h = h,
                .block_w = dst->pic_w,
                .block_h = h,
                .blend_cm = PPA_BLEND_COLOR_MODE_A8,
            },
            .out = ppa_out(dst, y),
            .bg_alpha_update_mode = PPA_ALPHA_NO_CHANGE,
            .fg_alpha_update_mode = PPA_ALPHA_FIX_VALUE,
            .fg_alpha_fix_val = opa,
            .fg_fix_rgb_val = {.r = color.red, .g = color.green, .b = color.blue},
            .mode = PPA_TRANS_MODE_BLOCKING,
        };

        LV_PROFILER_DRAW_BEGIN_TAG("ppa_fill_opa");
        err = ppa_do_blend(s_blend_client, &cfg);
        LV_PROFILER_DRAW_END_TAG("ppa_fill_opa");
    }

    if (err != ESP_OK) {
        ppa_log_failure(err);
        return false;
    }
    return true;
}

static bool ppa_image_rows(const ppa_target_t *dst, int32_t y, int32_t h,
                           const void *src, bool src_argb, uint32_t src_pic_w, uint32_t src_pic_h,
                           int32_t src_x, int32_t src_y, lv_opa_t opa)
{
    ppa_alpha_update_mode_t fg_alpha = PPA_ALPHA_NO_CHANGE;

    /* ARGB8888 keeps its per-pixel alpha, scaled by the overall opacity. */
    if (opa < LV_OPA_MAX) {
        fg_alpha = src_argb ? PPA_ALPHA_SCALE : PPA_ALPHA_FIX_VALUE;
    }
    /* Opaque copies use blend too: a full-frame SRM copy measured 22 ms against 12 ms. */
    ppa_blend_oper_config_t cfg = {
        .in_bg = ppa_in_bg(dst, y, h),
        .in_fg = {
            .buffer = src,
            .pic_w = src_pic_w,
            .pic_h = src_pic_h,
            .block_w = dst->pic_w,
            .block_h = h,
            .block_offset_x = src_x,
            .block_offset_y = src_y,
            .blend_cm = src_argb ? PPA_BLEND_COLOR_MODE_ARGB8888 : PPA_BLEND_COLOR_MODE_RGB565,
        },
        .out = ppa_out(dst, y),
        .bg_alpha_update_mode = PPA_ALPHA_NO_CHANGE,
        .fg_alpha_update_mode = fg_alpha,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    if (fg_alpha == PPA_ALPHA_FIX_VALUE) {
        cfg.fg_alpha_fix_val = opa;
    } else if (fg_alpha == PPA_ALPHA_SCALE) {
        cfg.fg_alpha_scale_ratio = (float)opa / 256.0f;
    }
    esp_err_t err;

    LV_PROFILER_DRAW_BEGIN_TAG(src_argb ? "ppa_image_argb" : "ppa_image");
    err = ppa_do_blend(s_blend_client, &cfg);
    LV_PROFILER_DRAW_END_TAG(src_argb ? "ppa_image_argb" : "ppa_image");

    if (err != ESP_OK) {
        ppa_log_failure(err);
        return false;
    }
    return true;
}

static bool row_is_aligned(const ppa_target_t *dst, int32_t row)
{
    return (((uintptr_t)dst->buf + (size_t)row * dst->stride) & (s_align - 1)) == 0;
}

static bool ppa_blend_eligible(const lv_layer_t *layer, const lv_draw_sw_blend_dsc_t *dsc, const lv_area_t *area)
{
    if (layer->color_format != LV_COLOR_FORMAT_RGB565 || layer->draw_buf == NULL ||
        !esp_ptr_external_ram(layer->draw_buf->data) ||
        ((uintptr_t)layer->draw_buf->data & (s_align - 1)) != 0 ||
        area->x1 != layer->buf_area.x1 || area->x2 != layer->buf_area.x2 ||
        lv_area_get_height(area) < PPA_BLEND_MIN_ROWS ||
        (dsc->mask_buf != NULL && dsc->mask_res != LV_DRAW_SW_MASK_RES_FULL_COVER) ||
        dsc->blend_mode != LV_BLEND_MODE_NORMAL) {
        return false;
    }
    if (dsc->src_buf != NULL &&
        ((dsc->src_color_format != LV_COLOR_FORMAT_RGB565 && dsc->src_color_format != LV_COLOR_FORMAT_ARGB8888) ||
         !esp_ptr_external_ram(dsc->src_buf) ||
         (dsc->src_stride % lv_color_format_get_size(dsc->src_color_format)) != 0)) {
        return false;
    }
    return true;
}

static void ppa_blend_handler(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    lv_layer_t *layer = t->target_layer;
    lv_area_t area;

    if (!lv_area_intersect(&area, dsc->blend_area, &t->clip_area)) {
        return;
    }
    if (dsc->src_buf != NULL &&
        (!lv_area_intersect(&area, &area, dsc->src_area) ||
         (dsc->mask_area != NULL && !lv_area_intersect(&area, &area, dsc->mask_area)))) {
        return;
    }

    if (ppa_blend_eligible(layer, dsc, &area)) {
        ppa_target_t dst = {
            .buf = layer->draw_buf->data,
            .stride = layer->draw_buf->header.stride,
            .pic_w = layer->draw_buf->header.stride / 2,
            .pic_h = lv_area_get_height(&layer->buf_area),
        };
        int32_t first = area.y1 - layer->buf_area.y1;
        int32_t end = area.y2 - layer->buf_area.y1 + 1;

        while (first < end && !row_is_aligned(&dst, first)) {
            first++;
        }
        while (end > first && !row_is_aligned(&dst, end)) {
            end--;
        }

        if (end - first >= PPA_BLEND_MIN_ROWS && (dst.stride * dst.pic_h) % s_align == 0) {
            bool done;
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
            int64_t started_us = esp_timer_get_time();
#endif

            if (dsc->src_buf == NULL) {
                done = ppa_fill_rows(&dst, first, end - first, dsc->color, dsc->opa);
            } else {
                bool argb = dsc->src_color_format == LV_COLOR_FORMAT_ARGB8888;

                done = ppa_image_rows(&dst, first, end - first, dsc->src_buf, argb,
                                      dsc->src_stride / lv_color_format_get_size(dsc->src_color_format),
                                      lv_area_get_height(dsc->src_area),
                                      area.x1 - dsc->src_area->x1,
                                      first + layer->buf_area.y1 - dsc->src_area->y1, dsc->opa);
            }
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
            __atomic_fetch_add(&s_stat_ops, 1, __ATOMIC_RELAXED);
            __atomic_fetch_add(&s_stat_us, (uint32_t)(esp_timer_get_time() - started_us), __ATOMIC_RELAXED);
#endif

            if (done) {
                lv_area_t edge = area;

                if (first + layer->buf_area.y1 > area.y1) {
                    edge.y2 = first + layer->buf_area.y1 - 1;
                    sw_blend(layer, dsc, &edge);
                }
                if (end + layer->buf_area.y1 <= area.y2) {
                    edge.y1 = end + layer->buf_area.y1;
                    edge.y2 = area.y2;
                    sw_blend(layer, dsc, &edge);
                }
                return;
            }
        }
    }

    sw_blend(layer, dsc, &area);
}

#if CONFIG_TOUCH_CLOCK_PERF_TOUR
void ui_ppa_blend_stats(uint32_t *ops, uint32_t *us)
{
    *ops = __atomic_load_n(&s_stat_ops, __ATOMIC_RELAXED);
    *us = __atomic_load_n(&s_stat_us, __ATOMIC_RELAXED);
}
#endif

static lv_draw_sw_custom_blend_handler_t s_handler = {
    .dest_cf = LV_COLOR_FORMAT_RGB565,
    .handler = ppa_blend_handler,
};

void ui_ppa_blend_install(void)
{
    ppa_client_config_t cfg = {.max_pending_trans_num = LV_DRAW_SW_DRAW_UNIT_CNT};

    if (s_blend_client != NULL) {
        return;
    }

    esp_cache_get_alignment(MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA, &s_align);
    if (s_align == 0) {
        return;
    }

    s_a8_dummy_size = (size_t)SCREEN_SIZE * SCREEN_SIZE;
    s_a8_dummy = heap_caps_aligned_calloc(s_align, 1, s_a8_dummy_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);

    cfg.oper_type = PPA_OPERATION_BLEND;
    if (ppa_register_client(&cfg, &s_blend_client) != ESP_OK) {
        return;
    }
    cfg.oper_type = PPA_OPERATION_FILL;
    if (ppa_register_client(&cfg, &s_fill_client) != ESP_OK) {
        return;
    }

    lv_draw_sw_register_blend_handler(&s_handler);
    ESP_LOGI(TAG, "PPA blending enabled (cache line %u)", (unsigned)s_align);
}

#if CONFIG_TOUCH_CLOCK_PERF_TOUR
static void bench_report(const char *name, int64_t start_us, int iterations)
{
    printf("PERF ppa_bench op=%s us=%lld\n", name, (esp_timer_get_time() - start_us) / iterations);
}

void ui_ppa_blend_benchmark(void)
{
    const int iterations = 5;
    const size_t size = (size_t)SCREEN_SIZE * SCREEN_SIZE * 2;
    ppa_target_t dst = {.stride = SCREEN_SIZE * 2, .pic_w = SCREEN_SIZE, .pic_h = SCREEN_SIZE};
    uint8_t *src = heap_caps_aligned_calloc(s_align, 1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    lv_draw_sw_blend_fill_dsc_t fill = {0};
    lv_draw_sw_blend_image_dsc_t image = {0};
    int64_t start;

    dst.buf = heap_caps_aligned_calloc(s_align, 1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if (src == NULL || dst.buf == NULL || s_blend_client == NULL) {
        printf("PERF ppa_bench skipped\n");
        heap_caps_free(src);
        heap_caps_free(dst.buf);
        return;
    }

    fill.dest_buf = dst.buf;
    fill.dest_w = SCREEN_SIZE;
    fill.dest_h = SCREEN_SIZE;
    fill.dest_stride = dst.stride;
    fill.color = lv_color_make(0x20, 0x40, 0x60);
    lv_area_set(&fill.relative_area, 0, 0, SCREEN_SIZE - 1, SCREEN_SIZE - 1);

    image.dest_buf = dst.buf;
    image.dest_w = SCREEN_SIZE;
    image.dest_h = SCREEN_SIZE;
    image.dest_stride = dst.stride;
    image.src_buf = src;
    image.src_stride = dst.stride;
    image.src_color_format = LV_COLOR_FORMAT_RGB565;
    image.blend_mode = LV_BLEND_MODE_NORMAL;
    image.relative_area = fill.relative_area;
    image.src_area = fill.relative_area;

    fill.opa = LV_OPA_COVER;
    start = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        lv_draw_sw_blend_color_to_rgb565(&fill);
    }
    bench_report("cpu_fill", start, iterations);

    fill.opa = LV_OPA_70;
    start = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        lv_draw_sw_blend_color_to_rgb565(&fill);
    }
    bench_report("cpu_fill_opa70", start, iterations);

    image.opa = LV_OPA_COVER;
    start = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        lv_draw_sw_blend_image_to_rgb565(&image);
    }
    bench_report("cpu_image", start, iterations);

    image.opa = LV_OPA_50;
    start = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        lv_draw_sw_blend_image_to_rgb565(&image);
    }
    bench_report("cpu_image_opa50", start, iterations);

    start = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        ppa_fill_rows(&dst, 0, SCREEN_SIZE, fill.color, LV_OPA_COVER);
    }
    bench_report("ppa_fill", start, iterations);

    start = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        ppa_fill_rows(&dst, 0, SCREEN_SIZE, fill.color, LV_OPA_70);
    }
    bench_report("ppa_fill_opa70", start, iterations);

    start = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        ppa_image_rows(&dst, 0, SCREEN_SIZE, src, false, SCREEN_SIZE, SCREEN_SIZE, 0, 0, LV_OPA_COVER);
    }
    bench_report("ppa_image", start, iterations);

    start = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        ppa_image_rows(&dst, 0, SCREEN_SIZE, src, false, SCREEN_SIZE, SCREEN_SIZE, 0, 0, LV_OPA_50);
    }
    bench_report("ppa_image_opa50", start, iterations);

    heap_caps_free(src);
    heap_caps_free(dst.buf);
}
#endif

#else

/* No PPA (the simulator): the same CPU blending, so the simulator draws what the clock does. */
static void cpu_blend_handler(lv_draw_task_t *t, const lv_draw_sw_blend_dsc_t *dsc)
{
    lv_area_t area;

    if (!lv_area_intersect(&area, dsc->blend_area, &t->clip_area)) {
        return;
    }
    if (dsc->src_buf != NULL &&
        (!lv_area_intersect(&area, &area, dsc->src_area) ||
         (dsc->mask_area != NULL && !lv_area_intersect(&area, &area, dsc->mask_area)))) {
        return;
    }
    sw_blend(t->target_layer, dsc, &area);
}

static lv_draw_sw_custom_blend_handler_t s_handler = {
    .dest_cf = LV_COLOR_FORMAT_RGB565,
    .handler = cpu_blend_handler,
};

void ui_ppa_blend_install(void)
{
    static bool installed;

    if (!installed) {
        installed = true;
        lv_draw_sw_register_blend_handler(&s_handler);
    }
}

#if CONFIG_TOUCH_CLOCK_PERF_TOUR
void ui_ppa_blend_benchmark(void)
{
}

void ui_ppa_blend_stats(uint32_t *ops, uint32_t *us)
{
    *ops = 0;
    *us = 0;
}
#endif

#endif
