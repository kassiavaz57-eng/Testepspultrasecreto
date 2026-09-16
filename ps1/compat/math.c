#include "math.h"
#include <stdint.h>

static const float PI = 3.14159265358979323846f;
static const float HALF_PI = 1.57079632679489661923f;
static const float TWO_PI = 6.28318530717958647692f;
static float ps1_abs(float x) { return x < 0.0f ? -x : x; }

int ps1_isnanf(float x) { return x != x; }
int ps1_isinff(float x) { return x == INFINITY || x == -INFINITY; }
float fabsf(float x) { return ps1_abs(x); }

float sqrtf(float x) {
    if (x <= 0.0f) return x == 0.0f ? 0.0f : NAN;
    float g = x > 1.0f ? x : 1.0f;
    int i;
    for (i = 0; i < 10; ++i) g = 0.5f * (g + x / g);
    return g;
}

static float wrap_pi(float x) {
    while (x > PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;
    return x;
}

float sinf(float x) {
    x = wrap_pi(x);
    float x2 = x * x;
    return x * (1.0f - x2 * (1.0f/6.0f) + x2*x2*(1.0f/120.0f) - x2*x2*x2*(1.0f/5040.0f) + x2*x2*x2*x2*(1.0f/362880.0f));
}

float cosf(float x) {
    return sinf(x + HALF_PI);
}

float tanf(float x) {
    float c = cosf(x);
    return c == 0.0f ? (sinf(x) >= 0.0f ? INFINITY : -INFINITY) : sinf(x) / c;
}

float atanf(float x) {
    float ax = ps1_abs(x);
    if (ax > 1.0f) return (x < 0.0f ? -HALF_PI : HALF_PI) - atanf(1.0f / x);
    return x * (PI/4.0f + 0.273f * (1.0f - ax));
}

float atan2f(float y, float x) {
    if (x > 0.0f) return atanf(y / x);
    if (x < 0.0f) return y >= 0.0f ? atanf(y / x) + PI : atanf(y / x) - PI;
    if (y > 0.0f) return HALF_PI;
    if (y < 0.0f) return -HALF_PI;
    return 0.0f;
}

float asinf(float x) {
    if (x >= 1.0f) return HALF_PI;
    if (x <= -1.0f) return -HALF_PI;
    return atan2f(x, sqrtf(1.0f - x*x));
}

float acosf(float x) { return HALF_PI - asinf(x); }

float floorf(float x) {
    int32_t i = (int32_t)x;
    if (x < 0.0f && x != (float)i) --i;
    return (float)i;
}

float ceilf(float x) {
    int32_t i = (int32_t)x;
    if (x > 0.0f && x != (float)i) ++i;
    return (float)i;
}

float roundf(float x) { return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f); }
float fmodf(float x, float y) {
    if (y == 0.0f) return NAN;
    return x - floorf(x / y) * y;
}
float fminf(float a, float b) { return a < b ? a : b; }
float fmaxf(float a, float b) { return a > b ? a : b; }

float logf(float x) {
    if (x <= 0.0f) return -INFINITY;
    int e = 0;
    while (x > 2.0f) { x *= 0.5f; ++e; }
    while (x < 1.0f) { x *= 2.0f; --e; }
    float z = (x - 1.0f) / (x + 1.0f);
    float z2 = z*z;
    float s = z;
    float p = z;
    int k;
    for (k = 3; k <= 15; k += 2) { p *= z2; s += p / (float)k; }
    return 2.0f*s + (float)e*0.6931471805599453f;
}
float log2f(float x) { return logf(x) * 1.4426950408889634f; }
float log10f(float x) { return logf(x) * 0.4342944819032518f; }

float expf(float x) {
    if (x > 88.0f) return INFINITY;
    if (x < -88.0f) return 0.0f;
    int n = (int)floorf(x / 0.6931471805599453f);
    float r = x - (float)n * 0.6931471805599453f;
    float term = 1.0f, sum = 1.0f;
    int i;
    for (i = 1; i <= 12; ++i) {
        term *= r / (float)i;
        sum += term;
    }
    while (n > 0) { sum *= 2.0f; --n; }
    while (n < 0) { sum *= 0.5f; ++n; }
    return sum;
}
float powf(float x, float y) {
    if (x == 0.0f) return y > 0.0f ? 0.0f : INFINITY;
    if (x < 0.0f) {
        int32_t iy = (int32_t)y;
        if ((float)iy != y) return NAN;
        float r = expf(logf(-x) * y);
        return (iy & 1) ? -r : r;
    }
    return expf(logf(x) * y);
}
float nextafterf(float x, float y) {
    if (x == y) return y;
    union { float f; uint32_t u; } a;
    a.f = x;
    if (x == 0.0f) { a.u = y > 0.0f ? 1u : 0x80000001u; return a.f; }
    if ((y > x) == (x > 0.0f)) ++a.u; else --a.u;
    return a.f;
}
long lround(double x) { return x >= 0.0 ? (long)(x + 0.5) : (long)(x - 0.5); }
