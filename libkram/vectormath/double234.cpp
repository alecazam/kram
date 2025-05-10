// kram - Copyright 2020-2025 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.
#include "vectormath234.h"

// This has to include this, not double234.h
#if USE_SIMDLIB && SIMD_DOUBLE

#if SIMD_ACCELERATE_MATH
// TODO: reduce this header to just calls use (f.e. geometry, etc)
#include <simd/simd.h>
#endif // SIMD_ACCELERATE_MATH

namespace SIMD_NAMESPACE {

// clang-format off

#if SIMD_ACCELERATE_MATH
// These will get inlined here from the template
macroVectorRepeatFnImpl(double, log)
macroVectorRepeatFnImpl(double, exp)

macroVectorRepeatFnImpl(double, sin)
macroVectorRepeatFnImpl(double, cos)
macroVectorRepeatFnImpl(double, tan)

macroVectorRepeatFnImpl(double, asin)
macroVectorRepeatFnImpl(double, acos)
macroVectorRepeatFnImpl(double, atan)

macroVectorRepeatFn2Impl(double, atan2)

#endif // SIMD_ACCELERATE_MATH

#if SIMD_CMATH_MATH
macroVectorRepeatFnImpl(double, log, ::log)
macroVectorRepeatFnImpl(double, exp, ::exp)

macroVectorRepeatFnImpl(double, sin, ::sin)
macroVectorRepeatFnImpl(double, cos, ::cos)
macroVectorRepeatFnImpl(double, tan, ::tan)

macroVectorRepeatFnImpl(double, asin, ::asin)
macroVectorRepeatFnImpl(double, acos, ::acos)
macroVectorRepeatFnImpl(double, atan, ::atan)

#endif // SIMD_CMATH_MATH

    // clang-format on

    //---------------------------

