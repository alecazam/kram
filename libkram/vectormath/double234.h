// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

// This is not yet standalone.  vectormath++.h includes it.
#if USE_SIMDLIB && SIMD_DOUBLE

#ifdef __cplusplus
extern "C" {
#endif

// define c vector/matrix types
macroVector8TypesAligned(double, double)
macroVector8TypesPacked(double, double)

// storage type for matrix
typedef struct { double2a columns[2]; } double2x2a;
typedef struct { double3a columns[3]; } double3x3a;
typedef struct { double4a columns[3]; } double3x4a;
typedef struct { double4a columns[4]; } double4x4a;

// glue to Accelerate
#if SIMD_ACCELERATE_MATH_NAMES
macroVector8TypesStorageRenames(double, simd_double)
#endif // SIMD_ACCELERATE_MATH_NAMES

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace SIMD_NAMESPACE {

macroVector8TypesStorageRenames(double, double)

SIMD_CALL double2 double2m(double x) {
    return x;
}
SIMD_CALL double2 double2m(double x, double y) {
    return {x,y};
}

SIMD_CALL double3 double3m(double x) {
    return x;
}
SIMD_CALL double3 double3m(double x, double y, double z) {
    return {x,y,z};
}
SIMD_CALL double3 double3m(double2 v, double z) {
    double3 r; r.xy = v; r.z = z; return r;
}

SIMD_CALL double4 double4m(double x) {
    return x;
}
SIMD_CALL double4 double4m(double2 xy, double2 zw) {
    double4 r; r.xy = xy; r.zw = zw; return r;
}
SIMD_CALL double4 double4m(double x, double y, double z, double w = 1.0) {
    return {x,y,z,w};
}
SIMD_CALL double4 double4m(double3 v, double w = 1.0) {
    double4 r; r.xyz = v; r.w = w; return r;
}

//-----------------------------------
// start of implementation

// zeroext - internal helper
SIMD_CALL double4 zeroext(double2 x) { double4 v = 0; v.xy = x; return v;}
SIMD_CALL double4 zeroext(double3 x) { double4 v = 0; v.xyz = x; return v; }

#if SIMD_NEON

// TODO: expose double2 ops on Neon.
// think I have to, so that 4 can call 2x2 with hi/lo

SIMD_CALL double reduce_min(double2 x) {
    return vminvq_f64(x);
}
SIMD_CALL double reduce_min(double4 x) {
    return fmin(reduce_min(x.lo),reduce_min(x.hi));
}

SIMD_CALL double reduce_max(double2 x) {
    return vmaxvq_f64(x);
}
SIMD_CALL double reduce_max(double4 x) {
    return fmax(reduce_max(x.lo),reduce_max(x.hi));
}

SIMD_CALL double2 min(double2 x, double2 y) {
    // precise returns x on Nan
    return vminnmq_f64(x, y);
}
SIMD_CALL double4 min(double4 x, double4 y) {
    // precise returns x on Nan
    return double4m(min(x.lo,y.lo), min(x.hi,y.hi));
}

SIMD_CALL double2 max(double2 x, double2 y) {
    // precise returns x on Nan
    return vmaxnmq_f64(x, y);
}
SIMD_CALL double4 max(double4 x, double4 y) {
    // precise returns x on Nan
    return double4m(max(x.lo,y.lo), max(x.hi,y.hi));
}

SIMD_CALL double2 muladd(double2 x, double2 y, double2 t) {
    // requires __ARM_VFPV4__
    // t passed first unlike sse
    return vfmaq_f64(t, x,y);
}
SIMD_CALL double4 muladd(double4 x, double4 y, double4 t) {
    return double4m(muladd(x.lo,y.lo,t.lo), muladd(x.hi,y.hi,t.hi));
}

SIMD_CALL double2 sqrt(double2 x) {
    return vsqrtq_f64(x);
}
SIMD_CALL double4 sqrt(double4 x) {
    return double4m(sqrt(x.lo), sqrt(x.hi));
}

SIMD_CALL double2 reduce_addv(double2 x) {
    // 4:1 reduction
    x = vpaddq_f64(x, x);
    return x.x;
}
SIMD_CALL double4 reduce_addv(double4 x) {
    // 4:1 reduction
    x.lo = vpaddq_f64(x.lo, x.lo);
    x.hi = vpaddq_f64(x.hi, x.hi);
    x.lo = vpaddq_f64(x.lo, x.hi);
    return x.x; // repeat x to all values
}
SIMD_CALL double3 reduce_addv(double3 x) {
    return reduce_addv(zeroext(x)).x;
}

// round to nearest | exc
SIMD_CALL double2 round(double2 vv) {
    return vrndnq_f64(vv);
}
SIMD_CALL double4 round(double4 vv) {
    return double4m(round(vv.lo),round(vv.hi));
}

SIMD_CALL double2 ceil(double2 vv) {
    return vrndpq_f64(vv);
}
SIMD_CALL double4 ceil(double4 vv) {
    return double4m(ceil(vv.lo),ceil(vv.hi));
}

SIMD_CALL double2 floor(double2 vv) {
    return vrndmq_f64(vv);
}
SIMD_CALL double4 floor(double4 vv) {
    return double4m(floor(vv.lo),floor(vv.hi));
}

#endif // SIMD_NEON

//----------------------

#if SIMD_SSE

// x64 doesn't seem to have a simd op for min/max reduce
SIMD_CALL double reduce_min(double2 x) {
    return fmin(x.x,x.y);
}
SIMD_CALL double reduce_min(double4 x) {
    return fmin(fmin(x.x,x.y), fmin(x.z,x.w));
}

SIMD_CALL double reduce_max(double2 x) {
    return fmax(x.x,x.y);
}
SIMD_CALL double reduce_max(double4 x) {
    return fmax(fmax(x.x,x.y), fmax(x.z,x.w));
}

// needs SIMD_LONG
// needed for precise min/max calls below
#if SIMD_LONG
SIMD_CALL double2 bitselect_forminmax(double2 x, double2 y, long2 mask) {
    return (double2)(((long2)x & ~mask) | ((long2)y & mask));
}
// may only want to use this on AVX2
SIMD_CALL double4 bitselect_forminmax(double4 x, double4 y, long4 mask) {
    return (double4)(((long4)x & ~mask) | ((long4)y & mask));
}
#endif // SIMD_LONG

// precise returns x on Nan
SIMD_CALL double2 min(double2 x, double2 y) {
    return bitselect_forminmax(_mm_min_pd(x, y), x, y != y);
}
SIMD_CALL double2 max(double2 x, double2 y) {
    return bitselect_forminmax(_mm_max_pd(x, y), x, y != y);
}
SIMD_CALL double2 muladd(double2 x, double2 y, double2 t) {
#ifdef __FMA__
    return _mm_fmadd_pd(x,y,t);
#else
    // fallback with not same characteristics
    return x * y + t;
#endif
}

SIMD_CALL double2 sqrt(double2 x) {
    return _mm_sqrt_pd(x);
}
SIMD_CALL double2 reduce_addv(double2 x) {
    x = _mm_hadd_pd(x, x);
    return x.x;
}
SIMD_CALL double2 round(double2 x) {
    return _mm_round_pd(x, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
}
SIMD_CALL double2 ceil(double2 x) {
    return _mm_ceil_pd(x);
}
SIMD_CALL double2 floor(double2 x) {
    return _mm_floor_pd(x);
}

// now avx/avx2 can do 4 doubles in one call
#if SIMD_AVX2

SIMD_CALL double4 min(double4 x, double4 y) {
    return bitselect_forminmax(_mm256_min_pd(x, y), x, y != y);
}
SIMD_CALL double4 max(double4 x, double4 y) {
    return bitselect_forminmax(_mm256_max_pd(x, y), x, y != y);
}
SIMD_CALL double4 muladd(double4 x, double4 y, double4 t) {
#ifdef __FMA__
    return _mm256_fmadd_pd(x,y,t);
#else
    // fallback with not same characteristics
    return x * y + t;
#endif
}

SIMD_CALL double4 sqrt(double4 x) {
    return _mm256_sqrt_pd(x);
}

SIMD_CALL double4 reduce_addv(double4 x) {
    x = _mm256_hadd_ps(x, x); // xy = x+y,z+w
    x = _mm256_hadd_ps(x, x); // x  = x+y
    return x.x; // repeat x to all values
}
SIMD_CALL double3 reduce_addv(double3 x) {
    return reduce_addv(zeroext(x)).x;
}

SIMD_CALL double4 round(double4 vv) {
    return _mm256_round_pd(vv, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
}
SIMD_CALL double4 ceil(double4 vv) {
    return _mm256_ceil_pd(vv);
}
SIMD_CALL double4 floor(double4 vv) {
    return _mm256_floor_pd(vv);
}

#else

// SSE4 ops as a fallback.  These have to make 2+ calls.
SIMD_CALL double4 min(double4 x, double4 y) {
    return double4m(min(x.lo,y.lo), min(x.hi,y.hi));
}
SIMD_CALL double4 max(double4 x, double4 y) {
    return double4m(max(x.lo,y.lo), max(x.hi,y.hi));
}
SIMD_CALL double4 muladd(double4 x, double4 y, double4 t) {
#ifdef __FMA__
    return double4m(muladd(x.lo,y.lo,t.lo),
                    muladd(x.hi,y.hi,t.hi));
#else
    // fallback with not same characteristics
    return x * y + t;
#endif
}

SIMD_CALL double4 sqrt(double4 x) {
    return double4m(sqrt(x.lo), sqrt(x.hi));
}

SIMD_CALL double4 reduce_addv(double4 x) {
    // 4:1 reduction
    x.lo = _mm_hadd_pd(x.lo, x.lo);
    x.hi = _mm_hadd_pd(x.hi, x.hi);
    x.lo = _mm_hadd_pd(x.lo, x.hi);
    return x.x; // repeat x to all values
}
SIMD_CALL double3 reduce_addv(double3 x) {
    return reduce_addv(zeroext(x)).x;
}

SIMD_CALL double4 round(double4 vv) {
    return double4m(round(vv.lo), round(vv.hi));
}
SIMD_CALL double4 ceil(double4 vv) {
    return double4m(ceil(vv.lo),ceil(vv.hi));
}
SIMD_CALL double4 floor(double4 vv) {
    return double4m(floor(vv.lo),floor(vv.hi));
}

#endif
#endif // SIMD_SSE


// end of implementation
//-----------------------------------

#if SIMD_LONG

// bitselect
SIMD_CALL double2 bitselect(double2 x, double2 y, long2 mask) {
    return (double2)bitselect((long2)x, (long2)y, mask);
}
SIMD_CALL double3 bitselect(double3 x, double3 y, long3 mask) {
    return (double3)bitselect((long3)x, (long3)y, mask);
}
SIMD_CALL double4 bitselect(double4 x, double4 y, long4 mask) {
    return (double4)bitselect((long4)x, (long4)y, mask);
}

// select
SIMD_CALL double2 select(double2 x, double2 y, long2 mask) {
    return bitselect(x, y, mask >> 63);
}
SIMD_CALL double3 select(double3 x, double3 y, long3 mask) {
    return bitselect(x, y, mask >> 63);
}
SIMD_CALL double4 select(double4 x, double4 y, long4 mask) {
    return bitselect(x, y, mask >> 63);
}

#endif // SIMD_LONG

// double3 leftovers
SIMD_CALL double3 min(double3 x, double3 y) { return vec4to3(min(vec3to4(x), vec3to4(y))); }
SIMD_CALL double3 max(double3 x, double3 y) { return vec4to3(max(vec3to4(x), vec3to4(y))); }
SIMD_CALL double3 muladd(double3 x, double3 y, double3 t) { return vec4to3(muladd(vec3to4(x), vec3to4(y), vec3to4(t))); }
SIMD_CALL double3 sqrt(double3 x) { return vec4to3(sqrt(vec3to4(x))); }
SIMD_CALL double reduce_min(double3 x) { return reduce_min(vec3to4(x)); }
SIMD_CALL double reduce_max(double3 x) { return reduce_max(vec3to4(x)); }
SIMD_CALL double3 round(double3 x) { return vec4to3(round(vec3to4(x))); }
SIMD_CALL double3 ceil(double3 x) { return vec4to3(ceil(vec3to4(x))); }
SIMD_CALL double3 floor(double3 x) { return vec4to3(floor(vec3to4(x))); }

SIMD_CALL double4 rsqrt(double4 x) { return 1.0/sqrt(x); }
SIMD_CALL double2 rsqrt(double2 x) { return 1.0/sqrt(x); }
SIMD_CALL double3 rsqrt(double3 x) { return 1.0/sqrt(x); }

SIMD_CALL double2 recip(double2 x) { return 1.0/x; }
SIMD_CALL double3 recip(double3 x) { return 1.0/x; }
SIMD_CALL double4 recip(double4 x) { return 1.0/x; }

SIMD_CALL double reduce_add(double2 x) { return reduce_addv(x).x; }
SIMD_CALL double reduce_add(double3 x) { return reduce_addv(x).x; }
SIMD_CALL double reduce_add(double4 x) { return reduce_addv(x).x; }

// clamp
// order matters here for Nan, left op returned on precise min/max
SIMD_CALL double2 clamp(double2 x, double2 minv, double2 maxv) {
    return min(maxv, max(minv, x));
}
SIMD_CALL double3 clamp(double3 x, double3 minv, double3 maxv) {
    return min(maxv, max(minv, x));
}
SIMD_CALL double4 clamp(double4 x, double4 minv, double4 maxv) {
    return min(maxv, max(minv, x));
}

// saturate
SIMD_CALL double2 saturate(double2 x) { return clamp(x, 0, (double2)1); }
SIMD_CALL double3 saturate(double3 x) { return clamp(x, 0, (double3)1); }
SIMD_CALL double4 saturate(double4 x) { return clamp(x, 0, (double4)1); }

// lerp - another easy one
SIMD_CALL double2 lerp(double2 x, double2 y, double2 t) { return x + t*(y - x); }
SIMD_CALL double3 lerp(double3 x, double3 y, double3 t) { return x + t*(y - x); }
SIMD_CALL double4 lerp(double4 x, double4 y, double4 t) { return x + t*(y - x); }

// dot
SIMD_CALL double dot(double2 x, double2 y) { return reduce_add(x * y); }
SIMD_CALL double dot(double3 x, double3 y) { return reduce_add(x * y); }
SIMD_CALL double dot(double4 x, double4 y) { return reduce_add(x * y); }

// length_squared
SIMD_CALL double length_squared(double2 x) { return reduce_add(x * x); }
SIMD_CALL double length_squared(double3 x) { return reduce_add(x * x); }
SIMD_CALL double length_squared(double4 x) { return reduce_add(x * x); }

// length
// worth using simd_sqrt?
SIMD_CALL double length(double2 x) { return ::sqrt(reduce_add(x * x)); }
SIMD_CALL double length(double3 x) { return ::sqrt(reduce_add(x * x)); }
SIMD_CALL double length(double4 x) { return ::sqrt(reduce_add(x * x)); }

// distance
SIMD_CALL double distance(double2 x, double2 y) { return length(x - y); }
SIMD_CALL double distance(double3 x, double3 y) { return length(x - y); }
SIMD_CALL double distance(double4 x, double4 y) { return length(x - y); }

// normalize
// optimized by staying in reg
// x * invlength(x)
SIMD_CALL double4 normalize(double4 x) { return x * rsqrt(reduce_addv(x * x)).x; }
SIMD_CALL double2 normalize(double2 x) { return x * rsqrt(reduce_addv(x * x)).x; }
SIMD_CALL double3 normalize(double3 x) { return x * rsqrt(reduce_addv(x * x)).x; }

// abs
SIMD_CALL double2 abs(double2 x) {
    return bitselect(0.0, x, 0x7fffffffffffffff);
}
SIMD_CALL double3 abs(double3 x) {
    return bitselect(0.0, x, 0x7fffffffffffffff);
}
SIMD_CALL double4 abs(double4 x) {
    return bitselect(0.0, x, 0x7fffffffffffffff);
}

// cross
SIMD_CALL double cross(double2 x, double2 y) {
    return x.x * y.y - x.y * y.x;
}
SIMD_CALL double3 cross(double3 x, double3 y) {
    return x.yzx * y.zxy - x.zxy * y.yzx;
}

// equal_abs
SIMD_CALL bool equal_abs(double2 x, double2 y, double tol) {
    return all((abs(x - y) <= tol));
}
SIMD_CALL bool equal_abs(double3 x, double3 y, double tol) {
    return all((abs(x - y) <= tol));
}
SIMD_CALL bool equal_abs(double4 x, double4 y, double tol) {
    return all((abs(x - y) <= tol));
}

// equal_rel
SIMD_CALL bool equal_rel(double2 x, double2 y, double tol) {
    return all((abs(x - y) <= tol * ::abs(x.x)));
}
SIMD_CALL bool equal_rel(double3 x, double3 y, double tol) {
    return all((abs(x - y) <= tol * ::abs(x.x)));
}
SIMD_CALL bool equal_rel(double4 x, double4 y, double tol) {
    return all((abs(x - y) <= tol * ::abs(x.x)));
}

// step
SIMD_CALL double2 step(double2 edge, double2 x) {
    return bitselect((double2)1, 0, x < edge);
}
SIMD_CALL double3 step(double3 edge, double3 x) {
    return bitselect((double3)1, 0, x < edge);
}
SIMD_CALL double4 step(double4 edge, double4 x) {
    return bitselect((double4)1, 0, x < edge);
}

// smoothstep
SIMD_CALL double2 smoothstep(double2 edge0, double2 edge1, double2 x) {
    double2 t = saturate((x-edge0)/(edge0-edge1));
    return t*t*(3 - 2*t);
}
SIMD_CALL double3 smoothstep(double3 edge0, double3 edge1, double3 x) {
    double3 t = saturate((x-edge0)/(edge0-edge1));
    return t*t*(3 - 2*t);
}
SIMD_CALL double4 smoothstep(double4 edge0, double4 edge1, double4 x) {
    double4 t = saturate((x-edge0)/(edge0-edge1));
    return t*t*(3 - 2*t);
}

// fract
SIMD_CALL double2 fract(double2 x) {
    return min(x - floor(x), 0x1.fffffffffffffp-1);
}
SIMD_CALL double3 fract(double3 x) {
    return min(x - floor(x), 0x1.fffffffffffffp-1);
}
SIMD_CALL double4 fract(double4 x) {
    return min(x - floor(x), 0x1.fffffffffffffp-1);
}

//-------------------
// Functions

// power series
macroVectorRepeatFnDecl(double, log)
macroVectorRepeatFnDecl(double, exp)
//macroVectorRepeatFnDecl(double, pow)

// trig
macroVectorRepeatFnDecl(double, cos)
macroVectorRepeatFnDecl(double, sin)
macroVectorRepeatFnDecl(double, tan)

SIMD_CALL double2 pow(double2 x, double2 y) { return exp(log(x) * y); }
SIMD_CALL double3 pow(double3 x, double3 y) { return exp(log(x) * y); }
SIMD_CALL double4 pow(double4 x, double4 y) { return exp(log(x) * y); }

//-------------------
// constants

const double2& double2_zero();
const double2& double2_ones();

const double2& double2_posx();
const double2& double2_posy();

const double2& double2_negx();
const double2& double2_negy();

//----

const double3& double3_zero();
const double3& double3_ones();

const double3& double3_posx();
const double3& double3_posy();
const double3& double3_posz();

const double3& double3_negx();
const double3& double3_negy();
const double3& double3_negz();

//----

const double4& double4_zero();
const double4& double4_ones();

const double4& double4_posx();
const double4& double4_posy();
const double4& double4_posz();
const double4& double4_posw();

const double4& double4_negx();
const double4& double4_negy();
const double4& double4_negz();
const double4& double4_negw();

const double4& double4_posxw();
const double4& double4_posyw();
const double4& double4_poszw();

const double4& double4_negxw();
const double4& double4_negyw();
const double4& double4_negzw();

//-------------------
// Matrix

struct double2x2 : double2x2a
{
    // can be split out to traits
    static constexpr int col = 2;
    static constexpr int row = 2;
    using column_t = double2;
    using scalar_t = double;
    using base = double2x2a;
    
    static const double2x2& zero();
    static const double2x2& identity();
    
    double2x2() { }  // no default init
    explicit double2x2(double2 diag);
    double2x2(double2 c0, double2 c1)
    : base((base){c0, c1}) { }
    double2x2(const base& m)
    : base(m) { }
    
    // simd lacks these ops
    double2& operator[](int idx) { return columns[idx]; }
    const double2& operator[](int idx) const { return columns[idx]; }
};

struct double3x3 : double3x3a
{
    static constexpr int col = 3;
    static constexpr int row = 3;
    using column_t = double3;
    using scalar_t = double;
    using base = double3x3a;
    
    // Done as wordy c funcs otherwize.  Funcs allow statics to init.
    static const double3x3& zero();
    static const double3x3& identity();
    
    double3x3() { }  // no default init
    explicit double3x3(double3 diag);
    double3x3(double3 c0, double3 c1, double3 c2)
    : base((base){c0, c1, c2}) { }
    double3x3(const base& m)
    : base(m) { }
    
    double3& operator[](int idx) { return columns[idx]; }
    const double3& operator[](int idx) const { return columns[idx]; }
};

// This is mostly a transposed holder for a 4x4, so very few ops defined
// Can also serve as a SOA for some types of cpu math.
struct double3x4 : double3x4a
{
    static constexpr int col = 3;
    static constexpr int row = 4;
    using column_t = double4;
    using scalar_t = double;
    using base = double3x4a;
    
    static const double3x4& zero();
    static const double3x4& identity();
    
    double3x4() { } // no default init
    explicit double3x4(double3 diag);
    double3x4(double4 c0, double4 c1, double4 c2)
    : base((base){c0, c1, c2}) { }
    double3x4(const base& m)
    : base(m) { }
    
    double4& operator[](int idx) { return columns[idx]; }
    const double4& operator[](int idx) const { return columns[idx]; }
};

struct double4x4 : double4x4a
{
    static constexpr int col = 4;
    static constexpr int row = 4;
    using column_t = double4;
    using scalar_t = double;
    using base = double4x4a;
    
    static const double4x4& zero();
    static const double4x4& identity();
    
    double4x4() { } // no default init
    explicit double4x4(double4 diag);
    double4x4(double4 c0, double4 c1, double4 c2, double4 c3)
    : base((base){c0, c1, c2, c3}) { }
    double4x4(const base& m)
    : base(m) { }
    
    double4& operator[](int idx) { return columns[idx]; }
    const double4& operator[](int idx) const { return columns[idx]; }
};

double2x2 diagonal_matrix(double2 x);
double3x3 diagonal_matrix(double3 x);
double3x4 diagonal_matrix3x4(double3 x);
double4x4 diagonal_matrix(double4 x);

// ops need to call these

// using refs here, 3x3 and 4x4 are large to pass by value (3 simd regs)
double2x2 transpose(const double2x2& x);
double3x3 transpose(const double3x3& x);
double4x4 transpose(const double4x4& x);

double2x2 inverse(const double2x2& x);
double3x3 inverse(const double3x3& x);
double4x4 inverse(const double4x4& x);

double determinant(const double2x2& x);
double determinant(const double3x3& x);
double determinant(const double4x4& x);

double trace(const double2x2& x);
double trace(const double3x3& x);
double trace(const double4x4& x);

// premul = dot + premul
double2 mul(double2 y, const double2x2& x);
double3 mul(double3 y, const double3x3& x);
double4 mul(double4 y, const double4x4& x);

// posmul = mul + mad
double2x2 mul(const double2x2& x, const double2x2& y);
double3x3 mul(const double3x3& x, const double3x3& y);
double4x4 mul(const double4x4& x, const double4x4& y);

double2 mul(const double2x2& x, double2 y);
double3 mul(const double3x3& x, double3 y);
double4 mul(const double4x4& x, double4 y);

double2x2 sub(const double2x2& x, const double2x2& y);
double3x3 sub(const double3x3& x, const double3x3& y);
double4x4 sub(const double4x4& x, const double4x4& y);

double2x2 add(const double2x2& x, const double2x2& y);
double3x3 add(const double3x3& x, const double3x3& y);
double4x4 add(const double4x4& x, const double4x4& y);

bool equal(const double2x2& x, const double2x2& y);
bool equal(const double3x3& x, const double3x3& y);
bool equal(const double4x4& x, const double4x4& y);

// equal_abs
bool equal_abs(const double2x2& x, const double2x2& y, double tol);
bool equal_abs(const double3x3& x, const double3x3& y, double tol);
bool equal_abs(const double4x4& x, const double4x4& y, double tol);

// equal_rel
bool equal_rel(const double2x2& x, const double2x2& y, double tol);
bool equal_rel(const double3x3& x, const double3x3& y, double tol);
bool equal_rel(const double4x4& x, const double4x4& y, double tol);


// operators for C++
macroMatrixOps(double2x2);
macroMatrixOps(double3x3);
// TODO: no mat ops yet on storage type double3x4
// macroMatrixOps(double3x4);
macroMatrixOps(double4x4);

// fast conversions where possible
SIMD_CALL const double3x3& as_double3x3(const double4x4& m) {
    return reinterpret_cast<const double3x3&>(m);
}

} // SIMD_NAMESPACE

#endif

#endif // USE_SIMDLIB && SIMD_DOUBLE
