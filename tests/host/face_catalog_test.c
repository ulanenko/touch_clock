#include "domain/face_catalog.h"
#include "test_support.h"

int main(void)
{
    static const clock_face_id_t s_expected_faces[] = {
        CLOCK_FACE_DIGITAL,
        CLOCK_FACE_MATRIX,
        CLOCK_FACE_WHARTON,
        CLOCK_FACE_STERNGLAS,
        CLOCK_FACE_AVENIR,
        CLOCK_FACE_MODERN_SILVER,
    };
    int visible_count;

    test_use_utc();

    EXPECT_TRUE(face_catalog_is_valid(face_catalog_default_face()));
    EXPECT_TRUE(face_catalog_is_enabled(face_catalog_default_face()));
    EXPECT_EQ_INT(CLOCK_FACE_DIGITAL, face_catalog_default_face());

    visible_count = face_catalog_visible_count();
    EXPECT_EQ_INT((int)(sizeof(s_expected_faces) / sizeof(s_expected_faces[0])), visible_count);

    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = face_catalog_visible_index_to_id(index);

        EXPECT_EQ_INT(s_expected_faces[index], face);
        EXPECT_TRUE(face_catalog_is_enabled(face));
        EXPECT_EQ_INT(index, face_catalog_visible_id_to_index(face));
        EXPECT_TRUE(face_catalog_name(face) != NULL);
    }

    EXPECT_FALSE(face_catalog_is_enabled(CLOCK_FACE_SLAVA));
    EXPECT_FALSE(face_catalog_is_enabled(CLOCK_FACE_SLAVA_DARK));

    EXPECT_EQ_INT(face_catalog_visible_index_to_id(0),
                  face_catalog_step_enabled(face_catalog_visible_index_to_id(visible_count - 1), 1));
    EXPECT_EQ_INT(face_catalog_visible_index_to_id(visible_count - 1),
                  face_catalog_step_enabled(face_catalog_visible_index_to_id(0), -1));

    return 0;
}
