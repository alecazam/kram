// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.
#include "vectormath++.h"

#if USE_SIMDLIB

// Tests with godbolt are here to show code comparsions with optimizations.

// -Og can't unroll small loops for some reason. -O2 and -O3 do.
// https://godbolt.org/z/KMPa8bchb
//
// As optimal as it gets
// https://godbolt.org/z/YxzobGM17
//
// optimized quake rcp, rsqrt, sqrt
// https://michaldrobot.files.wordpress.com/2014/05/gcn_alu_opt_digitaldragons2014.pdf
//
// TODO: Fabian on fp16 <-> fp32
// Not the right gist, you want the RTNE one (nm: that only matters for float->half,
// this was the half->float one. FWIW, other dir is https://gist.github.com/rygorous/eb3a019b99fdaa9c3064.
// These days I use a variant of the RTNE/RN version that also preserves NaN payload bits,
// which is slightly more ops but matches hardware conversions exactly for every input, including all NaNs.
//
// DONE: bring over fast inverses (RTS, RTU, etc)
// DONE: need translation, rotation, scale
// TODO: need fast post-translation, post-rotation, post-scale
//
// TODO: saturating conversions would be useful to, and prevent overflow
// see the conversion.h code, bit select to clamp values.
//
// TODO: matrix_types.h has a type_traits with rows, cols, etc.
// can call get_traits() on them.  See matrix.h for all the ops.

// The storage of affine data in a column matrix is no different than rows
// translation is in (r30 r31 r32) or in (c30, c31 c32)
//
// r0: r00 r01 r02 0
// r1: r10         0
// r2: r20         0
// r3: px  py  pz  1
//
// c0  c1  c2  c3
// c00 c10 c20 px
// c01 c11     py
// c02         pz
//  0   0   0  1
//
// col: TRS * TRS * v   cameraToWorldTfm * worldToModelTfm * ..
// row: v * SRT * SRT   modelToWorldTfm * worldToCameraTfm * ...
//
// TODO: need natvis and lldb formatting of math classes.
//
// TODO: here's a decomp
// https://github.com/erich666/GraphicsGems/blob/master/gemsii/unmatrix.c

//-----------------

// TODO: profvide controls over this
#ifdef __APPLE__
#define SIMD_ACCELERATE_LIB 1
#define SIMD_FAST_MATH      0
#else
#define SIMD_ACCELERATE_LIB 0
#define SIMD_FAST_MATH      0
#endif

#if SIMD_ACCELERATE_LIB
// TODO: reduce this header to just calls use (f.e. geometry, etc)
#include <simd/simd.h>
#elif SIMD_FAST_MATH
#include "sse_mathfun.h"
#endif

// These are large functions that can be buried and optimized in the .cpp
// Has alternate cmath form it uses for now.  Look into ISPC calls to
// replace some of this.

//-----------------

