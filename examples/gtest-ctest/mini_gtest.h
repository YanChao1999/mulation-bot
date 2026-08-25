#ifndef MINI_GTEST_H
#define MINI_GTEST_H

#include <iostream>
#include <vector>

struct MiniTest {
    const char *name;
    void (*fn)();
};

inline std::vector<MiniTest> &mini_tests() {
    static std::vector<MiniTest> t;
    return t;
}

inline int &mini_failures() {
    static int f;
    return f;
}

struct MiniAdd {
    MiniAdd(const char *name, void (*fn)()) { mini_tests().push_back({name, fn}); }
};

#define TEST(suite, name)                                                                          \
    static void test_##suite##_##name();                                                           \
    static MiniAdd _add_##suite##_##name(#suite "." #name, test_##suite##_##name);                 \
    static void test_##suite##_##name()

#define EXPECT_TRUE(x)                                                                             \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __FILE__ << ":" << __LINE__ << ": Failure\nExpected true: " #x "\n";      \
            ++mini_failures();                                                                     \
        }                                                                                          \
    } while (0)

#define EXPECT_FALSE(x) EXPECT_TRUE(!(x))

#define EXPECT_EQ(a, b)                                                                            \
    do {                                                                                           \
        const auto _ea = (a);                                                                      \
        const auto _eb = (b);                                                                      \
        if (!((_ea) == (_eb))) {                                                                   \
            std::cerr << __FILE__ << ":" << __LINE__ << ": Failure\nExpected equality of " #a      \
                      << " and " #b << "\n";                                                       \
            ++mini_failures();                                                                     \
        }                                                                                          \
    } while (0)

inline int mini_gtest_main() {
    int failed_tests = 0;
    for (const MiniTest &t : mini_tests()) {
        const int before = mini_failures();
        std::cout << "[ RUN      ] " << t.name << "\n";
        t.fn();
        if (mini_failures() > before) {
            std::cout << "[  FAILED  ] " << t.name << "\n";
            ++failed_tests;
        } else {
            std::cout << "[       OK ] " << t.name << "\n";
        }
    }
    if (failed_tests) {
        std::cout << "[  FAILED  ] " << failed_tests << " test(s)\n";
        return 1;
    }
    std::cout << "[  PASSED  ] " << mini_tests().size() << " test(s)\n";
    return 0;
}

#ifndef MINI_GTEST_NO_MAIN
int main() {
    return mini_gtest_main();
}
#endif

#endif
