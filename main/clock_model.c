#include "clock_model.h"

#include "domain/face_catalog.h"

bool clock_face_is_valid(int face)
{
    return face_catalog_is_valid(face);
}

bool clock_face_is_enabled(clock_face_id_t face)
{
    return face_catalog_is_enabled(face);
}

clock_face_id_t clock_face_first_enabled(void)
{
    return face_catalog_first_enabled();
}

int clock_face_visible_count(void)
{
    return face_catalog_visible_count();
}

clock_face_id_t clock_face_visible_index_to_id(int visible_index)
{
    return face_catalog_visible_index_to_id(visible_index);
}

int clock_face_visible_id_to_index(clock_face_id_t face)
{
    return face_catalog_visible_id_to_index(face);
}

clock_face_id_t clock_face_step_enabled(clock_face_id_t face, int direction)
{
    return face_catalog_step_enabled(face, direction);
}

const char *clock_face_name(clock_face_id_t face)
{
    return face_catalog_name(face);
}
