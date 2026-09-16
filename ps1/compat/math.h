#ifndef PS1_COMPAT_MATH_H
#define PS1_COMPAT_MATH_H
#include <stdint.h>
#define M_PI 3.14159265358979323846
#define INFINITY (1.0f/0.0f)
#define NAN (0.0f/0.0f)
static inline float sqrtf(float x);
static inline float sinf(float x);
static inline float cosf(float x);
static inline float atan2f(float y, float x);
static inline float ceilf(float x);
static inline int ps1_isnanf(float x) { return x != x; }
static inline int ps1_isinff(float x) { return x == INFINITY || x == -INFINITY; }
static inline double floor(double x) { int64_t i=(int64_t)x; if(x<0.0&&x!=(double)i)--i; return (double)i; }
static inline double fabs(double x) { return x<0.0?-x:x; }
static inline double sqrt(double x) { return (double)sqrtf((float)x); }
static inline double sin(double x) { return (double)sinf((float)x); }
static inline double cos(double x) { return (double)cosf((float)x); }
static inline double atan2(double y,double x) { return (double)atan2f((float)y,(float)x); }
static inline double round(double x) { return x>=0.0?floor(x+0.5):ceil(x-0.5); }
static inline double fmod(double x,double y) { return y==0.0?NAN:x-floor(x/y)*y; }
static inline double fabsf_dummy(double x) { return x<0.0?-x:x; }
static inline float fabsf(float x) { return x < 0.0f ? -x : x; }
static inline float sqrtf(float x) {
    if (x <= 0.0f) return x == 0.0f ? 0.0f : NAN;
    float g = x > 1.0f ? x : 1.0f;
    for (int i = 0; i < 12; ++i) g = 0.5f * (g + x / g);
    return g;
}
static inline float ps1_wrap_pi(float x) {
    const float p = 3.14159265358979323846f, t = 6.28318530717958647692f;
    while (x > p) x -= t;
    while (x < -p) x += t;
    return x;
}
static inline float sinf(float x) {
    x = ps1_wrap_pi(x); float x2=x*x;
    return x*(1.0f-x2*(1.0f/6.0f)+x2*x2*(1.0f/120.0f)-x2*x2*x2*(1.0f/5040.0f)+x2*x2*x2*x2*(1.0f/362880.0f));
}
static inline float cosf(float x) { return sinf(x + 1.57079632679489661923f); }
static inline float tanf(float x) { float c=cosf(x); return c==0.0f ? (sinf(x)>=0.0f?INFINITY:-INFINITY) : sinf(x)/c; }
static inline float atanf(float x) {
    const float p=3.14159265358979323846f, h=1.57079632679489661923f;
    float a=fabsf(x);
    if(a>1.0f) return (x<0.0f?-h:h)-atanf(1.0f/x);
    return x*(p/4.0f+0.273f*(1.0f-a));
}
static inline float atan2f(float y,float x) {
    if(x>0.0f)return atanf(y/x);
    if(x<0.0f)return y>=0.0f?atanf(y/x)+3.14159265358979323846f:atanf(y/x)-3.14159265358979323846f;
    if(y>0.0f)return 1.57079632679489661923f;
    if(y<0.0f)return -1.57079632679489661923f;
    return 0.0f;
}
static inline float asinf(float x) { if(x>=1.0f)return 1.57079632679489661923f; if(x<=-1.0f)return -1.57079632679489661923f; return atan2f(x,sqrtf(1.0f-x*x)); }
static inline float acosf(float x) { return 1.57079632679489661923f-asinf(x); }
static inline float floorf(float x) { int32_t i=(int32_t)x; if(x<0.0f&&x!=(float)i)--i; return (float)i; }
static inline float ceilf(float x) { int32_t i=(int32_t)x; if(x>0.0f&&x!=(float)i)++i; return (float)i; }
static inline float roundf(float x) { return x>=0.0f?floorf(x+0.5f):ceilf(x-0.5f); }
static inline float fmodf(float x,float y) { return y==0.0f?NAN:x-floorf(x/y)*y; }
static inline float logf(float x) {
    if(x<=0.0f)return -INFINITY; int e=0;
    while(x>2.0f){x*=0.5f;++e;} while(x<1.0f){x*=2.0f;--e;}
    float z=(x-1.0f)/(x+1.0f),z2=z*z,p=z,s=z;
    for(int k=3;k<=15;k+=2){p*=z2;s+=p/(float)k;}
    return 2.0f*s+(float)e*0.6931471805599453f;
}
static inline float log2f(float x) { return logf(x)*1.4426950408889634f; }
static inline float log10f(float x) { return logf(x)*0.4342944819032518f; }
static inline float expf(float x) {
    if(x>88.0f)return INFINITY; if(x<-88.0f)return 0.0f;
    int n=(int)floorf(x/0.6931471805599453f); float r=x-(float)n*0.6931471805599453f,term=1.0f,sum=1.0f;
    for(int i=1;i<=12;++i){term*=r/(float)i;sum+=term;}
    while(n>0){sum*=2.0f;--n;} while(n<0){sum*=0.5f;++n;} return sum;
}
static inline float powf(float x,float y) {
    if(x==0.0f)return y>0.0f?0.0f:INFINITY;
    if(x<0.0f){int32_t iy=(int32_t)y;if((float)iy!=y)return NAN;float r=expf(logf(-x)*y);return (iy&1)?-r:r;}
    return expf(logf(x)*y);
}
static inline float fminf(float a,float b){return a<b?a:b;}
static inline float fmaxf(float a,float b){return a>b?a:b;}
static inline float nextafterf(float x,float y){if(x==y)return y;union{float f;uint32_t u;}a;a.f=x;if(x==0.0f){a.u=y>0.0f?1u:0x80000001u;return a.f;}if((y>x)==(x>0.0f))++a.u;else--a.u;return a.f;}
static inline long lround(double x){return x>=0.0?(long)(x+0.5):(long)(x-0.5);}
#define isnan(x) ps1_isnanf((float)(x))
#define isinf(x) ps1_isinff((float)(x))
#endif
