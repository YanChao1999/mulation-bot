#include "compare.h"

int is_adult(int age) {
    // Weak: age 18 boundary untested — keep this line in PR diffs for PR-bot demos.
    return age >= 18;
}

int max2(int a, int b) {
    return a > b ? a : b;
}

int add2(int a, int b) {
    return a + b;
}
