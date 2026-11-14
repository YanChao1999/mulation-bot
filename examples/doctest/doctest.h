#ifndef MULATION_MINI_DOCTEST_H
#define MULATION_MINI_DOCTEST_H

/*
 * Minimal doctest-compatible harness for the mulation example.
 * Supports TEST_CASE / CHECK / CHECK_EQ enough to demo mutation gating
 * without vendoring the full doctest single-header.
 */

#include <iostream>
#include <vector>

struct DoctestCase {
    const char *name;
    void (*fn)();
};

inline std::vector<DoctestCase> &doctest_cases() {
    static std::vector<DoctestCase> t;
    return t;
}

inline int &doctest_failures() {
    static int f;
    return f;
}

struct DoctestAdd {
    DoctestAdd(const char *name, void (*fn)()) { doctest_cases().push_back({name, fn}); }
};

#define DOCTEST_CAT_IMPL(a, b) a##b
#define DOCTEST_CAT(a, b) DOCTEST_CAT_IMPL(a, b)

#define TEST_CASE(name)                                                                            \
    static void DOCTEST_CAT(DOCTEST_ANON_FUNC_, __LINE__)();                                        \
    static DoctestAdd DOCTEST_CAT(_doctest_add_, __LINE__)(                                         \
        name, DOCTEST_CAT(DOCTEST_ANON_FUNC_, __LINE__));                                           \
    static void DOCTEST_CAT(DOCTEST_ANON_FUNC_, __LINE__)()

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #x "\n";                \
            ++doctest_failures();                                                                  \
        }                                                                                          \
    } while (0)

#define CHECK_EQ(a, b)                                                                             \
    do {                                                                                           \
        const auto _ea = (a);                                                                      \
        const auto _eb = (b);                                                                      \
        if (!((_ea) == (_eb))) {                                                                   \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK_EQ failed: " #a " == " #b "\n";   \
            ++doctest_failures();                                                                  \
        }                                                                                          \
    } while (0)

#define CHECK_FALSE(x) CHECK(!(x))

inline int doctest_main(int /*argc*/, char ** /*argv*/) {
    int failed = 0;
    for (const DoctestCase &t : doctest_cases()) {
        const int before = doctest_failures();
        std::cout << "[doctest] " << t.name << "\n";
        t.fn();
        if (doctest_failures() > before) {
            ++failed;
        }
    }
    if (failed) {
        std::cout << "[doctest] " << failed << " test case(s) failed\n";
        return 1;
    }
    std::cout << "[doctest] all " << doctest_cases().size() << " test case(s) passed\n";
    return 0;
}

#ifndef DOCTEST_CONFIG_NO_MAIN
int main(int argc, char **argv) {
    return doctest_main(argc, argv);
}
#endif

#endif
