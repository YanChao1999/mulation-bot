#include "cli.hpp"
#include "test_filter.hpp"
#include "unit_check.hpp"

#include <string>
#include <vector>

TEST(TestFilter, DetectCTestAndZephyr) {
    CHECK_EQ(detect_framework({"ctest", "--test-dir", "build"}, "auto"), std::string("ctest"));
    CHECK_EQ(detect_framework({"./build/zephyr/zephyr.exe"}, "auto"), std::string("ztest"));
    CHECK_EQ(detect_framework({"./my_tests"}, "auto"), std::string("gtest"));
    CHECK_EQ(detect_framework({"./my_tests"}, "doctest"), std::string("doctest"));
}

TEST(TestFilter, NativeArgs) {
    auto g = native_test_filter_args("gtest", "Adult.*");
    CHECK_EQ(g.size(), 1u);
    CHECK_EQ(g[0], std::string("--gtest_filter=Adult.*"));

    auto c = native_test_filter_args("ctest", "compare");
    CHECK_EQ(c.size(), 2u);
    CHECK_EQ(c[0], std::string("-R"));
    CHECK_EQ(c[1], std::string("compare"));

    auto d = native_test_filter_args("doctest", "*Adult*");
    CHECK_EQ(d.size(), 1u);
    CHECK_EQ(d[0], std::string("-tc=*Adult*"));

    auto z = native_test_filter_args("ztest", "compare::adult");
    CHECK_EQ(z.size(), 1u);
    CHECK_EQ(z[0], std::string("-test=compare::adult"));

    CHECK(native_test_filter_args("custom", "x").empty());
}

TEST(TestFilter, ApplyAppendsGTestFilter) {
    Options opt;
    opt.cmd = {"./suite"};
    opt.test_filter = "Foo.*";
    opt.framework = "auto";
    std::string err;
    CHECK(apply_test_filter(opt, &err));
    CHECK_EQ(opt.resolved_framework, std::string("gtest"));
    CHECK_EQ(opt.cmd.size(), 2u);
    CHECK_EQ(opt.cmd[1], std::string("--gtest_filter=Foo.*"));
}

TEST(TestFilter, ApplyCustomLeavesCmd) {
    Options opt;
    opt.cmd = {"./harness"};
    opt.test_filter = "only";
    opt.framework = "custom";
    std::string err;
    CHECK(apply_test_filter(opt, &err));
    CHECK_EQ(opt.cmd.size(), 1u);
    CHECK_EQ(opt.resolved_framework, std::string("custom"));
}

TEST(TestFilter, UnknownFrameworkErrors) {
    Options opt;
    opt.cmd = {"./t"};
    opt.test_filter = "x";
    opt.framework = "nope";
    std::string err;
    CHECK(!apply_test_filter(opt, &err));
    CHECK(!err.empty());
}
