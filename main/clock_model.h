#pragma once

#include "domain/clock_types.h"

bool clock_face_is_valid(int face);
bool clock_face_is_enabled(clock_face_id_t face);
clock_face_id_t clock_face_first_enabled(void);
clock_face_id_t clock_face_visible_index_to_id(int visible_index);
int clock_face_visible_id_to_index(clock_face_id_t face);
int clock_face_visible_count(void);
clock_face_id_t clock_face_step_enabled(clock_face_id_t face, int direction);
const char *clock_face_name(clock_face_id_t face);
