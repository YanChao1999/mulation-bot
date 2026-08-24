/*
 * Intentionally weak tests: the suite is green, but a mutant of
 * `>=` -> `>` at is_adult() survives because nobody checks the boundary 18.
 */
#if defined(MULATION_USE_GTEST)
#include <gtest/gtest.h>
#else
#include "mini_gtest.h"
#endif

#include "compare.h"

TEST(Adult, TwentyIsAdult) {
    EXPECT_TRUE(is_adult(20));
}

TEST(Max, Unequal) {
    EXPECT_EQ(max2(5, 3), 5);
}

/* Strong enough to kill `+` -> `-`. */
TEST(Add, TwoPlusTwo) {
    EXPECT_EQ(add2(2, 2), 4);
}

/*
 * Uncomment to kill the surviving `>=` -> `>` mutant:
 *   TEST(Adult, Boundary) {
 *     EXPECT_TRUE(is_adult(18));
 *     EXPECT_FALSE(is_adult(17));
 *   }
 */
