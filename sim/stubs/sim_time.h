#pragma once

/* Force-included into the UI sources so wall-clock reads come from the simulator. */
#include <time.h>

time_t sim_time(time_t *out);
#define time(out) sim_time(out)
