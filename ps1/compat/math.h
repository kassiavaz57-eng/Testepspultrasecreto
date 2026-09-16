#ifndef PS1_COMPAT_MATH_H
#define PS1_COMPAT_MATH_H
#include <stdint.h>
#define M_PI 3.14159265358979323846
#define INFINITY (1.0f/0.0f)
#define NAN (0.0f/0.0f)
int ps1_isnanf(float x);
int ps1_isinff(float x);
float sqrtf(float x);
float sinf(float x);
float cosf(float x);
float tanf(float x);
float asinf(float x);
float acosf(float x);
float atanf(float x);
float atan2f(float y, float x);
float fabsf(float x);
float fmodf(float x, float y);
float floorf(float x);
float ceilf(float x);
float roundf(float x);
float powf(float x, float y);
float logf(float x);
float log2f(float x);
float log10f(float x);
float expf(float x);
float fminf(float a, float b);
float fmaxf(float a, float b);
float nextafterf(float x, float y);
long lround(double x);
#define isnan(x) ps1_isnanf((float)(x))
#define isinf(x) ps1_isinff((float)(x))
#endif
