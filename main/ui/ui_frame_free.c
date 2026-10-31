#include "ui/ui_frame_free.h"

#include "ui/clock_ui_private.h"

void ui_moving_row_forget(const void *data);

static lv_draw_buf_t **s_waiting;
static uint32_t s_count;
static uint32_t s_cap;
static size_t s_bytes;
static bool s_hooked;

static void destroy(lv_draw_buf_t *buf)
{
    ui_sparse_image_unregister(buf);
    ui_moving_row_forget(buf->data);
    lv_draw_buf_destroy(buf);
}

void ui_frame_free_now(void)
{
    for (uint32_t i = 0; i < s_count; i++) {
        destroy(s_waiting[i]);
    }
    s_count = 0;
    s_bytes = 0;
}

static void refr_ready_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_frame_free_now();
}

void ui_frame_free(lv_draw_buf_t *buf)
{
    if (buf == NULL) {
        return;
    }
    if (!s_hooked && lv_display_get_default() != NULL) {
        lv_display_add_event_cb(lv_display_get_default(), refr_ready_cb, LV_EVENT_REFR_READY, NULL);
        s_hooked = true;
    }
    if (s_count == s_cap) {
        uint32_t cap = s_cap != 0 ? s_cap * 2 : 16;
        lv_draw_buf_t **grown = lv_realloc(s_waiting, cap * sizeof(*grown));

        if (grown == NULL) {
            /* No room to wait: let the frame's running tasks finish, and free it now. */
            lv_draw_wait_for_finish();
            destroy(buf);
            return;
        }
        s_waiting = grown;
        s_cap = cap;
    }
    s_waiting[s_count++] = buf;
    s_bytes += buf->data_size;
}

size_t ui_frame_free_waiting_bytes(void)
{
    return s_bytes;
}
