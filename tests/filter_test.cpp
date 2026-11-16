#include "catalog.hpp"
#include "filter.hpp"
#include "unit_check.hpp"

#include <string>
#include <vector>

TEST(Filter, EmptyKeepsAll) {
    Mutant a{1, "src/a.cpp", 1, 0, "ROR", ">", ">="};
    Mutant b{2, "src/b.cpp", 2, 0, "AOR", "+", "-"};
    auto kept = filter_mutants({a, b}, {});
    CHECK_EQ(kept.size(), 2u);
}

TEST(Filter, FilterOrExclude) {
    Mutant a{1, "src/foo.cpp", 1, 0, "ROR", ">", ">="};
    Mutant b{2, "src/bar.cpp", 2, 0, "AOR", "+", "-"};
    Mutant c{3, "third_party/x.cpp", 3, 0, "LVR", "0", "1"};
    MutantFilter f;
    f.filter = {"src/"};
    f.exclude = {"bar"};
    auto kept = filter_mutants({a, b, c}, f);
    CHECK_EQ(kept.size(), 1u);
    CHECK_EQ(kept[0].id, 1u);
}

TEST(Filter, KindsCaseInsensitive) {
    Mutant a{1, "a.cpp", 1, 0, "ROR", ">", ">="};
    Mutant b{2, "b.cpp", 2, 0, "AOR", "+", "-"};
    MutantFilter f;
    f.kinds = {"ror"};
    auto kept = filter_mutants({a, b}, f);
    CHECK_EQ(kept.size(), 1u);
    CHECK_EQ(kept[0].id, 1u);
}

TEST(Filter, MutantIds) {
    Mutant a{10, "a.cpp", 1, 0, "ROR", ">", ">="};
    Mutant b{20, "b.cpp", 2, 0, "AOR", "+", "-"};
    MutantFilter f;
    f.mutant_ids = {20};
    auto kept = filter_mutants({a, b}, f);
    CHECK_EQ(kept.size(), 1u);
    CHECK_EQ(kept[0].id, 20u);
}
