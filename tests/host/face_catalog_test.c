#include "domain/face_catalog.h"
#include "test_support.h"

int main(void)
{
    int visible_count;

    test_use_utc();

    EXPECT_TRUE(face_catalog_is_valid(face_catalog_default_face()));
    EXPECT_TRUE(face_catalog_is_enabled(face_catalog_default_face()));
    EXPECT_EQ_INT(CLOCK_FACE_DIGITAL, face_catalog_default_face());

    visible_count = face_catalog_visible_count();
    EXPECT_TRUE(visible_count > 0);

    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = face_catalog_visible_index_to_id(index);

        EXPECT_TRUE(face_catalog_is_enabled(face));
        EXPECT_EQ_INT(index, face_catalog_visible_id_to_index(face));
        EXPECT_TRUE(face_catalog_name(face) != NULL);
    }

    EXPECT_EQ_INT(face_catalog_visible_index_to_id(0),
                  face_catalog_step_enabled(face_catalog_visible_index_to_id(visible_count - 1), 1));
    EXPECT_EQ_INT(face_catalog_visible_index_to_id(visible_count - 1),
                  face_catalog_step_enabled(face_catalog_visible_index_to_id(0), -1));

    return 0;
}
