#pragma once

/*
 * Draw buffers freed after the frame that may still read them.
 *
 * A cache that drops an image while a frame is being drawn (a draw callback making room for
 * another) can't free it at once: draw tasks queued earlier in the frame, or running on the
 * other core, may still read it. These wait for the display's next REFR_READY, when every
 * task of the frame is done.
 */

#include <stddef.h>

#include "lvgl.h"

/* Frees buf once the frame being drawn is done (at the next frame's end when none is). It is
 * forgotten as a sparse image and a moving glass row then too. NULL is ignored. */
void ui_frame_free(lv_draw_buf_t *buf);

/* Frees the waiting buffers now: only where no draw task can read them (between frames, after
 * lv_draw_wait_for_finish). */
void ui_frame_free_now(void);

/* The bytes waiting: memory a cache can count as about to be free. */
size_t ui_frame_free_waiting_bytes(void);
