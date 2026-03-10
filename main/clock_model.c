#include "clock_model.h"

bool clock_face_is_valid(int face)
{
    return face >= 0 && face < CLOCK_FACE_COUNT;
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
    default:
        return "Unknown";
    }
}
