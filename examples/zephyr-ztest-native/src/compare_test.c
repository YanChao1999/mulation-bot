/*
 * Intentionally weak ZTEST suite (same story as examples/gtest-ctest).
 * Green + coverage can still miss age 18; mulation should report survivors.
 */
#include <zephyr/ztest.h>

#include "compare.h"

ZTEST_SUITE(compare, NULL, NULL, NULL, NULL, NULL);

ZTEST(compare, adult_twenty) {
    zassert_true(is_adult(20), "20 should be adult");
}

ZTEST(compare, max_unequal) {
    zassert_equal(max2(5, 3), 5, "max2");
}

ZTEST(compare, add_two_plus_two) {
    /* Kills + -> -; + -> * may still survive (2*2 == 4). */
    zassert_equal(add2(2, 2), 4, "add2");
}

/*
 * Uncomment to kill surviving boundary / arithmetic mutants:
 *
 * ZTEST(compare, adult_boundary)
 * {
 *         zassert_true(is_adult(18), "18 inclusive");
 *         zassert_false(is_adult(17), "17 not adult");
 * }
 *
 * ZTEST(compare, add_three_plus_one)
 * {
 *         zassert_equal(add2(3, 1), 4, "kills + -> *");
 * }
 */
