#include "domain/face_catalog.h"

typedef struct {
    bool enabled;
    const char *name;
} face_manifest_entry_t;

static const face_manifest_entry_t s_face_manifest[CLOCK_FACE_COUNT] = {
    [CLOCK_FACE_DIGITAL] = {
        .enabled = true,
        .name = "Digital",
    },
    [CLOCK_FACE_MATRIX] = {
        .enabled = true,
        .name = "Matrix",
    },
    [CLOCK_FACE_WHARTON] = {
        .enabled = true,
        .name = "Wharton",
    },
    [CLOCK_FACE_SLAVA] = {
        .enabled = CLOCK_FACE_ENABLE_SLAVA,
        .name = "Slava",
    },
    [CLOCK_FACE_SLAVA_DARK] = {
        .enabled = CLOCK_FACE_ENABLE_SLAVA_DARK,
        .name = "Slava Dark",
    },
    [CLOCK_FACE_STERNGLAS] = {
        .enabled = true,
        .name = "Sternglas",
    },
    [CLOCK_FACE_AVENIR] = {
        .enabled = true,
        .name = "Avenir",
    },
    [CLOCK_FACE_MODERN_SILVER] = {
        .enabled = true,
        .name = "Modern Silver",
    },
};

bool face_catalog_is_valid(int face)
{
    return face >= 0 && face < CLOCK_FACE_COUNT;
}

bool face_catalog_is_enabled(clock_face_id_t face)
{
    if (!face_catalog_is_valid(face)) {
        return false;
    }

    return s_face_manifest[face].enabled;
}

clock_face_id_t face_catalog_default_face(void)
{
    return CLOCK_FACE_DIGITAL;
}

clock_face_id_t face_catalog_first_enabled(void)
{
    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        if (face_catalog_is_enabled((clock_face_id_t)face)) {
            return (clock_face_id_t)face;
        }
    }

    return face_catalog_default_face();
}

int face_catalog_visible_count(void)
{
    int count = 0;

    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        if (face_catalog_is_enabled((clock_face_id_t)face)) {
            ++count;
        }
    }

    return count;
}

clock_face_id_t face_catalog_visible_index_to_id(int visible_index)
{
    int count = 0;

    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        if (!face_catalog_is_enabled((clock_face_id_t)face)) {
            continue;
        }
        if (count == visible_index) {
            return (clock_face_id_t)face;
        }
        ++count;
    }

    return face_catalog_first_enabled();
}

int face_catalog_visible_id_to_index(clock_face_id_t face)
{
    int count = 0;

    for (int current = 0; current < CLOCK_FACE_COUNT; ++current) {
        if (!face_catalog_is_enabled((clock_face_id_t)current)) {
            continue;
        }
        if (current == (int)face) {
            return count;
        }
        ++count;
    }

    return -1;
}

clock_face_id_t face_catalog_step_enabled(clock_face_id_t face, int direction)
{
    int visible_count = face_catalog_visible_count();
    int index;

    if (visible_count <= 0) {
        return face_catalog_default_face();
    }

    if (!face_catalog_is_enabled(face)) {
        return face_catalog_first_enabled();
    }

    index = face_catalog_visible_id_to_index(face);
    if (index < 0) {
        return face_catalog_first_enabled();
    }

    index += (direction >= 0) ? 1 : -1;
    if (index < 0) {
        index = visible_count - 1;
    } else if (index >= visible_count) {
        index = 0;
    }

    return face_catalog_visible_index_to_id(index);
}

const char *face_catalog_name(clock_face_id_t face)
{
    if (!face_catalog_is_valid(face)) {
        return "Unknown";
    }

    return s_face_manifest[face].name;
}
