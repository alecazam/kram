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
macroVector8TypesStorage(double, double)
macroVector8TypesPacked(double, double)

// storage type for matrix
typedef struct { double2s columns[2]; } double2x2s;
typedef struct { double3s columns[3]; } double3x3s;
typedef struct { double4s columns[3]; } double3x4s;
typedef struct { double4s columns[4]; } double4x4s;

// glue to Accelerate
#if SIMD_RENAME_TO_SIMD_NAMESPACE
macroVector8TypesStorageRenames(double, simd_double)
#endif

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

// use sse2neon to port this for now
SIMD_CALL double4 reduce_addv(double4 x) {
    // 4:1 reduction
    x.lo = _mm_hadd_pd(x.lo, x.lo);
    x.hi = _mm_hadd_pd(x.hi, x.hi);
    x.lo = _mm_hadd_pd(x.lo, x.hi);
    return x.lo.x; // repeat x to all values
}

SIMD_CALL double reduce_add(double4 x) {
    return reduce_addv(x).x;
}

#endif // SIMD_NEON

#if SIMD_SSE

// x64 doesn't seem to have a simd op for min/max reduce
SIMD_CALL double reduce_min(double4 x) {
    return fmin(fmin(x.x,x.y), fmin(x.z,x.w));
}

SIMD_CALL double reduce_max(double4 x) {
    return fmax(fmax(x.x,x.y), fmax(x.z,x.w));
}

// needs SIMD_LONG
// needed for precise min/max calls below
#if SIMD_LONG
SIMD_CALL double4 bitselect_forminmax(double4 x, double4 y, long4 mask) {
    return (double4)(((long4)x & ~mask) | ((long4)y & mask));
}
#endif // SIMD_LONG

SIMD_CALL double4 min(double4 x, double4 y) {
    // precise returns x on Nan
    return bitselect_forminmax(_mm_min_pd(x, y), x, y != y);
}

SIMD_CALL double4 max(double4 x, double4 y) {
    // precise returns x on Nan
    return bitselect_forminmax(_mm_max_pd(x, y), x, y != y);
}

