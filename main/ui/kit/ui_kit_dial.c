/*
 * Dials: static ring decorations (hairline rings, rim bands, bezels, minute tracks) baked once
 * into ARGB8888 tiles that hug the ring, and cached.
 *
 * On the device, two costs make drawing these live expensive:
 *   - lv_draw_arc builds a mask over the arc's whole bounding box row by row;
 *   - the draw dispatcher compares every draw task with every earlier one in the frame, so
 *     drawing a ring as many short lines (or a minute track as 60 lines) multiplies the cost
 *     of the whole frame.
 * A cached dial is at most KIT_DIAL_TILES image draws however detailed it is, and the tiles
 * only cover the ring, so the blending stays small.
 */
#include <math.h>
#include <string.h>

#include "ui/kit/ui_kit_internal.h"

#define KIT_DIAL_TILES 16
#define KIT_DIAL_CACHE 24
#define DEG_TO_RAD (3.14159265f / 180.0f)

typedef struct {
    lv_draw_buf_t *buf[KIT_DIAL_TILES];
    lv_area_t area[KIT_DIAL_TILES];
} dial_tiles_t;

typedef struct {
    bool used;
    uint32_t stamp;
    kit_dial_t key;
    dial_tiles_t tiles;
} dial_entry_t;

static dial_entry_t s_cache[KIT_DIAL_CACHE];
static uint32_t s_stamp;

/* --- Geometry --------------------------------------------------------------------------- */

static void sector_bounds(int32_t cx, int32_t cy, float r_in, float r_out, float a0, float a1, lv_area_t *area)
{
    float xs[12];
    float ys[12];
    int n = 0;
    float angles[2] = {a0, a1};

    for (int i = 0; i < 2; i++) {
        xs[n] = (float)cx + r_in * cosf(angles[i] * DEG_TO_RAD);
        ys[n++] = (float)cy + r_in * sinf(angles[i] * DEG_TO_RAD);
        xs[n] = (float)cx + r_out * cosf(angles[i] * DEG_TO_RAD);
        ys[n++] = (float)cy + r_out * sinf(angles[i] * DEG_TO_RAD);
    }
    /* Cardinal directions inside the sector reach the outer radius. */
    for (int k = -360; k <= 720; k += 90) {
        if ((float)k > a0 && (float)k < a1) {
            xs[n] = (float)cx + r_out * cosf((float)k * DEG_TO_RAD);
            ys[n++] = (float)cy + r_out * sinf((float)k * DEG_TO_RAD);
        }
    }
    area->x1 = INT32_MAX;
    area->y1 = INT32_MAX;
    area->x2 = INT32_MIN;
    area->y2 = INT32_MIN;
    for (int i = 0; i < n; i++) {
        area->x1 = LV_MIN(area->x1, (int32_t)floorf(xs[i]) - 1);
        area->y1 = LV_MIN(area->y1, (int32_t)floorf(ys[i]) - 1);
        area->x2 = LV_MAX(area->x2, (int32_t)ceilf(xs[i]) + 1);
        area->y2 = LV_MAX(area->y2, (int32_t)ceilf(ys[i]) + 1);
    }
    area->x1 = LV_CLAMP(0, area->x1, KIT_SCREEN - 1);
    area->y1 = LV_CLAMP(0, area->y1, KIT_SCREEN - 1);
    area->x2 = LV_CLAMP(0, area->x2, KIT_SCREEN - 1);
    area->y2 = LV_CLAMP(0, area->y2, KIT_SCREEN - 1);
}

static float norm_deg(float a)
{
    while (a < 0.0f) {
        a += 360.0f;
    }
    while (a >= 360.0f) {
        a -= 360.0f;
    }
    return a;
}

/* Distance from a point (relative to the centre) to a ring's centre line between its angles,
 * with round ends for partial arcs. */
static float ring_distance(const kit_dial_ring_t *ring, float px, float py, float d, float ang)
{
    float a0 = (float)ring->start_deg;
    float a1 = (float)ring->end_deg;
    float best = 1e9f;

    if (a1 - a0 >= 360.0f) {
        return fabsf(d - (float)ring->r);
    }
    for (int turn = -1; turn <= 1; turn++) {
        float a = ang + 360.0f * (float)turn;

        if (a >= a0 && a <= a1) {
            return fabsf(d - (float)ring->r);
        }
    }
    for (int i = 0; i < 2; i++) {
        float e = (i == 0 ? a0 : a1) * DEG_TO_RAD;
        float ex = (float)ring->r * cosf(e) - px;
        float ey = (float)ring->r * sinf(e) - py;

        best = LV_MIN(best, sqrtf(ex * ex + ey * ey));
    }
    return best;
}

