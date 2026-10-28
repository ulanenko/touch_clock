#pragma once
/* Synthetic scenes for calibrating the simulator's cost model; see ui_microbench.c. */
#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

int ui_microbench_count(void);
const char *ui_microbench_name(int index);
/* Shows scene index full-screen on the top layer (redrawn every frame if asked). */
lv_obj_t *ui_microbench_show(int index, bool redraw_every_frame);
void ui_microbench_hide(void);
/* Times the per-pixel CPU loop (scene "cpu_loop"), in microseconds. */
int64_t ui_microbench_cpu_loop(int64_t (*now_us)(void));
