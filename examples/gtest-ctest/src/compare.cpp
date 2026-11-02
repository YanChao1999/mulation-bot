#include "compare.h"

/* Weak contract on purpose: boundary age 18 is untested in compare_test.cpp. */
int is_adult(int age) {
    return age >= 18;
}

int max2(int a, int b) {
    return a > b ? a : b;
}

int add2(int a, int b) {
    return a + b;
}