/* Distance from a point to the segment between two points. */
static float segment_distance(float px, float py, float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len2 = dx * dx + dy * dy;
    float t = len2 > 0.0f ? ((px - x0) * dx + (py - y0) * dy) / len2 : 0.0f;
    float qx;
    float qy;

    t = LV_CLAMP(0.0f, t, 1.0f);
    qx = x0 + t * dx - px;
    qy = y0 + t * dy - py;
    return sqrtf(qx * qx + qy * qy);
}

/* Composites colour c at coverage a over the pixel (straight alpha, 0..1). */
static void over(float *r, float *g, float *b, float *a, lv_color_t c, float cover)
{
    float out = cover + *a * (1.0f - cover);

    if (out <= 0.0f) {
        return;
    }
    *r = ((float)c.red * cover + *r * *a * (1.0f - cover)) / out;
    *g = ((float)c.green * cover + *g * *a * (1.0f - cover)) / out;
    *b = ((float)c.blue * cover + *b * *a * (1.0f - cover)) / out;
    *a = out;
}

/* --- Building ----------------------------------------------------------------------------- */

static void dial_extent(const kit_dial_t *dial, float *r_min, float *r_max)
{
    *r_min = 1e9f;
    *r_max = 0.0f;
    for (int i = 0; i < KIT_DIAL_RINGS; i++) {
        const kit_dial_ring_t *ring = &dial->rings[i];

        if (ring->width > 0) {
            float hw = (float)ring->width / 2.0f + 1.0f;

            *r_min = LV_MIN(*r_min, (float)ring->r - hw);
            *r_max = LV_MAX(*r_max, (float)ring->r + hw);
        }
    }
    for (int i = 0; i < KIT_DIAL_TICK_SETS; i++) {
        const kit_dial_ticks_t *tk = &dial->ticks[i];

        if (tk->count > 0) {
            int32_t lo = tk->major_every > 0 ? LV_MIN(tk->r_in, tk->r_in_major) : tk->r_in;
            int32_t hi = tk->major_every > 0 ? LV_MAX(tk->r_out, tk->r_out_major) : tk->r_out;

            *r_min = LV_MIN(*r_min, (float)lo - 2.0f);
            *r_max = LV_MAX(*r_max, (float)hi + 2.0f);
        }
    }
    *r_min = LV_MAX(0.0f, *r_min);
}

static bool ring_touches_sector(const kit_dial_ring_t *ring, float a0, float a1)
{
    float pad = 6.0f; /* round ends reach a little past the angles */

    if (ring->width <= 0) {
        return false;
    }
    if (ring->end_deg - ring->start_deg >= 360) {
        return true;
    }
    for (int turn = -1; turn <= 1; turn++) {
        float s = (float)ring->start_deg + 360.0f * (float)turn - pad;
        float e = (float)ring->end_deg + 360.0f * (float)turn + pad;

        if (s < a1 && e > a0) {
            return true;
        }
    }
    return false;
}

