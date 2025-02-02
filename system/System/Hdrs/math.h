#ifndef __LIGHTOS_LIBC_MATH__
#define __LIGHTOS_LIBC_MATH__

#define M_PI 3.14159265358979323846
#define M_E 2.7182818284590452354
#define NAN (__builtin_nanf(""))
#define INFINITY (__builtin_inff())

extern double floor(double x);
extern double ceil(double x);

extern int abs(int j);
extern double pow(double x, double y);
extern double exp(double x);
extern double fmod(double x, double y);
extern double sqrt(double x);
extern float sqrtf(float x);
extern double fabs(double x);
extern float fabsf(float x);
extern double sin(double x);
extern double cos(double x);
extern double tan(double x);
extern float sinf(float x);
extern float cosf(float x);
extern float tanf(float x);
extern float atan2f(float a, float b);

extern float roundf(float a);
extern double round(double a);
extern long double roundl(long double arg);

#endif // !__LIGHTOS_LIBC_MATH__