SIMD_CALL double4 muladd(double4 x, double4 y, double4 t) {
    // can't get Xcode to set -mfma with AVX2 set
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
SIMD_CALL double4 sqrt(double4 x) {
    return double4m(sqrt(x.lo), sqrt(x.hi));
}

SIMD_CALL double2 reduce_addv(double2 x) {
    x = _mm_hadd_pd(x.lo, x.lo);
    return x.x;
}

SIMD_CALL double4 reduce_addv(double4 x) {
    // 4:1 reduction
    x.lo = _mm_hadd_pd(x.lo, x.lo); // TODO: fix
    x.hi = _mm_hadd_pd(x.hi, x.hi);
    x.lo = _mm_hadd_pd(x.lo, x.hi);
    return x.lo.x; // repeat x to all values
}

SIMD_CALL double reduce_add(double4 x) {
    return reduce_addv(x).x;
}

#endif // SIMD_LONG && SIMD_SSE

// SSE4.1
// single ops in AVX/2

SIMD_CALL double4 round(double4 vv) {
    return double4m(_mm_round_pd(vv.lo, 0x8),_mm_round_pd(vv.hi, 0x8));  // round to nearest | exc
}
SIMD_CALL double4 ceil(double4 vv) {
    return double4m(_mm_ceil_pd(vv.lo),_mm_ceil_pd(vv.hi));
}
SIMD_CALL double4 floor(double4 vv) {
    return double4m(_mm_floor_pd(vv.lo),_mm_floor_pd(vv.hi));
}

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

// zeroext - internal helper
SIMD_CALL double4 zeroext(double2 x) {
    return (double4){x.x,x.y,0,0};
}
SIMD_CALL double4 zeroext(double3 x) {
    return (double4){x.x,x.y,x.z,0};
}

// min
//SIMD_CALL double2 min(double2 x, double2 y) {
//    return vec4to2(min(vec2to4(x), vec2to4(y)));
//}
SIMD_CALL double3 min(double3 x, double3 y) {
    return vec4to3(min(vec3to4(x), vec3to4(y)));
}

// max
//SIMD_CALL double2 max(double2 x, double2 y) {
//    return vec4to2(max(vec2to4(x), vec2to4(y)));
//}
SIMD_CALL double3 max(double3 x, double3 y) {
    return vec4to3(max(vec3to4(x), vec3to4(y)));
}

// sqrt
//SIMD_CALL double2 sqrt(double2 x) {
//    return vec4to2(sqrt(vec2to4(x)));
//}
SIMD_CALL double3 sqrt(double3 x) {
    return vec4to3(sqrt(vec3to4(x)));
}

// rsqrt
SIMD_CALL double4 rsqrt(double4 x) {
    // TODO: fixup near 0 ?
    // TODO: use _mm_div_ps if / doesn't use
    return 1.0/sqrt(x);
}
SIMD_CALL double2 rsqrt(double2 x) {
    return vec4to2(rsqrt(vec2to4(x)));
}
SIMD_CALL double3 rsqrt(double3 x) {
    return vec4to3(rsqrt(vec3to4(x)));
}


// recip
SIMD_CALL double4 recip(double4 x) {
    // TODO: fixup near 0 ?
    // TODO: use _mm_div_ps if / doesn't use
    return 1.0/x;
}
SIMD_CALL double2 recip(double2 x) {
    return vec4to2(recip(vec2to4(x)));
}
SIMD_CALL double3 recip(double3 x) {
    return vec4to3(recip(vec3to4(x)));
}

// reduce_add
SIMD_CALL double reduce_add(double2 x) {
    return reduce_add(zeroext(x));
}
SIMD_CALL double reduce_add(double3 x) {
    return reduce_add(zeroext(x));
}

// reduce_min - arm has double2 op
//SIMD_CALL double reduce_min(double2 x) {
//    return reduce_min(vec2to4(x));
//}

SIMD_CALL double reduce_min(double3 x) {
    return reduce_min(vec3to4(x));
}

// reduce_max
//SIMD_CALL double reduce_max(double2 x) {
//    return reduce_max(vec2to4(x));
//}

SIMD_CALL double reduce_max(double3 x) {
    return reduce_max(vec3to4(x));
}

// round (to nearest)
SIMD_CALL double2 round(double2 x) { return vec4to2(round(vec2to4(x))); }
SIMD_CALL double3 round(double3 x) { return vec4to3(round(vec3to4(x))); }

// ceil
SIMD_CALL double2 ceil(double2 x) { return vec4to2(ceil(vec2to4(x))); }
SIMD_CALL double3 ceil(double3 x) { return vec4to3(ceil(vec3to4(x))); }

// floor
SIMD_CALL double2 floor(double2 x) { return vec4to2(floor(vec2to4(x))); }
SIMD_CALL double3 floor(double3 x) { return vec4to3(floor(vec3to4(x))); }

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

double2 saturate(double2 x);
double3 saturate(double3 x);
double4 saturate(double4 x);

// muladd - arm has double2 op
//SIMD_CALL double2 muladd(double2 x, double2 y, double2 t) {
//    return vec4to2(muladd(vec2to4(x), vec2to4(y), vec2to4(t)));
//}
SIMD_CALL double3 muladd(double3 x, double3 y, double3 t) {
    return vec4to3(muladd(vec3to4(x), vec3to4(y), vec3to4(t)));
}

// lerp - another easy one
SIMD_CALL double2 lerp(double2 x, double2 y, double2 t) {
    return x + t*(y - x);
}
SIMD_CALL double3 lerp(double3 x, double3 y, double3 t) {
    return x + t*(y - x);
}
SIMD_CALL double4 lerp(double4 x, double4 y, double4 t) {
    return x + t*(y - x);
}


// dot
SIMD_CALL double dot(double2 x, double2 y) {
    return reduce_add(x * y);
}
SIMD_CALL double dot(double3 x, double3 y) {
    return reduce_add(x * y);
}
SIMD_CALL double dot(double4 x, double4 y) {
    return reduce_add(x * y);
}

// length_squared
SIMD_CALL double length_squared(double2 x) {
    return reduce_add(x * x);
}
SIMD_CALL double length_squared(double3 x) {
    return reduce_add(x * x);
}
SIMD_CALL double length_squared(double4 x) {
    return reduce_add(x * x);
}

// length
SIMD_CALL double length(double2 x) {
    // worth using simd_sqrt?
    return ::sqrt(reduce_add(x * x));
}
SIMD_CALL double length(double3 x) {
    return ::sqrt(reduce_add(x * x));
}
SIMD_CALL double length(double4 x) {
    return ::sqrt(reduce_add(x * x));
}

// distance
SIMD_CALL double distance(double2 x, double2 y) {
    return length(x - y);
}
SIMD_CALL double distance(double3 x, double3 y) {
    return length(x - y);
}
SIMD_CALL double distance(double4 x, double4 y) {
    return length(x - y);
}

// normalize
// optimized by staying in reg
SIMD_CALL double4 normalize(double4 x) {
    // x * invlength(x)
    return x * rsqrt(reduce_addv(x * x)).x;
}
SIMD_CALL double2 normalize(double2 x) {
    return vec4to2(normalize(zeroext(x)));
}
SIMD_CALL double3 normalize(double3 x) {
    return vec4to3(normalize(zeroext(x)));
}

// abs
SIMD_CALL double2 abs(double2 x) {
    return bitselect(0.0, x, 0x7fffffff); // TODO: fp64
}
SIMD_CALL double3 abs(double3 x) {
    return bitselect(0.0, x, 0x7fffffff);
}
SIMD_CALL double4 abs(double4 x) {
    return bitselect(0.0, x, 0x7fffffff);
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
    return min(x - floor(x), 0x1.fffffep-1f); // TODO: fp64
}
SIMD_CALL double3 fract(double3 x) {
    return min(x - floor(x), 0x1.fffffep-1f);
}
SIMD_CALL double4 fract(double4 x) {
    return min(x - floor(x), 0x1.fffffep-1f);
}

//-------------------



// power series
macroVectorRepeatFnDecl(double, log)
macroVectorRepeatFnDecl(double, exp)
//macroVectorRepeatFnDecl(double, pow)

// trig
macroVectorRepeatFnDecl(double, cos)
macroVectorRepeatFnDecl(double, sin)
macroVectorRepeatFnDecl(double, tan)

SIMD_CALL double2 pow(double2 x, double2 y) {
    return exp(log(x) * y);
}
SIMD_CALL double3 pow(double3 x, double3 y) {
    return exp(log(x) * y);
}
SIMD_CALL double4 pow(double4 x, double4 y) {
    return exp(log(x) * y);
}

struct double2x2 : double2x2s
{
    // can be split out to traits
    static constexpr int col = 2;
    static constexpr int row = 2;
    using column_t = double2;
    using scalar_t = double;
    
    static const double2x2& zero();
    static const double2x2& identity();
    
    double2x2() { }  // no default init
    explicit double2x2(double2 diag);
    double2x2(double2 c0, double2 c1)
    : double2x2s((double2x2s){c0, c1}) { }
    double2x2(const double2x2s& m)
    : double2x2s(m) { }
    
    // simd lacks these ops
    double2& operator[](int idx) { return columns[idx]; }
    const double2& operator[](int idx) const { return columns[idx]; }
};

struct double3x3 : double3x3s
{
    static constexpr int col = 3;
    static constexpr int row = 3;
    using column_t = double3;
    using scalar_t = double;
    
    // Done as wordy c funcs otherwize.  Funcs allow statics to init.
    static const double3x3& zero();
    static const double3x3& identity();
    
    double3x3() { }  // no default init
    explicit double3x3(double3 diag);
    double3x3(double3 c0, double3 c1, double3 c2)
    : double3x3s((double3x3s){c0, c1, c2}) { }
    double3x3(const double3x3s& m)
    : double3x3s(m) { }
    
    double3& operator[](int idx) { return columns[idx]; }
    const double3& operator[](int idx) const { return columns[idx]; }
};

// This is mostly a transposed holder for a 4x4, so very few ops defined
// Can also serve as a SOA for some types of cpu math.
struct double3x4 : double3x4s
{
    static constexpr int col = 3;
    static constexpr int row = 4;
    using column_t = double4;
    using scalar_t = double;
    
    static const double3x4& zero();
    static const double3x4& identity();
    
    double3x4() { } // no default init
    explicit double3x4(double3 diag);
    double3x4(double4 c0, double4 c1, double4 c2)
    : double3x4s((double3x4s){c0, c1, c2}) { }
    double3x4(const double3x4s& m)
    : double3x4s(m) { }
    
    double4& operator[](int idx) { return columns[idx]; }
    const double4& operator[](int idx) const { return columns[idx]; }
};

struct double4x4 : double4x4s
{
    static constexpr int col = 4;
    static constexpr int row = 4;
    using column_t = double4;
    using scalar_t = double;
    
    static const double4x4& zero();
    static const double4x4& identity();
    
    double4x4() { } // no default init
    explicit double4x4(double4 diag);
    double4x4(double4 c0, double4 c1, double4 c2, double4 c3)
    : double4x4s((double4x4s){c0, c1, c2, c3}) { }
    double4x4(const double4x4s& m)
    : double4x4s(m) { }
    
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
