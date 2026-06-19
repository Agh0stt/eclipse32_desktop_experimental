#define KMATH_PI   3.14159265358979323846f
#define KMATH_2PI  6.28318530717958647692f
#define KMATH_PI2  1.57079632679489661923f

static float kmath_reduce(float x) {
    while (x >  KMATH_PI)  x -= KMATH_2PI;
    while (x < -KMATH_PI)  x += KMATH_2PI;
    return x;
}

float sinf(float x) {
    x = kmath_reduce(x);
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    float x7 = x5 * x2;
    float x9 = x7 * x2;
    return x - x3*0.16666666667f + x5*0.00833333333f
             - x7*0.00019841269f + x9*0.00000275573f;
}

float cosf(float x) { return sinf(x + KMATH_PI2); }