static lv_draw_buf_t *build_tile(const kit_dial_t *dial, float a0, float a1, float r_min, float r_max,
                                 lv_area_t *area)
{
    lv_draw_buf_t *buf;
    int32_t w;
    int32_t h;
    bool any = false;

    sector_bounds(dial->cx, dial->cy, r_min, r_max, a0, a1, area);
    w = lv_area_get_width(area);
    h = lv_area_get_height(area);
    if (w <= 0 || h <= 0) {
        return NULL;
    }
    buf = lv_draw_buf_create((uint32_t)w, (uint32_t)h, LV_COLOR_FORMAT_ARGB8888, 0);
    if (buf == NULL) {
        return NULL;
    }
    for (int32_t y = 0; y < h; y++) {
        lv_color32_t *row = (lv_color32_t *)(buf->data + (size_t)y * buf->header.stride);
        float py = (float)(area->y1 + y) + 0.5f - (float)dial->cy;

        for (int32_t x = 0; x < w; x++) {
            float px = (float)(area->x1 + x) + 0.5f - (float)dial->cx;
            float d = sqrtf(px * px + py * py);
            float ang = norm_deg(atan2f(py, px) / DEG_TO_RAD);
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            float a = 0.0f;

            /* Each pixel belongs to exactly one tile. */
            if (ang < a0 || ang >= a1 || d < r_min - 1.0f || d > r_max + 1.0f) {
                row[x] = (lv_color32_t) {0};
                continue;
            }
            for (int i = 0; i < KIT_DIAL_RINGS; i++) {
                const kit_dial_ring_t *ring = &dial->rings[i];
                float cover;

                if (ring->width <= 0) {
                    continue;
                }
                cover = LV_CLAMP(0.0f, (float)ring->width / 2.0f - ring_distance(ring, px, py, d, ang) + 0.5f, 1.0f);
                if (cover > 0.0f) {
                    over(&r, &g, &b, &a, ring->color, cover * (float)ring->opa / 255.0f);
                }
            }
            for (int set = 0; set < KIT_DIAL_TICK_SETS; set++) {
                const kit_dial_ticks_t *tk = &dial->ticks[set];
                float tick_step;
                float rel;
                int32_t nearest;

                if (tk->count <= 0) {
                    continue;
                }
                /* Only the ticks next to this pixel's angle can touch it. */
                tick_step = 360.0f / (float)tk->count;
                rel = norm_deg(ang - tk->offset_deg);
                nearest = (int32_t)lroundf(rel / tick_step);
                for (int32_t k = nearest - 1; k <= nearest + 1; k++) {
                    int32_t i = ((k % tk->count) + tk->count) % tk->count;
                    bool major = tk->major_every > 0 && i % tk->major_every == 0;
                    float t;
                    float c;
                    float s;
                    float r0;
                    float r1;
                    float cover;
                    float hw;

                    if (i < 64 && ((tk->skip >> i) & 1u) != 0) {
                        continue;
                    }
                    t = (tk->offset_deg + tick_step * (float)i) * DEG_TO_RAD;
                    c = cosf(t);
                    s = sinf(t);
                    r0 = (float)(major ? tk->r_in_major : tk->r_in);
                    r1 = (float)(major ? tk->r_out_major : tk->r_out);
                    hw = (float)(major ? tk->width_major : tk->width) / 2.0f;
                    cover = LV_CLAMP(0.0f, hw - segment_distance(px, py, r0 * c, r0 * s, r1 * c, r1 * s) + 0.5f, 1.0f);
                    if (cover > 0.0f) {
                        over(&r, &g, &b, &a, major ? tk->color_major : tk->color, cover * (float)tk->opa / 255.0f);
                    }
                }
            }
            if (a > 0.0f) {
                any = true;
            }
            row[x].red = (uint8_t)lroundf(r);
            row[x].green = (uint8_t)lroundf(g);
            row[x].blue = (uint8_t)lroundf(b);
            row[x].alpha = (uint8_t)lroundf(a * 255.0f);
        }
    }
    if (!any) {
        lv_draw_buf_destroy(buf);
        return NULL;
    }
    return buf;
}

static void free_tiles(dial_tiles_t *tiles)
{
    for (int t = 0; t < KIT_DIAL_TILES; t++) {
        if (tiles->buf[t] != NULL) {
            lv_draw_buf_destroy(tiles->buf[t]);
            tiles->buf[t] = NULL;
        }
    }
}

static void build_tiles(const kit_dial_t *dial, dial_tiles_t *tiles)
{
    const float step = 360.0f / (float)KIT_DIAL_TILES;
    float r_min;
    float r_max;

    dial_extent(dial, &r_min, &r_max);
    for (int t = 0; t < KIT_DIAL_TILES; t++) {
        float a0 = step * (float)t;
        float a1 = step * (float)(t + 1);
        bool needed = dial->ticks[0].count > 0 || dial->ticks[1].count > 0;

        for (int i = 0; i < KIT_DIAL_RINGS && !needed; i++) {
            needed = ring_touches_sector(&dial->rings[i], a0, a1);
        }
        tiles->buf[t] = needed ? build_tile(dial, a0, a1, r_min, r_max, &tiles->area[t]) : NULL;
    }
}

/* --- Cache -------------------------------------------------------------------------------- */

