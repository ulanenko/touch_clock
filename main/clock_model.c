#include "clock_model.h"

static const bool s_face_enabled[CLOCK_FACE_COUNT] = {
    [CLOCK_FACE_DIGITAL] = true,
    [CLOCK_FACE_MATRIX] = true,
    [CLOCK_FACE_WHARTON] = true,
    [CLOCK_FACE_SLAVA] = CLOCK_FACE_ENABLE_SLAVA,
    [CLOCK_FACE_SLAVA_DARK] = CLOCK_FACE_ENABLE_SLAVA_DARK,
    [CLOCK_FACE_STERNGLAS] = true,
    [CLOCK_FACE_AVENIR] = true,
    [CLOCK_FACE_MODERN_SILVER] = true,
};

bool clock_face_is_valid(int face)
{
    return face >= 0 && face < CLOCK_FACE_COUNT;
}

bool clock_face_is_enabled(clock_face_id_t face)
{
    if (!clock_face_is_valid(face)) {
        return false;
    }

    return s_face_enabled[face];
}

clock_face_id_t clock_face_first_enabled(void)
{
    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        if (clock_face_is_enabled((clock_face_id_t)face)) {
            return (clock_face_id_t)face;
        }
    }

    return CLOCK_FACE_DIGITAL;
}

int clock_face_visible_count(void)
{
    int count = 0;

    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        if (clock_face_is_enabled((clock_face_id_t)face)) {
            ++count;
        }
    }

    return count;
}

clock_face_id_t clock_face_visible_index_to_id(int visible_index)
{
    int count = 0;

    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        if (!clock_face_is_enabled((clock_face_id_t)face)) {
            continue;
        }
        if (count == visible_index) {
            return (clock_face_id_t)face;
        }
        ++count;
    }

    return clock_face_first_enabled();
}

int clock_face_visible_id_to_index(clock_face_id_t face)
{
    int count = 0;

    for (int current = 0; current < CLOCK_FACE_COUNT; ++current) {
        if (!clock_face_is_enabled((clock_face_id_t)current)) {
            continue;
        }
        if (current == face) {
            return count;
        }
        ++count;
    }

    return -1;
}

clock_face_id_t clock_face_step_enabled(clock_face_id_t face, int direction)
{
    int visible_count = clock_face_visible_count();
    int index;

    if (visible_count <= 0) {
        return CLOCK_FACE_DIGITAL;
    }

    if (!clock_face_is_enabled(face)) {
        return clock_face_first_enabled();
    }

    index = clock_face_visible_id_to_index(face);
    if (index < 0) {
        return clock_face_first_enabled();
    }

    index += (direction >= 0) ? 1 : -1;
    if (index < 0) {
        index = visible_count - 1;
    } else if (index >= visible_count) {
        index = 0;
    }

    return clock_face_visible_index_to_id(index);
}

const char *clock_face_name(clock_face_id_t face)
{
    switch (face) {
    case CLOCK_FACE_DIGITAL:
        return "Digital";
    case CLOCK_FACE_MATRIX:
        return "Matrix";
    case CLOCK_FACE_WHARTON:
        return "Wharton";
    case CLOCK_FACE_SLAVA:
        return "Slava";
    case CLOCK_FACE_SLAVA_DARK:
        return "Slava Dark";
    case CLOCK_FACE_STERNGLAS:
        return "Sternglas";
    case CLOCK_FACE_AVENIR:
        return "Avenir";
    case CLOCK_FACE_MODERN_SILVER:
        return "Modern Silver";
    default:
        return "Unknown";
    }
}
