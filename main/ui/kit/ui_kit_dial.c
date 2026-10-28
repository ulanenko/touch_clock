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
#include "src/draw/lv_draw_private.h"
#include "src/misc/lv_area_private.h"

#define KIT_DIAL_TILES 32
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
    /* No clamp to the screen: dials are built around 0,0 and moved when drawn. */
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

/* Composites colour c at coverage cover (0..1) over an ARGB8888 pixel, straight alpha. */
static void over_px(lv_color32_t *px, lv_color_t c, float cover)
{
    float da = (float)px->alpha / 255.0f;
    float out = cover + da * (1.0f - cover);

    if (out <= 0.0f) {
        return;
    }
    px->red = (uint8_t)lroundf(((float)c.red * cover + (float)px->red * da * (1.0f - cover)) / out);
    px->green = (uint8_t)lroundf(((float)c.green * cover + (float)px->green * da * (1.0f - cover)) / out);
    px->blue = (uint8_t)lroundf(((float)c.blue * cover + (float)px->blue * da * (1.0f - cover)) / out);
    px->alpha = (uint8_t)lroundf(out * 255.0f);
}

typedef struct {
    float u0x, u0y, u1x, u1y;
} sector_t;

/* Which side of a boundary ray a point lies on. Neighbouring tiles evaluate this same
 * expression for their shared boundary, so a pixel on the seam can never fall into neither
 * or both of them (mirrored expressions can round differently once the compiler fuses the
 * multiply-adds). */
static float boundary_side(float ux, float uy, float px, float py)
{
    return ux * py - uy * px;
}

/* Whether a point (relative to the centre) lies in the sector [a0, a1) (span under 180 deg),
 * with cross products instead of atan2. */
static bool in_sector(const sector_t *sec, float px, float py)
{
    return boundary_side(sec->u0x, sec->u0y, px, py) >= 0.0f && boundary_side(sec->u1x, sec->u1y, px, py) < 0.0f;
}

/* Builds one tile. Only pixels inside the dial's annulus are visited (row by row), ownership
 * uses cross products, and each tick only touches its own small bounding box: on the device
 * a per-pixel atan2 over whole tiles took hundreds of milliseconds per dial. */
