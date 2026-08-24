#ifndef MULATION_UNIT_CHECK_HPP
#define MULATION_UNIT_CHECK_HPP

#include <iostream>
#include <string>
#include <vector>

struct UnitTest {
    const char *name;
    void (*fn)();
};

inline std::vector<UnitTest> &unit_tests() {
    static std::vector<UnitTest> t;
    return t;
}

inline int &unit_failures() {
    static int f;
    return f;
}

struct UnitAdd {
    UnitAdd(const char *name, void (*fn)()) { unit_tests().push_back({name, fn}); }
};

#define TEST(suite, name)                                                                          \
    static void test_##suite##_##name();                                                           \
    static UnitAdd _add_##suite##_##name(#suite "." #name, test_##suite##_##name);                 \
    static void test_##suite##_##name()

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n";              \
            ++unit_failures();                                                                     \
        }                                                                                          \
    } while (0)

#define CHECK_EQ(a, b)                                                                             \
    do {                                                                                           \
        const auto _ea = (a);                                                                      \
        const auto _eb = (b);                                                                      \
        if (!((_ea) == (_eb))) {                                                                   \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK_EQ failed: " #a " vs " #b "\n";    \
            ++unit_failures();                                                                     \
        }                                                                                          \
    } while (0)

inline int unit_main() {
    int failed = 0;
    for (const UnitTest &t : unit_tests()) {
        const int before = unit_failures();
        std::cout << "[ RUN      ] " << t.name << "\n";
        t.fn();
        if (unit_failures() > before) {
            std::cout << "[  FAILED  ] " << t.name << "\n";
            ++failed;
        } else {
            std::cout << "[       OK ] " << t.name << "\n";
        }
    }
    if (failed) {
        std::cout << "[  FAILED  ] " << failed << " test(s)\n";
        return 1;
    }
    std::cout << "[  PASSED  ] " << unit_tests().size() << " test(s)\n";
    return 0;
}

#endif
