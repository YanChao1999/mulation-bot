#include "mulation/mute_lines.hpp"
#include "unit_check.hpp"

#include <string>

TEST(MuteLines, SkipOnlyThisLine) {
    const char *src = "int a = 1 + 2;\n"
                      "int b = 3 + 4; // mulation:skip\n"
                      "int c = 5 + 6;\n";
    auto mute = mulation_mute_mask(src);
    CHECK(!mulation_line_muted(mute, 1));
    CHECK(mulation_line_muted(mute, 2));
    CHECK(!mulation_line_muted(mute, 3));
}

TEST(MuteLines, OffOnRegion) {
    const char *src = "int a = 1 + 2;\n"
                      "// mulation:off\n"
                      "int b = 3 + 4;\n"
                      "int c = 5 + 6;\n"
                      "// mulation:on\n"
                      "int d = 7 + 8;\n";
    auto mute = mulation_mute_mask(src);
    CHECK(!mulation_line_muted(mute, 1));
    CHECK(mulation_line_muted(mute, 2));
    CHECK(mulation_line_muted(mute, 3));
    CHECK(mulation_line_muted(mute, 4));
    CHECK(mulation_line_muted(mute, 5));
    CHECK(!mulation_line_muted(mute, 6));
}

TEST(MuteLines, OffAndOnSameLine) {
    const char *src = "int a = 1 + 2; /* mulation:off mulation:on */\n"
                      "int b = 3 + 4;\n";
    auto mute = mulation_mute_mask(src);
    CHECK(mulation_line_muted(mute, 1));
    CHECK(!mulation_line_muted(mute, 2));
}
