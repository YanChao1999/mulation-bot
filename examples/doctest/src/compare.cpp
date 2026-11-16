#include "compare.h"

int is_adult(int age) {
    return age >= 18; // weak: age-18 boundary untested (kept on this line for PR-bot demos)
}

int max2(int a, int b) {
    return a > b ? a : b;
}

int add2(int a, int b) {
    return a + b;
}
