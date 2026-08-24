#ifndef COMPARE_H
#define COMPARE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Production SUT: adult if age >= 18. */
int is_adult(int age);

/* Returns the larger of a and b. */
int max2(int a, int b);

int add2(int a, int b);

#ifdef __cplusplus
}
#endif

#endif
