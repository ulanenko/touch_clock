#include "localization.h"
#include "test_support.h"

static int test_runtime_statuses(void)
{
    char out[64];

    l10n_runtime_status(CLOCK_LANGUAGE_POLISH, "Scanning...", out, sizeof(out));
    EXPECT_STR_EQ("Skanowanie...", out);
    l10n_runtime_status(CLOCK_LANGUAGE_ENGLISH, "Scanning...", out, sizeof(out));
    EXPECT_STR_EQ("Scanning...", out);

    /* A status that carries a network's name keeps the name. */
    l10n_runtime_status(CLOCK_LANGUAGE_DUTCH, "Connecting to Home 5G", out, sizeof(out));
    EXPECT_STR_EQ("Verbinden met Home 5G", out);
    l10n_runtime_status(CLOCK_LANGUAGE_UKRAINIAN, "Connected: %d", out, sizeof(out));
    EXPECT_STR_EQ("Підключено: %d", out);
    l10n_runtime_status(CLOCK_LANGUAGE_POLISH, "Connected: ", out, sizeof(out));
    EXPECT_STR_EQ("Połączono: ", out);

    /* One the table lacks stays as it came; a short buffer is cut, terminated. */
    l10n_runtime_status(CLOCK_LANGUAGE_RUSSIAN, "Something new", out, sizeof(out));
    EXPECT_STR_EQ("Something new", out);
    l10n_runtime_status(CLOCK_LANGUAGE_ENGLISH, "Connecting to Home", out, 8);
    EXPECT_STR_EQ("Connect", out);
    l10n_runtime_status(CLOCK_LANGUAGE_ENGLISH, NULL, out, sizeof(out));
    EXPECT_STR_EQ("", out);
    return 0;
}

int main(void)
{
    return test_runtime_statuses();
}