namespace SIMD_NAMESPACE {

// I don't trust the approximations for now.  But providing them in case
// want to do futher.  Really 3 choices, use c calls, use approximations,
// or use simd lib that implements these (f.e. Accelerate).

// pow needs 2 args, so haven't exposed yet
//float4 pow(float4 x, float4 y) {
//    // can xy be <= 0 ?, no will return Nan in log/exp approx
//    return exp(log(x) * y);
//}
//
// don't have a c sincos, so just use fn calls
//float4 tan(float4 x) {
//    float4 s, c;
//    sincos(x, s, c);
//
//    // TODO: handle around c == 0 case
//    return s / c;
//}

#if SIMD_ACCELERATE_LIB

//---------------------------
// Use existing Accelerate lib.
//
// So there are currently 6 version of the Accelerate lib.
// This library hides implmenentations of some of the calls.
// So need to rely on a version of the lib to get them,
// or supply some alternative.
//
// 6: macOS 15.0, iOS 18.0
// 5: macOS 13.0, iOS 16.0
// 4: macOS 12.0, iOS 15.0
// 0: header only
//
// use 5 for macOS
// SIMD_LIBRARY_VERSION >= 5
//
// use 4 for iOS
// SIMD_LIBRARY_VERSION >= 4

// remap simdk to simd namespace
#define macroVectorRepeatFnImpl(type, cppfunc) \
type##2 cppfunc(type##2 x) { return simd::cppfunc(x); } \
type##3 cppfunc(type##3 x) { return simd::cppfunc(x); } \
type##4 cppfunc(type##4 x) { return simd::cppfunc(x); } \

#if SIMD_FLOAT

// These will get inlined here from the template
macroVectorRepeatFnImpl(float, log)
macroVectorRepeatFnImpl(float, exp)

macroVectorRepeatFnImpl(float, sin)
macroVectorRepeatFnImpl(float, cos)
macroVectorRepeatFnImpl(float, tan)

#endif

#if SIMD_DOUBLE

// These will get inlined here from the template
macroVectorRepeatFnImpl(double, log)
macroVectorRepeatFnImpl(double, exp)

macroVectorRepeatFnImpl(double, sin)
macroVectorRepeatFnImpl(double, cos)
macroVectorRepeatFnImpl(double, tan)

#endif

#elif !SIMD_FAST_MATH

//---------------------------

// This calls function repeatedly, then returns as vector.
// These don't call to the 4 version since it's so much more work.
#define macroVectorRepeatFnImpl(type, cppfunc, func) \
type##1 cppfunc(type##1 x) { return func(x); } \
type##2 cppfunc(type##2 x) { return {func(x.x), func(x.y)}; } \
type##3 cppfunc(type##3 x) { return {func(x.x), func(x.y), func(x.z)}; } \
type##4 cppfunc(type##4 x) { return {func(x.x), func(x.y), func(x.z), func(x.w)}; } \

#if SIMD_FLOAT

macroVectorRepeatFnImpl(float, log, ::logf)
macroVectorRepeatFnImpl(float, exp, ::expf)

macroVectorRepeatFnImpl(float, sin, ::sinf)
macroVectorRepeatFnImpl(float, cos, ::cosf)
macroVectorRepeatFnImpl(float, tan, ::tanf)

#endif // SIMD_FLOAT

#if SIMD_DOUBLE

macroVectorRepeatFnImpl(double, log, ::log)
macroVectorRepeatFnImpl(double, exp, ::exp)

macroVectorRepeatFnImpl(double, sin, ::sin)
macroVectorRepeatFnImpl(double, cos, ::cos)
macroVectorRepeatFnImpl(double, tan, ::tan)

#endif // SIMD_DOUBLE

#endif

// Which cmath had this
inline void sincosf(float angleInRadians, float& s, float& c) {
    s = sinf(angleInRadians);
    c = cosf(angleInRadians);
}

// These aren't embedded in function, so may have pre-init ordering issues.
// or could add pre-init order to skip using functions.
// Expose these through function calls as const&

static const float2 kfloat2_posx = {1.0f, 0.0f};
static const float2 kfloat2_posy = kfloat2_posx.yx;

static const float2 kfloat2_negx = {-1.0f, 0.0f};
static const float2 kfloat2_negy = kfloat2_negx.yx;

static const float2 kfloat2_ones = kfloat2_posx.xx;
static const float2 kfloat2_zero = {};

static const float3 kfloat3_posx = {1.0f, 0.0f, 0.0f};
static const float3 kfloat3_posy = kfloat3_posx.yxy;
static const float3 kfloat3_posz = kfloat3_posx.yyx;

static const float3 kfloat3_negx = {-1.0f, 0.0f, 0.0f};
static const float3 kfloat3_negy = kfloat3_negx.yxy;
static const float3 kfloat3_negz = kfloat3_negx.yyx;

static const float3 kfloat3_ones = kfloat3_posx.xxx;
static const float3 kfloat3_zero = {};

static const float4 kfloat4_posx = {1.0f, 0.0f, 0.0f, 0.0f};
static const float4 kfloat4_posy = kfloat4_posx.yxyy;
static const float4 kfloat4_posz = kfloat4_posx.yyxy;
static const float4 kfloat4_posw = kfloat4_posx.yyyx;

static const float4 kfloat4_negxw = {-1.0f, 0.0f, 0.0f, 1.0f};
static const float4 kfloat4_negyw = kfloat4_negxw.yxyw;
static const float4 kfloat4_negzw = kfloat4_negxw.yyxw;

static const float4 kfloat4_posxw = {1.0f, 0.0f, 0.0f, 1.0f};
static const float4 kfloat4_posyw = kfloat4_posxw.yxyw;
static const float4 kfloat4_poszw = kfloat4_posxw.yyxw;

static const float4 kfloat4_negx = {-1.0f, 0.0f, 0.0f, 0.0f};
static const float4 kfloat4_negy = kfloat4_negx.yxyy;
static const float4 kfloat4_negz = kfloat4_negx.yyxy;
static const float4 kfloat4_negw = kfloat4_negx.yyyx;

static const float4 kfloat4_ones = kfloat4_posx.xxxx;
static const float4 kfloat4_zero = {};

static const float2x2 kfloat2x2_zero = {}; // what is this value 0, or default ctor
static const float3x3 kfloat3x3_zero = {};
static const float3x4 kfloat3x4_zero = {};
static const float4x4 kfloat4x4_zero = {};

static const float2x2 kfloat2x2_identity = diagonal_matrix(kfloat2_ones);
static const float3x3 kfloat3x3_identity = diagonal_matrix(kfloat3_ones);
static const float3x4 kfloat3x4_identity = diagonal_matrix3x4(kfloat3_ones);
static const float4x4 kfloat4x4_identity = diagonal_matrix(kfloat4_ones);

//---------------------------

// These should not be used often.  So can stay buried
float2x2::float2x2(float2 diag)
: float2x2s((const float2x2s&)diagonal_matrix(diag)) { }
float3x3::float3x3(float3 diag)
: float3x3s((const float3x3s&)diagonal_matrix(diag)) { }
float3x4::float3x4(float3 diag)
: float3x4s((const float3x4s&)diagonal_matrix3x4(diag)) { }
float4x4::float4x4(float4 diag)
: float4x4s((const float4x4s&)diagonal_matrix(diag)) { }

//---------------------------

float2x2 diagonal_matrix(float2 x) {
    float4 xx = zeroext(x);
    return float2x2(xx.xw, xx.wy);
}
float3x3 diagonal_matrix(float3 x) {
    float4 xx = zeroext(x);
    return float3x3(xx.xww, xx.wyw, xx.wwz);
}
float3x4 diagonal_matrix3x4(float3 x) {
    float4 xx = zeroext(x);
    return float3x4(xx.xwww, xx.wyww, xx.wwzw);
}
float4x4 diagonal_matrix(float4 x) {
    float4 xx = x; xx.w = 0.0f;
    float4 ww = xx; ww.z = x.w;
    return float4x4(xx.xwww, xx.wyww, xx.wwzw, ww.wwwz);
}

//---------------------------

// saturate
float2 saturate(float2 x) {
    return clamp(x, kfloat2_zero, kfloat2_ones);
}
float3 saturate(float3 x) {
    return clamp(x, kfloat3_zero, kfloat3_ones);
}
float4 saturate(float4 x) {
    return clamp(x, kfloat4_zero, kfloat4_ones);
}

//--------------------------------------

// textbook transpose from simd/matrix.h
float2x2 transpose(const float2x2& x) {
    float4 x0, x1;
    x0.xy = x[0];
    x1.xy = x[1];
#if SIMD_SSE
    float4 r01 = _mm_unpacklo_ps(x0, x1);
#else
    float4 r01 = vzip1q_f32(x0, x1);
#endif
    return (float2x2){r01.lo, r01.hi};
}

float3x3 transpose(const float3x3& x) {
    float4 x0, x1, x2;
    x0.xyz = x[0];
    x1.xyz = x[1];
    x2.xyz = x[2];
#if SIMD_SSE
    float4 t0 = _mm_unpacklo_ps(x0, x1);
    float4 t1 = _mm_unpackhi_ps(x0, x1);
    float4 r0 = t0; r0.hi = x2.lo;
    float4 r1 = _mm_shuffle_ps(t0, x2, 0xde);
    float4 r2 = x2; r2.lo = t1.lo;
#else
    float4 padding = { 0 };
    float4 t0 = vzip1q_f32(x0,x2);
    float4 t1 = vzip2q_f32(x0,x2);
    float4 t2 = vzip1q_f32(x1,padding);
    float4 t3 = vzip2q_f32(x1,padding);
    float4 r0 = vzip1q_f32(t0,t2);
    float4 r1 = vzip2q_f32(t0,t2);
    float4 r2 = vzip1q_f32(t1,t3);
#endif
    return (float3x3){r0.xyz, r1.xyz, r2.xyz};
}

// TODO: needs to transpose both ways
// float4x4 transpose(float3x4 x) { .. }
// float3x4 transpose(float4x4 x) { .. }
// SIMD_CALL float4x4 transpose(float3x4 x) { m = transpose(x); }
// SIMD_CALL float3x4 transpose(float4x4 x) { m = transpose(x); }

float4x4 transpose(const float4x4& x) {
    // NOTE: also _MM_TRANSPOSE4_PS using shuffles
    // but old Neon didn't really have shuffle

#if SIMD_SSE
    float4 t0 = _mm_unpacklo_ps(x[0],x[2]);
    float4 t1 = _mm_unpackhi_ps(x[0],x[2]);
    float4 t2 = _mm_unpacklo_ps(x[1],x[3]);
    float4 t3 = _mm_unpackhi_ps(x[1],x[3]);
    float4 r0 = _mm_unpacklo_ps(t0,t2);
    float4 r1 = _mm_unpackhi_ps(t0,t2);
    float4 r2 = _mm_unpacklo_ps(t1,t3);
    float4 r3 = _mm_unpackhi_ps(t1,t3);
#else
    float4 t0 = vzip1q_f32(x[0],x[2]);
    float4 t1 = vzip2q_f32(x[0],x[2]);
    float4 t2 = vzip1q_f32(x[1],x[3]);
    float4 t3 = vzip2q_f32(x[1],x[3]);
    float4 r0 = vzip1q_f32(t0,t2);
    float4 r1 = vzip2q_f32(t0,t2);
    float4 r2 = vzip1q_f32(t1,t3);
    float4 r3 = vzip2q_f32(t1,t3);
#endif
    return (float4x4){r0,r1,r2,r3};
}

// inverse
float2x2 inverse(const float2x2& x) {
    float invDet = 1.0f / determinant(x);
    if (invDet == 0.0f) return kfloat2x2_zero;
    
    float2x2 r = transpose(x);
    r[0] *= invDet;
    r[1] *= invDet;
    return r;
}

float3x3 inverse(const float3x3& x) {
    float invDet = 1.0f / determinant(x);
    if (invDet == 0.0f) return kfloat3x3_zero;
    
    float3x3 r;
    
    // this forms the adjoint
    r[0] = cross(x[1], x[2]) * invDet;
    r[1] = cross(x[2], x[0]) * invDet;
    r[2] = cross(x[0], x[1]) * invDet;
    return r;
}

float4x4 inverse(const float4x4& x) {
    // This is a full gje inverse
    
    float4x4 a(x), b(kfloat4x4_identity);
    bool inversionSucceeded = true;
    
    // As a evolves from original mat into identity -
    // b evolves from identity into inverse(a)
    uint32_t cols = float4x4::col;
    uint32_t rows = float4x4::row;
    
    // Loop over cols of a from left to right, eliminating above and below diag
    for (uint32_t j=0; j<rows; j++) {
        // Find largest pivot in column j among rows j..2
        uint32_t i1 = j;            // Row with largest pivot candidate
        for (uint32_t i=j+1; i<cols; i++) {
            if ( fabsf(a[i][j]) > fabsf(a[i1][j]) ) {
                i1 = i;
            }
        }
        
        // Swap rows i1 and j in a and b to put pivot on diagonal
        std::swap(a[i1], a[j]);
        std::swap(b[i1], b[j]);
    
        // Scale row j to have a unit diagonal
        float s = a[j][j];
        if ( s == 0.0f ) {
            inversionSucceeded = false;
            break;
        }
        
        s = 1.0f/s;
        b[j] *= s;
        a[j] *= s;
    
        // Eliminate off-diagonal elems in col j of a, doing identical ops to b
        for (uint32_t i=0; i<cols; i++ ) {
            if (i != j) {
                s = a[i][j];
                b[i] -= b[j] * s;
                a[i] -= a[j] * s;
            }
        }
    }
    
    if (!inversionSucceeded) {
        b = kfloat4x4_zero;
    }
    
    return b;
}

// determinant
// internal only ops
// TODO: could just be macros
inline float3 rotate1(float3 x) { return x.yzx; }
inline float3 rotate2(float3 x) { return x.zxy; }
inline float4 rotate1(float4 x) { return x.yzwx; }
inline float4 rotate2(float4 x) { return x.zwxy; }
inline float4 rotate3(float4 x) { return x.wxyz; }

float determinant(const float2x2& x) {
    return cross(x[0], x[1]);
}

float determinant(const float3x3& x) {
    return reduce_add(
            x[0]*(rotate1(x[1])*rotate2(x[2]) - rotate2(x[1])*rotate1(x[2])));
}

float determinant(const float4x4& x) {
    float4 codet = x[0]*(rotate1(x[1])*(rotate2(x[2])*rotate3(x[3])-rotate3(x[2])*rotate2(x[3])) +
      rotate2(x[1])*(rotate3(x[2])*rotate1(x[3])-rotate1(x[2])*rotate3(x[3])) +
      rotate3(x[1])*(rotate1(x[2])*rotate2(x[3])-rotate2(x[2])*rotate1(x[3])));
    return reduce_add(codet.even - codet.odd);
}

// trace
float trace(const float2x2& x) {
    return x[0].x + x[1].y;
}

float trace(const float3x3& x) {
    return x[0].x + x[1].y + x[2].z;
}

float trace(const float4x4& x) {
    return x[0].x + x[1].y + x[2].z + x[3].w;
}

// TODO: may want pre-transform on float3x4 since it's transposed
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
float2 mul(float2 y, const float2x2& x) {
    float2 r;
    r.x = dot(y, x[0]);
    r.y = dot(y, x[1]);
    return r;
}

float3 mul(float3 y, const float3x3& x) {
    float3 r;
    r.x = dot(y, x[0]);
    r.y = dot(y, x[1]);
    r.z = dot(y, x[2]);
    return r;
}

float4 mul(float4 y, const float4x4& x) {
    float4 r;
    r.x = dot(y, x[0]);
    r.y = dot(y, x[1]);
    r.z = dot(y, x[2]);
    r.w = dot(y, x[3]);
    return r;
}


// post-transform at least does a mul madd
float2 mul(const float2x2& x, float2 y) {
    float2 r = x[0] * y[0]; // no mul(v,v)
    r = muladd( x[1], y[1], r);
    return r;
}

float3 mul(const float3x3& x, float3 y) {
    float3 r = x[0] * y[0];
    r = muladd( x[1], y[1], r);
    r = muladd( x[2], y[2], r);
    return r;
}

float4 mul(const float4x4& x, float4 y) {
    float4 r = x[0] * y[0];
    r = muladd( x[1], y[1], r);
    r = muladd( x[2], y[2], r);
    r = muladd( x[3], y[3], r);
    return r;
}

// matrix muls using mul madd
float2x2 mul(const float2x2& x, const float2x2& y) {
    float2x2 r;
    
    // m * columns
    r[0] = mul(x, y[0]);
    r[1] = mul(x, y[1]);
    
    return r;
}

float3x3 mul(const float3x3& x, const float3x3& y) {
    float3x3 r;
    r[0] = mul(x, y[0]);
    r[1] = mul(x, y[1]);
    r[2] = mul(x, y[2]);
    return r;
}

float4x4 mul(const float4x4& x, const float4x4& y) {
    float4x4 r;
    r[0] = mul(x, y[0]);
    r[1] = mul(x, y[1]);
    r[2] = mul(x, y[2]);
    r[3] = mul(x, y[3]);
    return r;
}

// sub
float2x2 sub(const float2x2& x, const float2x2& y) {
    float2x2 r(x);
    r[0] -= y[0];
    r[1] -= y[1];
    return r;
}
float3x3 sub(const float3x3& x, const float3x3& y) {
    float3x3 r(x);
    r[0] -= y[0];
    r[1] -= y[1];
    r[2] -= y[2];
    return r;
}
float4x4 sub(const float4x4& x, const float4x4& y) {
    float4x4 r(x);
    r[0] -= y[0];
    r[1] -= y[1];
    r[2] -= y[2];
    r[3] -= y[3];
    return r;
}

// add
float2x2 add(const float2x2& x, const float2x2& y) {
    float2x2 r(x);
    r[0] += y[0];
    r[1] += y[1];
    return r;
}
float3x3 add(const float3x3& x, const float3x3& y) {
    float3x3 r(x);
    r[0] += y[0];
    r[1] += y[1];
    r[2] += y[2];
    return r;
}
float4x4 add(const float4x4& x, const float4x4& y) {
    float4x4 r(x);
    r[0] += y[0];
    r[1] += y[1];
    r[2] += y[2];
    r[3] += y[3];
    return r;
}

// equal
bool equal(const float2x2& x, const float2x2& y) {
    return all(x[0] == y[0] &
               x[1] == y[1]);
}
bool equal(const float3x3& x, const float3x3& y) {
    return all(x[0] == y[0] &
               x[1] == y[1] &
               x[2] == y[2]);
}
bool equal(const float4x4& x, const float4x4& y) {
    return all(x[0] == y[0] &
               x[1] == y[1] &
               x[2] == y[2] &
               x[3] == y[3]);
}



//---------------------------
// Start of cpp calls

//---------------

const float2& float2_zero(){ return kfloat2_zero; }
const float2& float2_ones(){ return kfloat2_ones; }

const float2& float2_posx(){ return kfloat2_posx; }
const float2& float2_posy(){ return kfloat2_posy; }

const float2& float2_negx(){ return kfloat2_negx; }
const float2& float2_negy(){ return kfloat2_negy; }

//----

const float3& float3_zero(){ return kfloat3_zero; }
const float3& float3_ones(){ return kfloat3_ones; }

const float3& float3_posx(){ return kfloat3_posx; }
const float3& float3_posy(){ return kfloat3_posy; }
const float3& float3_posz(){ return kfloat3_posz; }

const float3& float3_negx(){ return kfloat3_negx; }
const float3& float3_negy(){ return kfloat3_negy; }
const float3& float3_negz(){ return kfloat3_negz; }

//----

const float4& float4_zero(){ return kfloat4_zero; }
const float4& float4_ones(){ return kfloat4_ones; }

const float4& float4_posx(){ return kfloat4_posx; }
const float4& float4_posy(){ return kfloat4_posy; }
const float4& float4_posz(){ return kfloat4_posz; }
const float4& float4_posw(){ return kfloat4_posw; }

const float4& float4_negx(){ return kfloat4_negx; }
const float4& float4_negy(){ return kfloat4_negy; }
const float4& float4_negz(){ return kfloat4_negz; }
const float4& float4_negw(){ return kfloat4_negw; }

const float4& float4_posxw(){ return kfloat4_posxw; }
const float4& float4_posyw(){ return kfloat4_posyw; }
const float4& float4_poszw(){ return kfloat4_poszw; }

const float4& float4_negxw(){ return kfloat4_negxw; }
const float4& float4_negyw(){ return kfloat4_negyw; }
const float4& float4_negzw(){ return kfloat4_negzw; }

//---------------

const float2x2& float2x2::zero() { return kfloat2x2_zero; }
const float2x2& float2x2::identity() { return kfloat2x2_identity; }

const float3x3& float3x3::zero() { return kfloat3x3_zero; }
const float3x3& float3x3::identity() { return kfloat3x3_identity; }

const float3x4& float3x4::zero() { return kfloat3x4_zero; }
const float3x4& float3x4::identity() { return kfloat3x4_identity; }

const float4x4& float4x4::zero() { return kfloat4x4_zero; }
const float4x4& float4x4::identity() { return kfloat4x4_identity; }



#if SIMD_FLOAT

string vecf::str(float2 v) const {
    return kram::format("(%f %f)", v.x, v.y);
}
string vecf::str(float3 v) const {
    return kram::format("(%f %f %f)", v.x, v.y, v.z);
}
string vecf::str(float4 v) const {
    return kram::format("(%f %f %f %f)", v.x, v.y, v.z, v.w);
}
 
string vecf::str(const float2x2& m) const {
    return kram::format("%s\n%s\n",
        str(m[0]).c_str(), str(m[1]).c_str());
}
string vecf::str(const float3x3& m) const {
    return kram::format("%s\n%s\n%s\n",
        str(m[0]).c_str(), str(m[1]).c_str(), str(m[2]).c_str());
}
string vecf::str(const float4x4& m) const {
  return kram::format("%s\n%s\n%s\n%s\n",
      str(m[0]).c_str(), str(m[1]).c_str(),
      str(m[2]).c_str(), str(m[3]).c_str());
}

#endif // SIMD_FLOAT

#define FMT_SEP() s += "-----------\n"

string vecf::simd_configs() const {
    string s;
    
#define FMT_CONFIG(val) s += kram::format("%s: %d\n", #val, val);
    
    FMT_CONFIG(SIMD_SSE);
    FMT_CONFIG(SIMD_NEON);
    
#if SIMD_SSE
    bool hasAVX2 = false;
    bool hasF16C = false;
    bool hasFMA = false;
    
    #ifdef __AVX2__
    hasAVX2 = true;
    #endif
    #ifdef __F16C__
    hasF16C = true;
    #endif
    #ifdef __FMA__
    hasFMA = true;
    #endif
    
    s += kram::format("%s: %d\n", "AVX2", hasAVX2);
    s += kram::format("%s: %d\n", "F16C", hasF16C);
    s += kram::format("%s: %d\n", "FMA ", hasFMA);
    
    // fp-contract, etc ?
#endif
    
#if SIMD_NEON
    // any neon setting, arm64 version
#endif
    
    FMT_CONFIG(SIMD_FLOAT_EXT);
    FMT_CONFIG(SIMD_RENAME_TO_SIMD_NAMESPACE);
    FMT_CONFIG(SIMD_HALF_FLOAT16);
    
    FMT_CONFIG(SIMD_ACCELERATE_LIB);
#if SIMD_ACCELERATE_LIB
    FMT_CONFIG(SIMD_LIBRARY_VERSION);
#endif
    
    FMT_SEP();
    
    FMT_CONFIG(SIMD_HALF);
    FMT_CONFIG(SIMD_FLOAT);
    FMT_CONFIG(SIMD_DOUBLE);
   
    FMT_CONFIG(SIMD_INT);
    FMT_CONFIG(SIMD_CHAR);
    FMT_CONFIG(SIMD_SHORT);
    FMT_CONFIG(SIMD_LONG);
    
#undef FMT_CONFIG
    
    return s;
}

string vecf::simd_alignments() const {
    string s;
    
#define FMT_CONFIG(val) s += kram::format("%s: %zu %zu\n", #val, sizeof(val), __alignof(val));
    
    // TODO: add other types int, half?
    
#if SIMD_FLOAT
    FMT_SEP();
    
    FMT_CONFIG(float2);
    FMT_CONFIG(float3);
    FMT_CONFIG(float4);
    FMT_CONFIG(float8);
    //FMT_CONFIG(float16);
    
    FMT_CONFIG(float2x2);
    FMT_CONFIG(float3x3);
    FMT_CONFIG(float3x4);
    FMT_CONFIG(float4x4);
#endif
    
#if SIMD_DOUBLE
    FMT_SEP();
    
    FMT_CONFIG(double2);
    FMT_CONFIG(double3);
    FMT_CONFIG(double4);
    //FMT_CONFIG(double8);
    
//    FMT_CONFIG(double2x2);
//    FMT_CONFIG(double3x3);
//    FMT_CONFIG(double3x4);
//    FMT_CONFIG(double4x4);
#endif
    
#if SIMD_INT
    FMT_SEP();
    
    FMT_CONFIG(int2);
    FMT_CONFIG(int3);
    FMT_CONFIG(int4);
    FMT_CONFIG(int8);
    //FMT_CONFIG(int16);
#endif
    
#undef FMT_CONFIG
    
    return s;
}
    

//---------------

#if SIMD_HALF4_ONLY
 
#if SIMD_NEON

float4 float4m(half4 vv)
{
    return float4(vcvt_f32_f16(*(const float16x4_t*)&vv));
}
half4 half4m(float4 vv)
{
    return half4(vcvt_f16_f32(*(const float32x4_t*)&vv));
}

#endif // SIMD_NEON

#if SIMD_SSE

float4 float4m(half4 vv)
{
    // https://patchwork.ozlabs.org/project/gcc/patch/559BC75A.1080606@arm.com/
    // https://gcc.gnu.org/onlinedocs/gcc-7.5.0/gcc/Half-Precision.html
    // https://developer.arm.com/documentation/dui0491/i/Using-NEON-Support/Converting-vectors
    __m128i reg16 = _mm_setzero_si128();
    
    // TODO: switch to load low 64-bits, but don't know which one _mm_cvtsi32_si128(&vv.reg); ?
    // want 0 extend here, sse overuses int32_t when really unsigned and zero extended value
    reg16 = _mm_insert_epi16(reg16, vv[0], 0);
    reg16 = _mm_insert_epi16(reg16, vv[1], 1);
    reg16 = _mm_insert_epi16(reg16, vv[2], 2);
    reg16 = _mm_insert_epi16(reg16, vv[3], 3);
    
    return simd::float4(_mm_cvtph_ps(reg16));
}

half4 half4m(float4 vv)
{
    __m128i reg16 = _mm_cvtps_ph(*(const __m128*)&vv, 0);  // 4xfp32-> 4xfp16,  round to nearest-even
    
    // TODO: switch to store/steam, but don't know which one _mm_storeu_epi16 ?
    half4 val;  // = 0;
    
    // 0 extended
    val[0] = (half)_mm_extract_epi16(reg16, 0);
    val[1] = (half)_mm_extract_epi16(reg16, 1);
    val[2] = (half)_mm_extract_epi16(reg16, 2);
    val[3] = (half)_mm_extract_epi16(reg16, 3);
    return val;
}

#endif // SIMD_SSE
#endif // SIMD_HALF4_ONLY

#if SIMD_FLOAT

static quatf quatf_zero(0.0f, 0.0f, 0.0f, 0.0f);
static quatf quatf_identity(0.0f, 0.0f, 0.0f, 1.0f);

const quatf& quatf::zero() { return quatf_zero; }
const quatf& quatf::identity() { return quatf_identity; }

// can negate w, or xyz with -kCross
static const float4 kCross = {1.0f,1.0f,1.0f,-1.0f};

// can eliminate 4 shufs by using 4 constants, 2 q swizzles are dupes
static const float4 kConvertToMatrix = {0.0f,1.0f,2.0f,-2.0f};

quatf::quatf(float3 axis, float angleInRadians)
{
    float s, c;
    sincosf(angleInRadians * 0.5f, s, c);
    v = float4m(s * axis, c);
}

quatf inverse(quatf q)
{
    return quatf(normalize(q.v * -kCross)); // vec *, but goiong to get quad mul below
}

quatf operator*(quatf q1, quatf q2)
{
    // This is original
    //q1.y * q2.z - q1.z * q2.y + q1.x * q2.w + q1.w * q2.x,
    //q1.z * q2.x - q1.x * q2.z + q1.y * q2.w + q1.w * q2.y,
    //q1.x * q2.y - q1.y * q2.x + q1.z * q2.w + q1.w * q2.z,
    //q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z
    
    // quaternion multiplication is similar to a four-space cross-product
    float4 qv1 = q1.v;
    float4 qv2 = q2.v;

    // TODO: probably better to swizzle the kCross, but need 4 constants then
    return quatf
    (
        dot((qv1 * qv2.wzyx).xywz, kCross), // -zy
        dot((qv1 * qv2.zwxy).wyzx, kCross), // -xz
        dot((qv1 * qv2.yxwz).xwzy, kCross), // -yx
        dot(-qv1 * qv2, kCross) // +w, -xyz
    );
}

float3x3 float3x3m(quatf qq)
{
    // not doing normalize like original
    //q = normalize(q);
    
    float3x3 m;
    float3 t;
    
    float4 c = kConvertToMatrix;
    float4 q = qq.v;
    q *= -kCross;
    
    t = q.wzy * c.wzw;
    m[0] = q.yzx * t.zxy - q.zxy * t.yzx + c.yxx;
    
    t = q.zwx * c.wwz;
    m[1] = q.yzx * t.zxy - q.zxy * t.yzx + c.xyx;
    
    // orthonormal basis, so use cross product for last term
    m[2] = cross(m[0], m[1]); // 2 instr hlsl, 7 ops SSE
    
    return m;
}

float4x4 float4x4m(quatf qq)
{
    /* from Ken Shoemake's GGIV article
    float xs = x*s,      ys = y*s,          zs = z*s;
    float wx = w*xs,      wy = w*ys,     wz = w*zs;
    float xx = x*xs,      xy = x*ys,     xz = x*zs;
    float yy = y*ys,      yz = y*zs;
    float zz = z*zs;
   
    // original formulation
    return Mat44
            (
            1.0 - 2.0*(y*y + z*z),          2.0*(x*y - w*z),        2.0*(x*z + w*y), 0.0,
                  2.0*(x*y + w*z), 1.0 - 2.0*(x*x + z*z),        2.0*(y*z - w*x), 0.0,
                  2.0*(x*z - w*y),       2.0*(y*z + w*x),  1.0 - 2.0*(x*x + y*y), 0.0
                  0.0,                                  0.0,                       0.0, 1.0
            );
   */

    
    // not doing normalize like original
    //q = normalize(q);
    
    float4x4 m;
    float4 t;

    float4 c = kConvertToMatrix;
    float4 q = qq.v;
    q *= -kCross;

    // really just 3 values, so w is always 0
    t = q.wzyw * c.wzwx;
    m[0] = q.yzxw * t.zxyw - q.zxyw * t.yzxw + c.yxxx;
    
    // really just 3 values, so w is always 0
    t = q.zwxw * c.wwzx;
    m[1] = q.yzxw * t.zxyw - q.zxyw * t.yzxw + c.xyxx;
    
    // orthonormal basis, so use cross product for last term
    m[2] = float4m(cross(m[0].xyz, m[1].xyz), 0.0f); // 2 instr hlsl, 7 ops SSE
    m[3] = float4_posw();

    return m;
}

// Slow but constant velocity.
quatf slerp(quatf q0, quatf q1, float t)
{
    // find smallest angle, flip axis
    if (dot(q0.v, q1.v) < 0.0f)
        q1.v.xyz = -q1.v.xyz;
    
    // theta/2
    float cosThetaHalf = dot(q0.v, q1.v);
    if (cosThetaHalf >= 0.995f)
    {
        return lerp(q0,q1,t);
    }
    else
    {
        // expensive
        float thetaHalf = acosf(cosThetaHalf);
        float sinThetaHalf = sinf(thetaHalf);

        float s0 = sinf(thetaHalf * (1-t)) / sinThetaHalf;  // at t=0, s0 = 1
        float s1 = sinf(thetaHalf * t)     / sinThetaHalf;  // at t=1, s1 = 1

        return quatf(s0 * q0.v+ s1 * q1.v);
    }
}

// compute control points for a bezier spline segment
inline void quat_bezier_cp_impl(quatf q0, quatf q1, quatf q2,
              quatf& a1, quatf& b1)
{
    // TODO: find out of these were quat or vec mul?
    // Double(q0, q1);
    a1.v = 2.0f * dot(q0.v,q1.v) * q1.v - q0.v;
    
    // Bisect(a1, q2);
    a1.v = (a1.v + q2.v);
    a1.v = normalize(a1.v);
    
    // Double(a1, q1);
    b1.v = 2.0f * dot(a1.v,q1.v) * q1.v - a1.v;
}


// compute control points for a cubic bezier spline segment (quats must be smallest angle)
void quat_bezier_cp(quatf q0, quatf q1, quatf q2, quatf q3,
                     quatf& a1, quatf& b2)
{
    quatf b1, a2; // b1 unused calc
    quat_bezier_cp_impl(q0,q1,q1, a1,b1);
    quat_bezier_cp_impl(q1,q2,q3, a2,b2);
}


// spherical cubic bezier spline interpolation
// takes in contol points
quatf quat_bezer_slerp(quatf q0, quatf b, quatf c, quatf q1, float t)
{
    // deCastljau interpolation of the control points
    quatf mid(slerp(b, c, t));

    return slerp(slerp(slerp(q0, b, t), mid, t),
                 slerp(mid, slerp(c, q1, t), t),
        t);
}

// spherical cubic bezier spline interpolation
// takes in contol points
quatf quat_bezer_lerp(quatf q0, quatf b, quatf c, quatf q1, float t)
{
    // deCastljau interpolation of the control points
    quatf mid(lerp(b, c, t));

    return lerp(
                 lerp(lerp(q0, b, t), mid, t),
                 lerp(mid, lerp(c, q1, t), t),
        t);
}

#endif // SIMD_FLOAT


#if SIMD_FLOAT

void transpose_affine(float4x4& m)
{
    // TODO: see other tranpose not using shuffles and do that.
    
    // avoid copy and one shuffle
    float4 tmp3, tmp2, tmp1, tmp0;
                   
    // TOOD: use platform shuffles on Neon
    //  this is using sse2neon
    tmp0 = _mm_shuffle_ps(m[0], m[1], 0x44);
    tmp2 = _mm_shuffle_ps(m[0], m[1], 0xEE);

    tmp1 = _mm_shuffle_ps(m[2], m[3], 0x44);
    tmp3 = _mm_shuffle_ps(m[2], m[3], 0xEE);
                                                            
    m[0] = _mm_shuffle_ps(tmp0, tmp1, 0x88);
    m[1] = _mm_shuffle_ps(tmp0, tmp1, 0xDD);
    m[2] = _mm_shuffle_ps(tmp2, tmp3, 0x88);

    // skips m[3] - known 0001
}

float4x4 inverse_tr(const float4x4& mtx)
{
    float4x4 inverse(mtx);
    inverse[3] = float4_negw();  // will be flipped by matrix mul
    transpose_affine(inverse);  // handle rotation (R)inv = (R)T
    
    inverse[3] = inverse * (-mtx[3]); // 1 mul, 3 mads

    return inverse;
}

// invert a row vector matrix
float4x4 inverse_tru(const float4x4& mtx)
{
    bool success = false;
    
    float scaleX = length_squared(mtx[0]);
    
    float4x4 inverse(mtx);
    if (scaleX > 1e-6f) {
        inverse[3] = float4_negw();
        
        transpose_affine(inverse);

        // all rows/columns in orthogonal tfm have same magnitude with uniform scale
        float4 invScaleX = float4m(1.0f / scaleX); // inverse squared
        
        // scale the rotation matrix by scaling inverse
        inverse[0] *= invScaleX;
        inverse[1] *= invScaleX;
        inverse[2] *= invScaleX;

        // handle the translation
        inverse[3] = inverse * (-mtx[3]);
        
        success = true;
    }
    
    return inverse;
}

float4x4 float4x4_tr(float3 t, quatf r) {
    float4x4 m(float4x4::identity());
    m[3].xyz = t;
    m = m * float4x4m(r);
    return m;
}

// TODO: there are faster ways to apply post rot, post scale
float4x4 float4x4_trs(float3 t, quatf r, float3 scale) {
    float4x4 m(float4x4::identity());
    m[3].xyz = t;
    m = m * float4x4m(r);
    
    // TODO: *= not working
    m = m * float4x4(float4m(scale,1.0f));
    return m;
}

// leaving this in here, since it can be further optimized
float4x4 float4x4_tru(float3 t, quatf r, float scale) {
    return float4x4_trs(t, r, float3m(scale));
}

float4x4 inverse_trs(const float4x4& mtx)
{
    bool success = false;
    
    float4x4 inverse(mtx);
    
    // TODO: fix handling of failure
    // compute the scaling terms (4 dots)
    // float3 scale = calcScaleSquaredRowTfm(m);
    // if (all(scale > float3(1e-6f)) {
        inverse[3] = float4_negw(); // neccessary for simple inverse to hold
    
        transpose_affine(inverse);

        // this is cheaper than 3 dot's above, just mul/add
        float4 invScale = recip(inverse[0]*inverse[0] +
                                inverse[1]*inverse[1] +
                                inverse[2]*inverse[2]);
    
        // scale the rotation matrix by scaling inverse
        inverse[0] *= invScale;
        inverse[1] *= invScale;
        inverse[2] *= invScale;
        inverse[3] = inverse * (-mtx[3]);
        
        success = true;
    //}
    
    return inverse;
}

// TODO: make ctor to avoid returning large matrix
float4x4 float4x4m(char axis, float angleInRadians)
{
    float    sinTheta, cosTheta;
    sincosf(angleInRadians, sinTheta, cosTheta);
            
    float4x4 m;
    m[3] = float4_posw();
            
    switch(axis) {
        case 'x':
        {
            m[0] = float4_posx();
            m[1] = float4m(0.0f,  cosTheta, sinTheta, 0.0f);
            m[2] = float4m(0.0f, -sinTheta, cosTheta, 0.0f);
            break;
        }
        
        case 'y':
        {
            m[0] = float4m(cosTheta, 0.0f, -sinTheta, 0.0f);
            m[1] = float4_posy();
            m[2] = float4m(sinTheta, 0.0f,  cosTheta, 0.0f);
            break;
        }
        
        case 'z':
        {
            m[0] = float4m( cosTheta, sinTheta, 0.0f, 0.0f);
            m[1] = float4m(-sinTheta, cosTheta, 0.0f, 0.0f);
            m[2] = float4_posz();
            break;
        }
    }
    return m;
}


#endif // SIMD_FLOAT

} // namespace SIMD_NAMESPACE

#endif

// Note: float4a.h has a rcp and rsqrt ops, but they are approximate.
// Have real div and sqrt ops now.

