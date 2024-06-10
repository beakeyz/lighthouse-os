#include "math.h"

int abs(int j)
{
    return (j < 0 ? -j : j);
}

double fabs(double x)
{
    return __builtin_fabs(x);
}

double exp(double x);
double fmod(double x, double y);
double sqrt(double x);
float sqrtf(float x);
float fabsf(float x);
double sin(double x);
double cos(double x);

double floor(double x);
double ceil(double x);
