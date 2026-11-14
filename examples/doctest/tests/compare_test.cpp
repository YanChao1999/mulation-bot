/*
 * Intentionally weak tests: the suite is green, but a mutant of
 * `>=` -> `>` at is_adult() survives because nobody checks the boundary 18.
 */
#include "doctest.h"

#include "compare.h"

TEST_CASE("Adult TwentyIsAdult") {
    CHECK(is_adult(20));
}

TEST_CASE("Max Unequal") {
    CHECK_EQ(max2(5, 3), 5);
}

/* Kills `+` -> `-`, but `+` -> `*` still survives (2*2 == 4). */
TEST_CASE("Add TwoPlusTwo") {
    CHECK_EQ(add2(2, 2), 4);
}