static const dial_tiles_t *dial_get(const kit_dial_t *dial)
{
    dial_entry_t *slot = NULL;

    for (int i = 0; i < KIT_DIAL_CACHE; i++) {
        if (s_cache[i].used && memcmp(&s_cache[i].key, dial, sizeof(*dial)) == 0) {
            s_cache[i].stamp = ++s_stamp;
            return &s_cache[i].tiles;
        }
    }
    for (int i = 0; i < KIT_DIAL_CACHE; i++) {
        if (!s_cache[i].used) {
            slot = &s_cache[i];
            break;
        }
        if (slot == NULL || s_cache[i].stamp < slot->stamp) {
            slot = &s_cache[i];
        }
    }
    free_tiles(&slot->tiles);
    memcpy(&slot->key, dial, sizeof(*dial));
    build_tiles(dial, &slot->tiles);
    slot->used = true;
    slot->stamp = ++s_stamp;
    return &slot->tiles;
}

void kit_dial_draw(lv_layer_t *layer, const kit_dial_t *dial)
{
    const dial_tiles_t *tiles = dial_get(dial);
    lv_draw_image_dsc_t img;

    lv_draw_image_dsc_init(&img);
    for (int t = 0; t < KIT_DIAL_TILES; t++) {
        if (tiles->buf[t] != NULL) {
            img.src = tiles->buf[t];
            lv_draw_image(layer, &img, &tiles->area[t]);
        }
    }
}

void kit_dial_cache_flush(void)
{
    kit_slider_cache_flush();
    for (int i = 0; i < KIT_DIAL_CACHE; i++) {
        free_tiles(&s_cache[i].tiles);
        s_cache[i].used = false;
    }
}

void kit_draw_ring(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t radius, int32_t width, int32_t start_deg,
                   int32_t end_deg, lv_color_t color, lv_opa_t opa)
{
    kit_dial_t dial;

    memset(&dial, 0, sizeof(dial));
    dial.cx = cx;
    dial.cy = cy;
    dial.rings[0] = (kit_dial_ring_t) {
        .r = radius,
        .width = width,
        .start_deg = start_deg,
        .end_deg = end_deg,
        .color = color,
        .opa = opa,
    };
    kit_dial_draw(layer, &dial);
}

/* --- Rim bands ---------------------------------------------------------------------------- */

lv_draw_buf_t *kit_arc_band_image(int32_t cx, int32_t cy, int32_t r_mid, int32_t width, float a0, float a1,
                                  lv_color_t fill, lv_opa_t fill_opa, lv_color_t edge, lv_opa_t edge_opa,
                                  lv_area_t *area)
{
    kit_dial_ring_t ring = {.r = r_mid, .start_deg = (int32_t)a0, .end_deg = (int32_t)a1};
    float hw = (float)width / 2.0f;
    lv_draw_buf_t *buf;
    int32_t w;
    int32_t h;

    sector_bounds(cx, cy, (float)(r_mid - width / 2 - 1), (float)(r_mid + width / 2 + 1), a0 - 10.0f, a1 + 10.0f,
                  area);
    w = lv_area_get_width(area);
    h = lv_area_get_height(area);
    buf = lv_draw_buf_create((uint32_t)w, (uint32_t)h, LV_COLOR_FORMAT_ARGB8888, 0);
    if (buf == NULL) {
        return NULL;
    }
    for (int32_t y = 0; y < h; y++) {
        lv_color32_t *row = (lv_color32_t *)(buf->data + (size_t)y * buf->header.stride);
        float py = (float)(area->y1 + y) + 0.5f - (float)cy;

        for (int32_t x = 0; x < w; x++) {
            float px = (float)(area->x1 + x) + 0.5f - (float)cx;
            float d = sqrtf(px * px + py * py);
            float dist = ring_distance(&ring, px, py, d, norm_deg(atan2f(py, px) / DEG_TO_RAD));
            float outer = LV_CLAMP(0.0f, hw - dist + 0.5f, 1.0f);
            float inner = edge_opa > 0 ? LV_CLAMP(0.0f, hw - 1.0f - dist + 0.5f, 1.0f) : outer;
            float fa = inner * (float)fill_opa;
            float ea = (outer - inner) * (float)edge_opa;
            float alpha = fa + ea;

            if (alpha <= 0.0f) {
                row[x] = (lv_color32_t) {0};
                continue;
            }
            row[x].red = (uint8_t)lroundf((fa * fill.red + ea * edge.red) / alpha);
            row[x].green = (uint8_t)lroundf((fa * fill.green + ea * edge.green) / alpha);
            row[x].blue = (uint8_t)lroundf((fa * fill.blue + ea * edge.blue) / alpha);
            row[x].alpha = (uint8_t)lroundf(LV_MIN(alpha, 255.0f));
        }
    }
    return buf;
}
