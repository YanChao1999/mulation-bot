#include "compare.h"

int is_adult(int age)
{
	/* Weak contract demo: boundary 18 often untested in ZTEST. */
	return age >= 18;
}

int max2(int a, int b)
{
	return a > b ? a : b;
}

int add2(int a, int b)
{
	return a + b;
}
