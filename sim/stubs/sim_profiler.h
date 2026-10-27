#pragma once

/* LVGL profiler hooks for the simulator: counts draw tasks (see sim_main.c "tasks"). */
void sim_prof_mark(const char *func);

#undef LV_PROFILER_BEGIN
#undef LV_PROFILER_END
#undef LV_PROFILER_BEGIN_TAG
#undef LV_PROFILER_END_TAG

#define LV_PROFILER_BEGIN sim_prof_mark(__func__)
#define LV_PROFILER_END
#define LV_PROFILER_BEGIN_TAG(tag) sim_prof_mark(tag)
#define LV_PROFILER_END_TAG(tag) ((void)(tag))