static lv_draw_buf_t *build_tile(const kit_dial_t *dial, float a0, float a1, float r_min, float r_max,
                                 lv_area_t *area)
{
    sector_t sec = {cosf(a0 * DEG_TO_RAD), sinf(a0 * DEG_TO_RAD), cosf(a1 * DEG_TO_RAD), sinf(a1 * DEG_TO_RAD)};
    float in2 = LV_MAX(0.0f, r_min - 1.0f);
    float out2 = r_max + 1.0f;
    lv_draw_buf_t *buf;
    int32_t w;
    int32_t h;
    bool any = false;

    in2 *= in2;
    out2 *= out2;
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
    lv_draw_buf_clear(buf, NULL);

    /* Rings. */
    for (int32_t y = 0; y < h; y++) {
        lv_color32_t *row = (lv_color32_t *)(buf->data + (size_t)y * buf->header.stride);
        float py = (float)(area->y1 + y) + 0.5f - (float)dial->cy;
        float py2 = py * py;
        float x_out;
        float x_in;

        if (py2 > out2) {
            continue;
        }
        x_out = sqrtf(out2 - py2);
        x_in = py2 < in2 ? sqrtf(in2 - py2) : 0.0f;
        for (int side = 0; side < 2; side++) {
            /* The annulus crosses this row in [-x_out, -x_in] and [x_in, x_out]. */
            bool whole = x_in <= 0.0f; /* the row misses the inner circle: one span */
            float lo = side == 0 ? -x_out : x_in;
            float hi = whole ? x_out : side == 0 ? -x_in : x_out;
            int32_t x0 = LV_MAX(0, (int32_t)floorf(lo + (float)dial->cx - (float)area->x1) - 1);
            int32_t x1 = LV_MIN(w - 1, (int32_t)ceilf(hi + (float)dial->cx - (float)area->x1) + 1);

            for (int32_t x = x0; x <= x1; x++) {
                float px = (float)(area->x1 + x) + 0.5f - (float)dial->cx;
                float d;

                if (!in_sector(&sec, px, py)) {
                    continue;
                }
                d = sqrtf(px * px + py2);
                for (int i = 0; i < KIT_DIAL_RINGS; i++) {
                    const kit_dial_ring_t *ring = &dial->rings[i];
                    float hw = (float)ring->width / 2.0f;
                    float dist;
                    float cover;

                    if (ring->width <= 0 || fabsf(d - (float)ring->r) > hw + 6.0f) {
                        continue;
                    }
                    dist = ring->end_deg - ring->start_deg >= 360
                               ? fabsf(d - (float)ring->r)
                               : ring_distance(ring, px, py, d, norm_deg(atan2f(py, px) / DEG_TO_RAD));
                    cover = LV_CLAMP(0.0f, hw - dist + 0.5f, 1.0f);
                    if (cover > 0.0f) {
                        over_px(&row[x], ring->color, cover * (float)ring->opa / 255.0f);
                        any = true;
                    }
                }
            }
            if (whole) {
                break;
            }
        }
    }

    /* Ticks, each within its own bounding box. */
    for (int set = 0; set < KIT_DIAL_TICK_SETS; set++) {
        const kit_dial_ticks_t *tk = &dial->ticks[set];
        float step;

        if (tk->count <= 0) {
            continue;
        }
        step = 360.0f / (float)tk->count;
        for (int32_t i = 0; i < tk->count; i++) {
            float ang = norm_deg(tk->offset_deg + step * (float)i);
            bool major = tk->major_every > 0 && i % tk->major_every == 0;
            float r0 = (float)(major ? tk->r_in_major : tk->r_in);
            float r1 = (float)(major ? tk->r_out_major : tk->r_out);
            float hw = (float)(major ? tk->width_major : tk->width) / 2.0f;
            float c = cosf(ang * DEG_TO_RAD);
            float sn = sinf(ang * DEG_TO_RAD);
            float x0 = r0 * c;
            float y0 = r0 * sn;
            float x1 = r1 * c;
            float y1 = r1 * sn;
            int32_t bx0;
            int32_t bx1;
            int32_t by0;
            int32_t by1;

            if (i < 64 && ((tk->skip >> i) & 1u) != 0) {
                continue;
            }
            /* Ticks whose box misses this tile cost nothing below (empty loops). */
            bx0 = LV_MAX(0, (int32_t)floorf(LV_MIN(x0, x1) - hw - 1.0f + (float)dial->cx) - area->x1);
            bx1 = LV_MIN(w - 1, (int32_t)ceilf(LV_MAX(x0, x1) + hw + 1.0f + (float)dial->cx) - area->x1);
            by0 = LV_MAX(0, (int32_t)floorf(LV_MIN(y0, y1) - hw - 1.0f + (float)dial->cy) - area->y1);
            by1 = LV_MIN(h - 1, (int32_t)ceilf(LV_MAX(y0, y1) + hw + 1.0f + (float)dial->cy) - area->y1);
            for (int32_t y = by0; y <= by1; y++) {
                lv_color32_t *row = (lv_color32_t *)(buf->data + (size_t)y * buf->header.stride);
                float py = (float)(area->y1 + y) + 0.5f - (float)dial->cy;

                for (int32_t x = bx0; x <= bx1; x++) {
                    float px = (float)(area->x1 + x) + 0.5f - (float)dial->cx;
                    float cover;

                    if (!in_sector(&sec, px, py)) {
                        continue;
                    }
                    cover = LV_CLAMP(0.0f, hw - segment_distance(px, py, x0, y0, x1, y1) + 0.5f, 1.0f);
                    if (cover > 0.0f) {
                        over_px(&row[x], major ? tk->color_major : tk->color, cover * (float)tk->opa / 255.0f);
                        any = true;
                    }
                }
            }
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
    float r_min;
    float r_max;
    int count;
    float step;

    dial_extent(dial, &r_min, &r_max);
    /* Small rings (a key's lit edge) need few tiles; every tile is a draw task. */
    count = r_max < 120.0f ? 4 : r_max - r_min < 40.0f ? KIT_DIAL_TILES : 16;
    step = 360.0f / (float)count;
    for (int t = 0; t < count; t++) {
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

/* Dials are cached by shape: the key has the centre at 0,0 and the tiles are moved to the
 * real centre when drawn, so the same ring on several keys is built once. */
static const dial_tiles_t *dial_get(const kit_dial_t *dial_in)
{
    dial_entry_t *slot = NULL;
    kit_dial_t key = *dial_in;
    const kit_dial_t *dial = &key;

    key.cx = 0;
    key.cy = 0;

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

static void dial_draw_opa(lv_layer_t *layer, const kit_dial_t *dial, lv_opa_t opa)
{
    const dial_tiles_t *tiles = dial_get(dial);
    lv_draw_image_dsc_t img;
    lv_area_t area;

    lv_draw_image_dsc_init(&img);
    img.opa = opa;
    for (int t = 0; t < KIT_DIAL_TILES; t++) {
        if (tiles->buf[t] != NULL) {
            img.src = tiles->buf[t];
            area = tiles->area[t];
            lv_area_move(&area, dial->cx, dial->cy);
            lv_draw_image(layer, &img, &area);
        }
    }
}

void kit_dial_draw(lv_layer_t *layer, const kit_dial_t *dial)
{
    dial_draw_opa(layer, dial, LV_OPA_COVER);
}

void kit_dial_prewarm(const kit_dial_t *dial)
{
    (void)dial_get(dial);
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
    int32_t span = LV_MIN(end_deg - start_deg, 360);

    /* The ring code wants the start in 0..359 and the end after it; this also makes equal
     * arcs share a cache entry. */
    start_deg = ((start_deg % 360) + 360) % 360;
    end_deg = start_deg + span;
    memset(&dial, 0, sizeof(dial));
    dial.cx = cx;
    dial.cy = cy;
    dial.rings[0] = (kit_dial_ring_t) {
        .r = radius,
        .width = width,
        .start_deg = start_deg,
        .end_deg = end_deg,
        .color = color,
        .opa = LV_OPA_COVER,
    };
    /* Opacity applies when drawing, so a fading ring does not build a dial per step. */
    if (opa > LV_OPA_MIN) {
        dial_draw_opa(layer, &dial, opa);
    }
}

/* --- Live arcs ----------------------------------------------------------------------------- */

void kit_draw_arc(lv_layer_t *layer, const lv_draw_arc_dsc_t *dsc)
{
    lv_area_t circle;
    lv_area_t bounds;
    lv_area_t clip;
    lv_area_t layer_clip = layer->_clip_area;
    lv_draw_task_t *t;

    if (dsc->opa <= LV_OPA_MIN || dsc->width == 0 || dsc->start_angle == dsc->end_angle) {
        return;
    }
    /* lv_draw_arc gives the task the whole circle as its area: the software renderer then
     * masks every row of it and the dispatcher treats the arc as covering the screen, so
     * nothing drawn after it runs in parallel. The renderer needs the circle for its masks,
     * so keep that as the area but clip and register the task by the arc's own bounds. */
    lv_draw_arc_get_area(dsc->center.x, dsc->center.y, dsc->radius, dsc->start_angle, dsc->end_angle, dsc->width,
                         dsc->rounded, &bounds);
    if (!lv_area_intersect(&clip, &layer_clip, &bounds)) {
        return;
    }
    circle.x1 = dsc->center.x - dsc->radius;
    circle.y1 = dsc->center.y - dsc->radius;
    circle.x2 = dsc->center.x + dsc->radius - 1;
    circle.y2 = dsc->center.y + dsc->radius - 1;
    layer->_clip_area = clip;
    t = lv_draw_add_task(layer, &circle, LV_DRAW_TASK_TYPE_ARC);
    layer->_clip_area = layer_clip;
    t->_real_area = clip;
    lv_memcpy(t->draw_dsc, dsc, sizeof(*dsc));
    lv_draw_finalize_task_creation(layer, t);
}

/* --- Baked lv_arc widgets -------------------------------------------------------------------- */

typedef struct {
    int32_t width[2];
    lv_color_t color[2];
    lv_opa_t opa[2];
} baked_arc_t;

static void baked_arc_ring(lv_layer_t *layer, const lv_area_t *c, int32_t radius, const baked_arc_t *b, int part,
                           int32_t start, int32_t end)
{
    if (b->opa[part] <= LV_OPA_MIN || b->width[part] <= 0 || start == end) {
        return;
    }
    while (end <= start) {
        end += 360;
    }
    kit_draw_ring(layer, c->x1 + lv_area_get_width(c) / 2, c->y1 + lv_area_get_height(c) / 2,
                  radius - b->width[part] / 2, b->width[part], start, end, b->color[part], b->opa[part]);
}

static void baked_arc_cb(lv_event_t *e)
{
    lv_obj_t *arc = lv_event_get_current_target(e);
    baked_arc_t *b = lv_event_get_user_data(e);
    lv_area_t c;
    int32_t rot;
    int32_t radius;

    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        lv_free(b);
        return;
    }
    lv_obj_get_content_coords(arc, &c);
    radius = LV_MIN(lv_area_get_width(&c), lv_area_get_height(&c)) / 2;
    rot = lv_arc_get_rotation(arc);
    baked_arc_ring(lv_event_get_layer(e), &c, radius, b, 0, rot + (int32_t)lv_arc_get_bg_angle_start(arc),
                   rot + (int32_t)lv_arc_get_bg_angle_end(arc));
    baked_arc_ring(lv_event_get_layer(e), &c, radius, b, 1, rot + (int32_t)lv_arc_get_angle_start(arc),
                   rot + (int32_t)lv_arc_get_angle_end(arc));
}

void kit_arc_bake(lv_obj_t *arc)
{
    static const lv_part_t parts[2] = {LV_PART_MAIN, LV_PART_INDICATOR};
    baked_arc_t *b = lv_malloc_zeroed(sizeof(*b));

    if (b == NULL) {
        return;
    }
    for (int i = 0; i < 2; i++) {
        b->width[i] = lv_obj_get_style_arc_width(arc, parts[i]);
        b->color[i] = lv_obj_get_style_arc_color(arc, parts[i]);
        b->opa[i] = lv_obj_get_style_arc_opa(arc, parts[i]);
        lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, parts[i]);
    }
    lv_obj_add_event_cb(arc, baked_arc_cb, LV_EVENT_DRAW_MAIN_END, b);
    lv_obj_add_event_cb(arc, baked_arc_cb, LV_EVENT_DELETE, b);
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
