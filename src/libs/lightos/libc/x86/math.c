#include <math.h>

/*
 * X86_64 pow() implementation
 */
double pow(double x, double y)
{
    double out;
    asm volatile(
        "fyl2x;"
        "fld %%st;"
        "frndint;"
        "fsub %%st,%%st(1);"
        "fxch;"
        "fchs;"
        "f2xm1;"
        "fld1;"
        "faddp;"
        "fxch;"
        "fld1;"
        "fscale;"
        "fstp %%st(1);"
        "fmulp;" : "=t"(out) : "0"(x), "u"(y) : "st(1)");
    return out;
}

double sqrt(double x)
{
    return __builtin_sqrt(x);
}
