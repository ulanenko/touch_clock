#pragma once

#include "domain/clock_types.h"

bool face_catalog_is_valid(int face);
bool face_catalog_is_enabled(clock_face_id_t face);
clock_face_id_t face_catalog_default_face(void);
clock_face_id_t face_catalog_first_enabled(void);
clock_face_id_t face_catalog_visible_index_to_id(int visible_index);
int face_catalog_visible_id_to_index(clock_face_id_t face);
int face_catalog_visible_count(void);
clock_face_id_t face_catalog_step_enabled(clock_face_id_t face, int direction);
const char *face_catalog_name(clock_face_id_t face);