    static const double2 kdouble2_posx = {1.0f, 0.0f};
static const double2 kdouble2_posy = kdouble2_posx.yx;

static const double2 kdouble2_negx = {-1.0f, 0.0f};
static const double2 kdouble2_negy = kdouble2_negx.yx;

static const double2 kdouble2_ones = kdouble2_posx.xx;
static const double2 kdouble2_zero = {};

//----

static const double3 kdouble3_posx = {1.0f, 0.0f, 0.0f};
static const double3 kdouble3_posy = kdouble3_posx.yxy;
static const double3 kdouble3_posz = kdouble3_posx.yyx;

static const double3 kdouble3_negx = {-1.0f, 0.0f, 0.0f};
static const double3 kdouble3_negy = kdouble3_negx.yxy;
static const double3 kdouble3_negz = kdouble3_negx.yyx;

static const double3 kdouble3_ones = kdouble3_posx.xxx;
static const double3 kdouble3_zero = {};

//----

static const double4 kdouble4_posx = {1.0f, 0.0f, 0.0f, 0.0f};
static const double4 kdouble4_posy = kdouble4_posx.yxyy;
static const double4 kdouble4_posz = kdouble4_posx.yyxy;
static const double4 kdouble4_posw = kdouble4_posx.yyyx;

static const double4 kdouble4_negxw = {-1.0f, 0.0f, 0.0f, 1.0f};
static const double4 kdouble4_negyw = kdouble4_negxw.yxyw;
static const double4 kdouble4_negzw = kdouble4_negxw.yyxw;

static const double4 kdouble4_posxw = {1.0f, 0.0f, 0.0f, 1.0f};
static const double4 kdouble4_posyw = kdouble4_posxw.yxyw;
static const double4 kdouble4_poszw = kdouble4_posxw.yyxw;

static const double4 kdouble4_negx = {-1.0f, 0.0f, 0.0f, 0.0f};
static const double4 kdouble4_negy = kdouble4_negx.yxyy;
static const double4 kdouble4_negz = kdouble4_negx.yyxy;
static const double4 kdouble4_negw = kdouble4_negx.yyyx;

static const double4 kdouble4_ones = kdouble4_posx.xxxx;
static const double4 kdouble4_zero = {};

//---------------------------

static const double2x2 kdouble2x2_zero = {}; // what is this value 0, or default ctor
static const double3x3 kdouble3x3_zero = {};
static const double3x4 kdouble3x4_zero = {};
static const double4x4 kdouble4x4_zero = {};

static const double2x2 kdouble2x2_identity = diagonal_matrix((double2)1);
static const double3x3 kdouble3x3_identity = diagonal_matrix((double3)1);
static const double3x4 kdouble3x4_identity = diagonal_matrix3x4((double3)1);
static const double4x4 kdouble4x4_identity = diagonal_matrix((double4)1);

//----

const double2& double2_zero() { return kdouble2_zero; }
const double2& double2_ones() { return kdouble2_ones; }

const double2& double2_posx() { return kdouble2_posx; }
const double2& double2_posy() { return kdouble2_posy; }

const double2& double2_negx() { return kdouble2_negx; }
const double2& double2_negy() { return kdouble2_negy; }

//----

const double3& double3_zero() { return kdouble3_zero; }
const double3& double3_ones() { return kdouble3_ones; }

const double3& double3_posx() { return kdouble3_posx; }
const double3& double3_posy() { return kdouble3_posy; }
const double3& double3_posz() { return kdouble3_posz; }

const double3& double3_negx() { return kdouble3_negx; }
const double3& double3_negy() { return kdouble3_negy; }
const double3& double3_negz() { return kdouble3_negz; }

//----

const double4& double4_zero() { return kdouble4_zero; }
const double4& double4_ones() { return kdouble4_ones; }

const double4& double4_posx() { return kdouble4_posx; }
const double4& double4_posy() { return kdouble4_posy; }
const double4& double4_posz() { return kdouble4_posz; }
const double4& double4_posw() { return kdouble4_posw; }

const double4& double4_negx() { return kdouble4_negx; }
const double4& double4_negy() { return kdouble4_negy; }
const double4& double4_negz() { return kdouble4_negz; }
const double4& double4_negw() { return kdouble4_negw; }

const double4& double4_posxw() { return kdouble4_posxw; }
const double4& double4_posyw() { return kdouble4_posyw; }
const double4& double4_poszw() { return kdouble4_poszw; }

const double4& double4_negxw() { return kdouble4_negxw; }
const double4& double4_negyw() { return kdouble4_negyw; }
const double4& double4_negzw() { return kdouble4_negzw; }

//---------------------------

const double2x2& double2x2::zero() { return kdouble2x2_zero; }
const double2x2& double2x2::identity() { return kdouble2x2_identity; }

const double3x3& double3x3::zero() { return kdouble3x3_zero; }
const double3x3& double3x3::identity() { return kdouble3x3_identity; }

const double3x4& double3x4::zero() { return kdouble3x4_zero; }
const double3x4& double3x4::identity() { return kdouble3x4_identity; }

const double4x4& double4x4::zero() { return kdouble4x4_zero; }
const double4x4& double4x4::identity() { return kdouble4x4_identity; }

//---------------------------

// These should not be used often.  So can stay buried
double2x2::double2x2(double2 diag)
    : base((const base&)diagonal_matrix(diag)) {}
double3x3::double3x3(double3 diag)
    : base((const base&)diagonal_matrix(diag)) {}
double3x4::double3x4(double3 diag)
    : base((const base&)diagonal_matrix3x4(diag)) {}
double4x4::double4x4(double4 diag)
    : base((const base&)diagonal_matrix(diag)) {}

//---------------------------

double2x2 diagonal_matrix(double2 x)
{
    double4 xx = zeroext(x);
    return double2x2(xx.xw, xx.wy);
}
double3x3 diagonal_matrix(double3 x)
{
    double4 xx = zeroext(x);
    return double3x3(xx.xww, xx.wyw, xx.wwz);
}
double3x4 diagonal_matrix3x4(double3 x)
{
    double4 xx = zeroext(x);
    return double3x4(xx.xwww, xx.wyww, xx.wwzw);
}
double4x4 diagonal_matrix(double4 x)
{
    double4 xx = x;
    xx.w = 0.0f;
    double4 ww = xx;
    ww.z = x.w;
    return double4x4(xx.xwww, xx.wyww, xx.wwzw, ww.wwwz);
}

//---------------------------

// textbook transpose
double2x2 transpose(const double2x2& x)
{
    // std::swap would seem faster here?
#if SIMD_SSE
#if SIMD_AVX2
    double4 x0, x1;
    x0.xy = x[0];
    x1.xy = x[1];

    double4 r01 = _mm256_unpacklo_pd(x0, x1);
    return (double2x2){r01.lo, r01.hi};
#else
    double2 x0, x1;
    x0.xy = x[0];
    x1.xy = x[1];

    // super slow transpose
    double2 r0 = {x0[0], x1[0]};
    double2 r1 = {x0[1], x1[1]};
    return (double2x2){r0, r1};
#endif
#endif // SIMD_SSE

#if SIMD_NEON
    double2 r0 = vzip1q_f64(x[0], x[1]);
    double2 r1 = vzip2q_f64(x[0], x[1]);
    return (double2x2){r0, r1};
#endif // SIMD_NEON
}

double3x3 transpose(const double3x3& x)
{
    double4 x0, x1, x2;
    x0.xyz = x[0];
    x1.xyz = x[1];
    x2.xyz = x[2];

#if SIMD_SSE
#if SIMD_AVX2 && 0
    double4 t0 = _mm256_unpacklo_pd(x0, x1);
    double4 t1 = _mm256_unpackhi_pd(x0, x1);

    double4 r0 = t0;
    r0.hi = x2.lo;
    // TODO: fix shuffle,  222 outside 15 range
    // looks like op was changed to 4-bit bitmask
    // lookup shuffle 4 values, and convert this
    //
    // 0xde = _MM_SHUFFLE(x,y,z,w)
    // #define _MM_SHUFFLE(fp3, fp2, fp1, fp0) \
        (((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))
    // fp0 to fp3 = 2, 3, 1, 3

    double4 r1 = _mm256_shuffle_pd(t0, x2, 0xde);
    double4 r2 = x2;
    r2.lo = t1.lo;
#else
    // super slow transpose
    double3 r0 = {x0[0], x1[0], x2[0]};
    double3 r1 = {x0[1], x1[1], x2[1]};
    double3 r2 = {x0[2], x1[2], x2[2]};
#endif
#endif // SIMD_SSE

#if SIMD_NEON
    double2 padding = {0};
    double4 r0, r1, r2;
    r0.lo = vzip1q_f64(x0.lo, x1.lo);
    r1.lo = vzip2q_f64(x0.lo, x1.lo);
    r2.lo = vzip1q_f64(x0.hi, x1.hi);
    r0.hi = vzip1q_f64(x2.lo, padding);
    r1.hi = vzip2q_f64(x2.lo, padding);
    r2.hi = vzip1q_f64(x2.hi, padding);
#endif // SIMD_NEON
    return (double3x3){r0.xyz, r1.xyz, r2.xyz};
}

double4x4 transpose(const double4x4& x)
{
#if SIMD_SSE
#if SIMD_AVX2

    // NOTE: similar to _MM_TRANSPOSE4_PS using shuffles
    // but old Neon didn't really have shuffle.

    // using shuffles + permute
    // unpack runs slower
    double4 tmp0, tmp1, tmp2, tmp3;
    tmp0 = _mm256_shuffle_pd(x[0], x[1], 0x0);
    tmp2 = _mm256_shuffle_pd(x[0], x[1], 0xF);
    tmp1 = _mm256_shuffle_pd(x[2], x[3], 0x0);
    tmp3 = _mm256_shuffle_pd(x[2], x[3], 0xF);

    double4 r0, r1, r2, r3;
    r0 = _mm256_permute2f128_pd(tmp0, tmp1, 0x20);
    r1 = _mm256_permute2f128_pd(tmp2, tmp3, 0x20);
    r2 = _mm256_permute2f128_pd(tmp0, tmp1, 0x31);
    r3 = _mm256_permute2f128_pd(tmp2, tmp3, 0x31);

#else
    // super slow transpose
    double4 x0, x1, x2, x3;
    x0 = x[0];
    x1 = x[1];
    x2 = x[2];
    x3 = x[3];

    double4 r0 = {x0[0], x1[0], x2[0], x3[0]};
    double4 r1 = {x0[1], x1[1], x2[1], x3[1]};
    double4 r2 = {x0[2], x1[2], x2[2], x3[2]};
    double4 r3 = {x0[3], x1[3], x2[3], x3[3]};
#endif
#endif // SIMD_SSE

#if SIMD_NEON
    double4 r0, r1, r2, r3;
    r0.lo = vzip1q_f64(x[0].lo, x[1].lo);
    r1.lo = vzip2q_f64(x[0].lo, x[1].lo);
    r2.lo = vzip1q_f64(x[0].hi, x[1].hi);
    r3.lo = vzip2q_f64(x[0].hi, x[1].hi);
    r0.hi = vzip1q_f64(x[2].lo, x[3].lo);
    r1.hi = vzip2q_f64(x[2].lo, x[3].lo);
    r2.hi = vzip1q_f64(x[2].hi, x[3].hi);
    r3.hi = vzip2q_f64(x[2].hi, x[3].hi);
#endif
    return (double4x4){r0, r1, r2, r3};
}

// inverse
double2x2 inverse(const double2x2& x)
{
    double invDet = 1.0f / determinant(x);
    if (invDet == 0.0f) return kdouble2x2_zero;

    double2x2 r = transpose(x);
    r[0] *= invDet;
    r[1] *= invDet;
    return r;
}

double3x3 inverse(const double3x3& x)
{
    double invDet = 1.0f / determinant(x);
    if (invDet == 0.0f) return kdouble3x3_zero;

    double3x3 r;

    // this forms the adjoint
    r[0] = cross(x[1], x[2]) * invDet;
    r[1] = cross(x[2], x[0]) * invDet;
    r[2] = cross(x[0], x[1]) * invDet;
    return r;
}

// std::swap has warning on aligned data
inline void swap(double4& a, double4& b)
{
    double4 temp = a;
    a = b;
    b = temp;
}

double4x4 inverse(const double4x4& x)
{
    // This is a full gje inverse

    double4x4 a(x), b(kdouble4x4_identity);
    bool inversionSucceeded = true;

    // As a evolves from original mat into identity -
    // b evolves from identity into inverse(a)
    int cols = double4x4::col;
    int rows = double4x4::row;

    // Loop over cols of a from left to right, eliminating above and below diag
    for (int j = 0; j < rows; j++) {
        // Find largest pivot in column j among rows j..2
        int i1 = j; // Row with largest pivot candidate
        for (int i = j + 1; i < cols; i++) {
            if (fabs(a[i][j]) > fabs(a[i1][j])) {
                i1 = i;
            }
        }

        // Swap rows i1 and j in a and b to put pivot on diagonal
        SIMD_NAMESPACE::swap(a[i1], a[j]);
        SIMD_NAMESPACE::swap(b[i1], b[j]);

        // Scale row j to have a unit diagonal
        double s = a[j][j];
        if (s == 0.0) {
            inversionSucceeded = false;
            break;
        }

        s = 1.0 / s;
        b[j] *= s;
        a[j] *= s;

        // Eliminate off-diagonal elems in col j of a, doing identical ops to b
        for (int i = 0; i < cols; i++) {
            if (i != j) {
                s = a[i][j];
                b[i] -= b[j] * s;
                a[i] -= a[j] * s;
            }
        }
    }

    if (!inversionSucceeded) {
        b = kdouble4x4_zero;
    }

    return b;
}

// determinant
// internal only ops
// TODO: could just be macros
inline double3 rotate1(double3 x) { return x.yzx; }
inline double3 rotate2(double3 x) { return x.zxy; }
inline double4 rotate1(double4 x) { return x.yzwx; }
inline double4 rotate2(double4 x) { return x.zwxy; }
inline double4 rotate3(double4 x) { return x.wxyz; }

double determinant(const double2x2& x)
{
    return cross(x[0], x[1]);
}

double determinant(const double3x3& x)
{
    return reduce_add(
        x[0] * (rotate1(x[1]) * rotate2(x[2]) - rotate2(x[1]) * rotate1(x[2])));
}

double determinant(const double4x4& x)
{
    double4 codet = x[0] * (rotate1(x[1]) * (rotate2(x[2]) * rotate3(x[3]) - rotate3(x[2]) * rotate2(x[3])) +
                            rotate2(x[1]) * (rotate3(x[2]) * rotate1(x[3]) - rotate1(x[2]) * rotate3(x[3])) +
                            rotate3(x[1]) * (rotate1(x[2]) * rotate2(x[3]) - rotate2(x[2]) * rotate1(x[3])));
    return reduce_add(codet.even - codet.odd);
}

// trace
double trace(const double2x2& x)
{
    return x[0].x + x[1].y;
}

double trace(const double3x3& x)
{
    return x[0].x + x[1].y + x[2].z;
}

double trace(const double4x4& x)
{
    return x[0].x + x[1].y + x[2].z + x[3].w;
}

// TODO: may want pre-transform on double3x4 since it's transposed
// 3 x m3x4 should = 3 element vec
//
// simd premul transform on left does a super expensive transpose to avoid dot
// don't use this, should just dotproducts?
//static   half2 mul(  half2 x,   half2x2 y) { return mul(transpose(y), x); }
//
//
// Here's how to multiply matrices, since default ops won't do this.
// be careful with operator* built-in.  Will do column by column mul won't it?
// Maybe that's why *= is missing on matrices.
//
// This is taking each scalar of y[0], hopfully this extends and stays in vec op

// premul-transform has to do dots
double2 mul(double2 y, const double2x2& x)
{
    double2 r;
    r.x = dot(y, x[0]);
    r.y = dot(y, x[1]);
    return r;
}

double3 mul(double3 y, const double3x3& x)
{
    double3 r;
    r.x = dot(y, x[0]);
    r.y = dot(y, x[1]);
    r.z = dot(y, x[2]);
    return r;
}

double4 mul(double4 y, const double4x4& x)
{
    double4 r;
    r.x = dot(y, x[0]);
    r.y = dot(y, x[1]);
    r.z = dot(y, x[2]);
    r.w = dot(y, x[3]);
    return r;
}

// post-transform at least does a mul madd
double2 mul(const double2x2& x, double2 y)
{
    double2 r = x[0] * y[0]; // no mul(v,v)
    r = muladd(x[1], y[1], r);
    return r;
}

double3 mul(const double3x3& x, double3 y)
{
    double3 r = x[0] * y[0];
    r = muladd(x[1], y[1], r);
    r = muladd(x[2], y[2], r);
    return r;
}

double4 mul(const double4x4& x, double4 y)
{
    double4 r = x[0] * y[0];
    r = muladd(x[1], y[1], r);
    r = muladd(x[2], y[2], r);
    r = muladd(x[3], y[3], r);
    return r;
}

// matrix muls using mul madd
double2x2 mul(const double2x2& x, const double2x2& y)
{
    double2x2 r;

    // m * columns
    r[0] = mul(x, y[0]);
    r[1] = mul(x, y[1]);

    return r;
}

double3x3 mul(const double3x3& x, const double3x3& y)
{
    double3x3 r;
    r[0] = mul(x, y[0]);
    r[1] = mul(x, y[1]);
    r[2] = mul(x, y[2]);
    return r;
}

double4x4 mul(const double4x4& x, const double4x4& y)
{
    double4x4 r;
    r[0] = mul(x, y[0]);
    r[1] = mul(x, y[1]);
    r[2] = mul(x, y[2]);
    r[3] = mul(x, y[3]);
    return r;
}

// sub
double2x2 sub(const double2x2& x, const double2x2& y)
{
    double2x2 r(x);
    r[0] -= y[0];
    r[1] -= y[1];
    return r;
}
double3x3 sub(const double3x3& x, const double3x3& y)
{
    double3x3 r(x);
    r[0] -= y[0];
    r[1] -= y[1];
    r[2] -= y[2];
    return r;
}
double4x4 sub(const double4x4& x, const double4x4& y)
{
    double4x4 r(x);
    r[0] -= y[0];
    r[1] -= y[1];
    r[2] -= y[2];
    r[3] -= y[3];
    return r;
}

// add
double2x2 add(const double2x2& x, const double2x2& y)
{
    double2x2 r(x);
    r[0] += y[0];
    r[1] += y[1];
    return r;
}
double3x3 add(const double3x3& x, const double3x3& y)
{
    double3x3 r(x);
    r[0] += y[0];
    r[1] += y[1];
    r[2] += y[2];
    return r;
}
double4x4 add(const double4x4& x, const double4x4& y)
{
    double4x4 r(x);
    r[0] += y[0];
    r[1] += y[1];
    r[2] += y[2];
    r[3] += y[3];
    return r;
}

// equal
bool equal(const double2x2& x, const double2x2& y)
{
    return all(x[0] == y[0] &
               x[1] == y[1]);
}
bool equal(const double3x3& x, const double3x3& y)
{
    return all(x[0] == y[0] &
               x[1] == y[1] &
               x[2] == y[2]);
}
bool equal(const double4x4& x, const double4x4& y)
{
    return all(x[0] == y[0] &
               x[1] == y[1] &
               x[2] == y[2] &
               x[3] == y[3]);
}

// equal_abs
bool equal_abs(const double2x2& x, const double2x2& y, double tol)
{
    return all((abs(x[0] - y[0]) <= tol) &
               (abs(x[1] - y[1]) <= tol));
}
bool equal_abs(const double3x3& x, const double3x3& y, double tol)
{
    return all((abs(x[0] - y[0]) <= tol) &
               (abs(x[1] - y[1]) <= tol) &
               (abs(x[2] - y[2]) <= tol));
}
bool equal_abs(const double4x4& x, const double4x4& y, double tol)
{
    return all((abs(x[0] - y[0]) <= tol) &
               (abs(x[1] - y[1]) <= tol) &
               (abs(x[2] - y[2]) <= tol) &
               (abs(x[3] - y[3]) <= tol));
}

// equal_rel
bool equal_rel(const double2x2& x, const double2x2& y, double tol)
{
    return all((abs(x[0] - y[0]) <= tol * abs(x[0])) &
               (abs(x[1] - y[1]) <= tol * abs(x[1])));
}
bool equal_rel(const double3x3& x, const double3x3& y, double tol)
{
    return all((abs(x[0] - y[0]) <= tol * abs(x[0])) &
               (abs(x[1] - y[1]) <= tol * abs(x[1])) &
               (abs(x[2] - y[2]) <= tol * abs(x[2])));
}
bool equal_rel(const double4x4& x, const double4x4& y, double tol)
{
    return all((abs(x[0] - y[0]) <= tol * abs(x[0])) &
               (abs(x[1] - y[1]) <= tol * abs(x[1])) &
               (abs(x[2] - y[2]) <= tol * abs(x[2])) &
               (abs(x[3] - y[3]) <= tol * abs(x[3])));
}

} // namespace SIMD_NAMESPACE
#endif // SIMD_DOUBLE
