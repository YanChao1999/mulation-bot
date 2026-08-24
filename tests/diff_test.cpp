#include "diff.hpp"
#include "unit_check.hpp"

TEST(Diff, ParseUnifiedHunks) {
    const char *text = "diff --git a/src/foo.cpp b/src/foo.cpp\n"
                       "--- a/src/foo.cpp\n"
                       "+++ b/src/foo.cpp\n"
                       "@@ -10,0 +11,2 @@ int foo()\n"
                       "+  return x;\n"
                       "+}\n"
                       "@@ -40 +42 @@ void bar()\n"
                       "-old\n"
                       "+new\n";
    auto ranges = parse_git_diff_text(text);
    CHECK_EQ(ranges.size(), 2u);
    CHECK_EQ(ranges[0].file, std::string("src/foo.cpp"));
    CHECK_EQ(ranges[0].start, 11u);
    CHECK_EQ(ranges[0].end, 12u);
    CHECK_EQ(ranges[1].start, 42u);
    CHECK_EQ(ranges[1].end, 42u);
}

TEST(Diff, ParseSkipsZeroCount) {
    auto ranges = parse_git_diff_text("+++ b/a.cpp\n@@ -3,2 +3,0 @@\n");
    CHECK(ranges.empty());
}

TEST(Diff, FilterByLineAndPathSuffix) {
    Mutant in_range{1, "/home/me/src/foo.cpp", 11, 0, "ROR", ">=", ">"};
    Mutant out_of_range{2, "/home/me/src/foo.cpp", 99, 0, "ROR", ">", ">="};
    Mutant other_file{3, "/home/me/src/bar.cpp", 11, 0, "AOR", "+", "-"};
    LineRange r{"src/foo.cpp", 11, 12};
    auto kept = filter_by_diff({in_range, out_of_range, other_file}, {r});
    CHECK_EQ(kept.size(), 1u);
    CHECK_EQ(kept[0].id, 1u);
}

TEST(Diff, FilterExactPath) {
    Mutant m{9, "src/foo.cpp", 5, 0, "LCR", "&&", "||"};
    LineRange r{"src/foo.cpp", 5, 5};
    auto kept = filter_by_diff({m}, {r});
    CHECK_EQ(kept.size(), 1u);
}

TEST(Diff, FilterIgnoresUnrelatedSameBasename) {
    Mutant other{3, "/home/me/other/foo.cpp", 11, 0, "AOR", "+", "-"};
    LineRange r{"src/foo.cpp", 11, 12};
    auto kept = filter_by_diff({other}, {r});
    CHECK(kept.empty());
}

TEST(Diff, FilterRequiresDirectoryBoundary) {
    Mutant not_foo{4, "/home/me/src/notfoo.cpp", 11, 0, "ROR", ">=", ">"};
    LineRange r{"foo.cpp", 11, 12};
    auto kept = filter_by_diff({not_foo}, {r});
    CHECK(kept.empty());
}

TEST(Diff, GitDiffRejectsOptionLikeRevision) {
    std::vector<LineRange> ranges;
    std::string err;
    CHECK(!git_diff_ranges("-c", ranges, &err));
    CHECK(ranges.empty());
    CHECK(!err.empty());
}

TEST(Diff, GitDiffFailsClosedOnUnknownRevision) {
    std::vector<LineRange> ranges;
    std::string err;
    CHECK(!git_diff_ranges("this-revision-does-not-exist-mulation", ranges, &err));
    CHECK(ranges.empty());
    CHECK(!err.empty());
}
