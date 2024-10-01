// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#if USE_SIMDLIB && SIMD_FLOAT

// This is not yet standalone.  vectormath++.h includes it.

#ifdef __cplusplus
extern "C" {
#endif

// define c++ vector/matrix types
macroVector4TypesAligned(float, float)
macroVector4TypesPacked(float, float)

// storage type for matrix
typedef struct { float2a columns[2]; } float2x2a;
typedef struct { float3a columns[3]; } float3x3a;
typedef struct { float4a columns[3]; } float3x4a;
typedef struct { float4a columns[4]; } float4x4a;

// glue to Accelerate
#if SIMD_ACCELERATE_MATH_NAMES
macroVector4TypesStorageRenames(float, simd_float)
#endif // SIMD_ACCELERATE_MATH_NAMES

#ifdef __cplusplus
}

namespace SIMD_NAMESPACE {

macroVector4TypesStorageRenames(float, float)

//-----------------------------------
// start of implementation

// zeroext - internal helper
SIMD_CALL float4 zeroext(float2 x) {
    float4 v = 0; v.xy = x; return v;
}
SIMD_CALL float4 zeroext(float3 x) {
    float4 v = 0; v.xyz = x; return v;
}

#if SIMD_NEON

// DONE: expose float2 ops on Neon.
// q = 4, nothing = 2

SIMD_CALL float reduce_min(float2 x) { return vminv_f32(x); }
SIMD_CALL float reduce_min(float4 x) { return vminvq_f32(x); }

SIMD_CALL float reduce_max(float2 x) { return vmaxv_f32(x); }
SIMD_CALL float reduce_max(float4 x) { return vmaxvq_f32(x); }

// precise returns x on Nan
SIMD_CALL float2 min(float2 x, float2 y) { return vminnm_f32(x, y); }
SIMD_CALL float4 min(float4 x, float4 y) { return vminnmq_f32(x, y); }

// precise returns x on Nan
SIMD_CALL float2 max(float2 x, float2 y) { return vmaxnm_f32(x, y); }
SIMD_CALL float4 max(float4 x, float4 y) { return vmaxnmq_f32(x, y); }

// requires __ARM_VFPV4__
// t passed first unlike sse
SIMD_CALL float2 muladd(float2 x, float2 y, float2 t) { return vfma_f32(t, x,y); }
SIMD_CALL float4 muladd(float4 x, float4 y, float4 t) { return vfmaq_f32(t, x,y); }

SIMD_CALL float2 sqrt(float2 x) { return vsqrt_f32(x); }
SIMD_CALL float4 sqrt(float4 x) { return vsqrtq_f32(x); }

SIMD_CALL float2 reduce_addv(float2 x) {
    x = vpadd_f32(x, x);
    return x.x; // repeat x to all values
}
SIMD_CALL float4 reduce_addv(float4 x) {
    // 4:1 reduction
    x = vpaddq_f32(x, x); // xy = x+y,z+w
    x = vpaddq_f32(x, x); // x  = x+y
    return x.x; // repeat x to all values
}
SIMD_CALL float3 reduce_addv(float3 x) {
    return reduce_addv(zeroext(x)).x; // repeat
}

// round to nearest | exc
SIMD_CALL float2 round(float2 vv) { return vrndn_f32(vv); }
SIMD_CALL float4 round(float4 vv) { return vrndnq_f32(vv); }

SIMD_CALL float2 ceil(float2 vv) { return vrndp_f32(vv); }
SIMD_CALL float4 ceil(float4 vv) { return vrndpq_f32(vv); }

SIMD_CALL float2 floor(float2 vv) { return vrndm_f32(vv); }
SIMD_CALL float4 floor(float4 vv) { return vrndmq_f32(vv); }

#endif // SIMD_NEON

#if SIMD_SSE

// x64 doesn't seem to have a simd op for min/max reduce
SIMD_CALL float reduce_min(float4 x) {
    return fmin(fmin(x.x,x.y), fmin(x.z,x.w));
}
SIMD_CALL float reduce_min(float2 x) {
    return reduce_min(vec2to4(x));
}

SIMD_CALL float reduce_max(float4 x) {
    return fmax(fmax(x.x,x.y), fmax(x.z,x.w));
}
SIMD_CALL float reduce_max(float2 x) {
    return reduce_max(vec2to4(x));
}

// needs SIMD_INT
// needed for precise min/max calls below
#if SIMD_INT
SIMD_CALL float4 bitselect_forminmax(float4 x, float4 y, int4 mask) {
    return (float4)(((int4)x & ~mask) | ((int4)y & mask));
}
#endif

SIMD_CALL float4 min(float4 x, float4 y) {
    // precise returns x on Nan
    return bitselect_forminmax(_mm_min_ps(x, y), x, y != y);
}
SIMD_CALL float2 min(float2 x, float2 y) {
    return vec4to2(min(vec2to4(x), vec2to4(y)));
}

SIMD_CALL float4 max(float4 x, float4 y) {
    // precise returns x on Nan
    return bitselect_forminmax(_mm_max_ps(x, y), x, y != y);
}
SIMD_CALL float2 max(float2 x, float2 y) {
    return vec4to2(max(vec2to4(x), vec2to4(y)));
}

SIMD_CALL float4 muladd(float4 x, float4 y, float4 t) {
    // can't get Xcode to set -mfma with AVX2 set
#ifdef __FMA__
    return _mm_fmadd_ps(x,y,t);
#else
    // fallback with not same characteristics
    return x * y + t;
#endif
}
SIMD_CALL float2 muladd(float2 x, float2 y, float2 t) {
    return vec4to2(muladd(vec2to4(x), vec2to4(y), vec2to4(t)));
}

SIMD_CALL float4 sqrt(float4 x) {
    return _mm_sqrt_ps(x);
}
SIMD_CALL float2 sqrt(float2 x) {
    return vec4to2(sqrt(vec2to4(x)));
}

SIMD_CALL float4 reduce_addv(float4 x) {
    // 4:1 reduction
    x = _mm_hadd_ps(x, x); // xy = x+y,z+w
    x = _mm_hadd_ps(x, x); // x  = x+y
    return x.x; // repeat x to all values
}
SIMD_CALL float2 reduce_addv(float2 x) {
    return reduce_addv(zeroext(x)).x;
}
SIMD_CALL float3 reduce_addv(float3 x) {
    return reduce_addv(zeroext(x)).x;
}

// SSE4.1
SIMD_CALL float4 round(float4 vv) {
    // round to nearest | exc
    return _mm_round_ps(vv, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
}
SIMD_CALL float2 round(float2 x) {
    return vec4to2(round(vec2to4(x)));
}

SIMD_CALL float4 ceil(float4 vv) {
    return _mm_ceil_ps(vv);
}
SIMD_CALL float2 ceil(float2 x) {
    return vec4to2(ceil(vec2to4(x)));
}

SIMD_CALL float4 floor(float4 vv) {
    return _mm_floor_ps(vv);
}
SIMD_CALL float2 floor(float2 x) {
    return vec4to2(floor(vec2to4(x)));
}

#endif // SIMD_INT && SIMD_SSE



// end of implementation
//-----------------------------------
#if SIMD_INT

// bitselect
// Hoping these casts float2 -> int2 don't truncate
//  want this to map to _mm_cast calls
SIMD_CALL float2 bitselect(float2 x, float2 y, int2 mask) {
    return (float2)bitselect((int2)x, (int2)y, mask);
}
SIMD_CALL float3 bitselect(float3 x, float3 y, int3 mask) {
    return (float3)bitselect((int3)x, (int3)y, mask);
}
SIMD_CALL float4 bitselect(float4 x, float4 y, int4 mask) {
    return (float4)bitselect((int4)x, (int4)y, mask);
}

// select
SIMD_CALL float2 select(float2 x, float2 y, int2 mask) {
    return bitselect(x, y, mask >> 31);
}
SIMD_CALL float3 select(float3 x, float3 y, int3 mask) {
    return bitselect(x, y, mask >> 31);
}
SIMD_CALL float4 select(float4 x, float4 y, int4 mask) {
    return bitselect(x, y, mask >> 31);
}

#endif // SIMD_INT

// TODO: consider casts instead of shuffles below, at least on inputs
// float3 same size as float4, can't use cast on reduce calls.

// float3 leftovers
SIMD_CALL float3 min(float3 x, float3 y) { return vec4to3(min(vec3to4(x), vec3to4(y))); }
SIMD_CALL float3 max(float3 x, float3 y) { return vec4to3(max(vec3to4(x), vec3to4(y))); }
SIMD_CALL float3 muladd(float3 x, float3 y, float3 t) { return vec4to3(muladd(vec3to4(x), vec3to4(y), vec3to4(t))); }
SIMD_CALL float reduce_min(float3 x) { return reduce_min(vec3to4(x)); }
SIMD_CALL float reduce_max(float3 x) { return reduce_max(vec3to4(x)); }
SIMD_CALL float3 round(float3 x) { return vec4to3(round(vec3to4(x))); }
SIMD_CALL float3 ceil(float3 x) { return vec4to3(ceil(vec3to4(x))); }
SIMD_CALL float3 floor(float3 x) { return vec4to3(floor(vec3to4(x))); }
SIMD_CALL float3 sqrt(float3 x) { return vec4to3(sqrt(vec3to4(x))); }

// rsqrt
SIMD_CALL float4 rsqrt(float4 x) { return 1.0f/sqrt(x); }
SIMD_CALL float2 rsqrt(float2 x) { return 1.0f/sqrt(x); }
SIMD_CALL float3 rsqrt(float3 x) { return 1.0f/sqrt(x); }

// recip
SIMD_CALL float4 recip(float4 x) { return 1.0f/x; }
SIMD_CALL float2 recip(float2 x) { return 1.0f/x; }
SIMD_CALL float3 recip(float3 x) { return 1.0f/x; }

SIMD_CALL float reduce_add(float2 x) { return reduce_addv(x).x; }
SIMD_CALL float reduce_add(float3 x) { return reduce_addv(x).x; }
SIMD_CALL float reduce_add(float4 x) { return reduce_addv(x).x; }

// clamp
// order matters here for Nan, left op returned on precise min/max
SIMD_CALL float2 clamp(float2 x, float2 minv, float2 maxv) {
    return min(maxv, max(minv, x));
}
SIMD_CALL float3 clamp(float3 x, float3 minv, float3 maxv) {
    return min(maxv, max(minv, x));
}
SIMD_CALL float4 clamp(float4 x, float4 minv, float4 maxv) {
    return min(maxv, max(minv, x));
}

// saturate
SIMD_CALL float2 saturate(float2 x) { return clamp(x, 0, (float2)1); }
SIMD_CALL float3 saturate(float3 x) { return clamp(x, 0, (float3)1); }
SIMD_CALL float4 saturate(float4 x) { return clamp(x, 0, (float4)1); }

// lerp - another easy one, could use muladd(t, y-x, x)
SIMD_CALL float2 lerp(float2 x, float2 y, float2 t) { return x + t*(y - x); }
SIMD_CALL float3 lerp(float3 x, float3 y, float3 t) { return x + t*(y - x); }
SIMD_CALL float4 lerp(float4 x, float4 y, float4 t) { return x + t*(y - x); }

// dot
SIMD_CALL float dot(float2 x, float2 y) { return reduce_add(x * y); }
SIMD_CALL float dot(float3 x, float3 y) { return reduce_add(x * y); }
SIMD_CALL float dot(float4 x, float4 y) { return reduce_add(x * y); }

// length_squared
SIMD_CALL float length_squared(float2 x) { return reduce_add(x * x); }
SIMD_CALL float length_squared(float3 x) { return reduce_add(x * x); }
SIMD_CALL float length_squared(float4 x) { return reduce_add(x * x); }

// length
SIMD_CALL float length(float2 x) { return ::sqrt(reduce_add(x * x)); }
SIMD_CALL float length(float3 x) { return ::sqrt(reduce_add(x * x)); }
SIMD_CALL float length(float4 x) { return ::sqrt(reduce_add(x * x)); }

// distance
SIMD_CALL float distance(float2 x, float2 y) { return length(x - y); }
SIMD_CALL float distance(float3 x, float3 y) { return length(x - y); }
SIMD_CALL float distance(float4 x, float4 y) { return length(x - y); }

// normalize
// optimized by staying in reg
// x * invlength(x)
SIMD_CALL float4 normalize(float4 x) { return x * rsqrt(reduce_addv(x * x)).x; }
SIMD_CALL float2 normalize(float2 x) { return x * rsqrt(reduce_addv(x * x)).x; }
SIMD_CALL float3 normalize(float3 x) { return x * rsqrt(reduce_addv(x * x)).x; }

// abs
SIMD_CALL float2 abs(float2 x) {
    return bitselect(0.0, x, 0x7fffffff);
}
SIMD_CALL float3 abs(float3 x) {
    return bitselect(0.0, x, 0x7fffffff);
}
SIMD_CALL float4 abs(float4 x) {
    return bitselect(0.0, x, 0x7fffffff);
}

// cross
SIMD_CALL float cross(float2 x, float2 y) {
    return x.x * y.y - x.y * y.x;
}
SIMD_CALL float3 cross(float3 x, float3 y) {
    return x.yzx * y.zxy - x.zxy * y.yzx;
}

// equal_abs
SIMD_CALL bool equal_abs(float2 x, float2 y, float tol) {
    return all((abs(x - y) <= tol));
}
SIMD_CALL bool equal_abs(float3 x, float3 y, float tol) {
    return all((abs(x - y) <= tol));
}
SIMD_CALL bool equal_abs(float4 x, float4 y, float tol) {
    return all((abs(x - y) <= tol));
}

// equal_rel
SIMD_CALL bool equal_rel(float2 x, float2 y, float tol) {
    return all((abs(x - y) <= tol * ::abs(x.x)));
}
SIMD_CALL bool equal_rel(float3 x, float3 y, float tol) {
    return all((abs(x - y) <= tol * ::abs(x.x)));
}
SIMD_CALL bool equal_rel(float4 x, float4 y, float tol) {
    return all((abs(x - y) <= tol * ::abs(x.x)));
}

// step
SIMD_CALL float2 step(float2 edge, float2 x) {
    return bitselect((float2)1, 0, x < edge);
}
SIMD_CALL float3 step(float3 edge, float3 x) {
    return bitselect((float3)1, 0, x < edge);
}
SIMD_CALL float4 step(float4 edge, float4 x) {
    return bitselect((float4)1, 0, x < edge);
}

// smoothstep
SIMD_CALL float2 smoothstep(float2 edge0, float2 edge1, float2 x) {
    float2 t = saturate((x-edge0)/(edge0-edge1));
    return t*t*(3 - 2*t);
}
SIMD_CALL float3 smoothstep(float3 edge0, float3 edge1, float3 x) {
    float3 t = saturate((x-edge0)/(edge0-edge1));
    return t*t*(3 - 2*t);
}
SIMD_CALL float4 smoothstep(float4 edge0, float4 edge1, float4 x) {
    float4 t = saturate((x-edge0)/(edge0-edge1));
    return t*t*(3 - 2*t);
}

// fract
SIMD_CALL float2 fract(float2 x) {
    return min(x - floor(x), 0x1.fffffep-1f);
}
SIMD_CALL float3 fract(float3 x) {
    return min(x - floor(x), 0x1.fffffep-1f);
}
SIMD_CALL float4 fract(float4 x) {
    return min(x - floor(x), 0x1.fffffep-1f);
}

/* this is just to show examples of extended vector types, float8 should move out
   
#if SIMD_FLOAT_EXT

// These are cpu only math.  None of the gpus support these long types.
// and MSL doesn't even support double.
 // need to convert float4 to 8/16
 float8 float8m(float4 x, float4 y) {
 }
 float16 float16m(float8 x, float8 y) {
 }

// how important are 8/16 ops for float and 8 for double?  Could reduce with only doing up to 4.
// can see doing more ops on smaller types.  Slower when these have to route through simd4.
 
 
SIMD_CALL float8 clamp(float8 x, float8 min, float8 max) {
    return min(max(x, min), max);
}
SIMD_CALL float reduce_min(float8 x) {
     return reduce_min(min(x.lo, x.hi));
}
SIMD_CALL float reduce_max(float8 x) {
     return reduce_max(max(x.lo, x.hi));
}
SIMD_CALL float8 muladd(float8 x, float8 y, float8 t) {
     return float8m(muladd(x.lo, y.lo, z.lo), muladd(x.hi, y.hi, z.hi));
}
SIMD_CALL float8 lerp(float8 x, float8 y, float8 t) {
     return x + t*(y - x);
}
SIMD_CALL float reduce_add(float8 x) {
    return reduce_add(x.lo + x.hi);
}
SIMD_CALL float normalize(float8 x) {
     return x / length(x);
}
 
// float16 calling up to float8
SIMD_CALL float16 clamp(float16 x, float16 min, float16 max) {
    return min(max(x, min), max);
}
SIMD_CALL float reduce_min(float16 x) {
    return fmin(reduce_min(x.lo), reduce_min(x.hi));
}
SIMD_CALL float reduce_max(float16 x) {
    return fmax(reduce_max(x.lo), reduce_max(x.hi));
}
SIMD_CALL float16 muladd(float16 x, float16 y, float16 t) {
    return float16m(muladd(x.lo, y.lo, z.lo), muladd(x.hi, y.hi, z.hi));
}
SIMD_CALL float16 lerp(float16 x, float16 y, float16 t) {
    return x + t*(y - x);
}
SIMD_CALL float reduce_add(float16 x) {
    return reduce_add(x.lo + x.hi);
}
SIMD_CALL float normalize(float16 x) {
    return x / length(x);
}

#endif // SIMD_FLOAT_EXT
*/

// make "m" ctors for vecs.  This avoids wrapping the type in a struct.
// vector types are C typedef, and so cannot have member functions.
// Be careful with initializers = { val }, only sets first element of vector
// and not all the values.  Use = val; or one of the calls below to be safe.

SIMD_CALL float2 float2m(float x) {
    return x;
}
SIMD_CALL float2 float2m(float x, float y) {
    return {x,y};
}

SIMD_CALL float3 float3m(float x) {
    return x;
}
SIMD_CALL float3 float3m(float x, float y, float z) {
    return {x,y,z};
}
SIMD_CALL float3 float3m(float2 v, float z) {
    float3 r; r.xy = v; r.z = z; return r;
}

SIMD_CALL float4 float4m(float x) {
    return x;
}
SIMD_CALL float4 float4m(float2 xy, float2 zw) {
    float4 r; r.xy = xy; r.zw = zw; return r;
}
SIMD_CALL float4 float4m(float x, float y, float z, float w = 1.0f) {
    return {x,y,z,w};
}
SIMD_CALL float4 float4m(float3 v, float w = 1.0f) {
    float4 r; r.xyz = v; r.w = w; return r;
}

// fast conversions where possible
SIMD_CALL const float3& as_float3(const float4& m) {
    return reinterpret_cast<const float3&>(m);
}

// power series
macroVectorRepeatFnDecl(float, log)
macroVectorRepeatFnDecl(float, exp)

// trig
macroVectorRepeatFnDecl(float, cos)
macroVectorRepeatFnDecl(float, sin)
macroVectorRepeatFnDecl(float, tan)

// pow
// can xy be <= 0 ?, no will return Nan in log/exp approx
SIMD_CALL float2 pow(float2 x, float2 y) {
    return exp(log(x) * y);
}
SIMD_CALL float3 pow(float3 x, float3 y) {
    return exp(log(x) * y);
}
SIMD_CALL float4 pow(float4 x, float4 y) {
    return exp(log(x) * y);
}

// TODO: add more math ops

//-----------------------------------
// constants

// TODO: better way to name these, can there be float2::zero()
// also could maybe use that for fake vector ctors.

const float2& float2_zero();
const float2& float2_ones();

const float2& float2_posx();
const float2& float2_posy();
const float2& float2_negx();
const float2& float2_negy();

//----

const float3& float3_zero();
const float3& float3_ones();

const float3& float3_posx();
const float3& float3_posy();
const float3& float3_posz();
const float3& float3_negx();
const float3& float3_negy();
const float3& float3_negz();

//----

const float4& float4_zero();
const float4& float4_ones();

const float4& float4_posx();
const float4& float4_posy();
const float4& float4_posz();
const float4& float4_posw();
const float4& float4_negx();
const float4& float4_negy();
const float4& float4_negz();
const float4& float4_negw();

const float4& float4_posxw();
const float4& float4_posyw();
const float4& float4_poszw();
const float4& float4_negxw();
const float4& float4_negyw();
const float4& float4_negzw();

//-----------------------------------
// matrix

// column matrix, so postmul vectors
// (projToCamera * cameraToWorld * worldToModel) * modelVec

struct float2x2 : float2x2a
{
    // can be split out to traits
    static constexpr int col = 2;
    static constexpr int row = 2;
    using column_t = float2;
    using scalar_t = float;
    using base = float2x2a;
    
    static const float2x2& zero();
    static const float2x2& identity();
    
    float2x2() { }  // default uninit
    explicit float2x2(float2 diag);
    float2x2(float2 c0, float2 c1)
    : base((base){c0, c1}) { }
    float2x2(const base& m)
    : base(m) { }
    
    // simd lacks these ops
    float2& operator[](int idx) { return columns[idx]; }
    const float2& operator[](int idx) const { return columns[idx]; }
};

struct float3x3 : float3x3a
{
    static constexpr int col = 3;
    static constexpr int row = 3;
    using column_t = float3;
    using scalar_t = float;
    using base = float3x3a;
    
    // Done as wordy c funcs otherwize.  Funcs allow statics to init.
    static const float3x3& zero();
    static const float3x3& identity();
    
    float3x3() { }  // default uninit
    explicit float3x3(float3 diag);
    float3x3(float3 c0, float3 c1, float3 c2)
    : base((base){c0, c1, c2}) { }
    float3x3(const base& m)
    : base(m) { }
    
    float3& operator[](int idx) { return columns[idx]; }
    const float3& operator[](int idx) const { return columns[idx]; }
};

// This is mostly a transposed holder for a 4x4, so very few ops defined
// Can also serve as a SOA for some types of cpu math.
struct float3x4 : float3x4a
{
    static constexpr int col = 3;
    static constexpr int row = 4;
    using column_t = float4;
    using scalar_t = float;
    using base = float3x4a;
    
    static const float3x4& zero();
    static const float3x4& identity();
    
    float3x4() { } // default uninit
    explicit float3x4(float3 diag);
    float3x4(float4 c0, float4 c1, float4 c2)
    : base((base){c0, c1, c2}) { }
    float3x4(const float3x4a& m)
    : base(m) { }
    
    float4& operator[](int idx) { return columns[idx]; }
    const float4& operator[](int idx) const { return columns[idx]; }
};

struct float4x4 : float4x4a
{
    static constexpr int col = 4;
    static constexpr int row = 4;
    using column_t = float4;
    using scalar_t = float;
    using base = float4x4a;
    
    static const float4x4& zero();
    static const float4x4& identity();
    
    float4x4() { } // default uninit
    explicit float4x4(float4 diag);
    float4x4(float4 c0, float4 c1, float4 c2, float4 c3)
    : base((base){c0, c1, c2, c3}) { }
    float4x4(const base& m)
    : base(m) { }
    
    float4& operator[](int idx) { return columns[idx]; }
    const float4& operator[](int idx) const { return columns[idx]; }
};

// transposes to convert between matrix type
float4x4 float4x4m(const float3x4& m);
float3x4 float3x4m(const float4x4& m);

// set diagonal to vec and rest to 0
float2x2 diagonal_matrix(float2 x);
float3x3 diagonal_matrix(float3 x);
float3x4 diagonal_matrix3x4(float3 x);
float4x4 diagonal_matrix(float4 x);

// transpose
float2x2 transpose(const float2x2& x);
float3x3 transpose(const float3x3& x);
float4x4 transpose(const float4x4& x);

// general inverses - faster ones for trs
float2x2 inverse(const float2x2& x);
float3x3 inverse(const float3x3& x);
float4x4 inverse(const float4x4& x);

// determinant
float determinant(const float2x2& x);
float determinant(const float3x3& x);
float determinant(const float4x4& x);

// diagonal sum
float trace(const float2x2& x);
float trace(const float3x3& x);
float trace(const float4x4& x);

// m * m
float2x2 mul(const float2x2& x, const float2x2& y);
float3x3 mul(const float3x3& x, const float3x3& y);
float4x4 mul(const float4x4& x, const float4x4& y);

// vrow * m - premul = dot + premul
float2 mul(float2 y, const float2x2& x);
float3 mul(float3 y, const float3x3& x);
float4 mul(float4 y, const float4x4& x);

// m * vcol - postmul = mul + mad (prefer this)
float2 mul(const float2x2& x, float2 y);
float3 mul(const float3x3& x, float3 y);
float4 mul(const float4x4& x, float4 y);

// sub
float2x2 sub(const float2x2& x, const float2x2& y);
float3x3 sub(const float3x3& x, const float3x3& y);
float4x4 sub(const float4x4& x, const float4x4& y);

// add
float2x2 add(const float2x2& x, const float2x2& y);
float3x3 add(const float3x3& x, const float3x3& y);
float4x4 add(const float4x4& x, const float4x4& y);

// equal
bool equal(const float2x2& x, const float2x2& y);
bool equal(const float3x3& x, const float3x3& y);
bool equal(const float4x4& x, const float4x4& y);

// equal_abs
bool equal_abs(const float2x2& x, const float2x2& y, float tol);
bool equal_abs(const float3x3& x, const float3x3& y, float tol);
bool equal_abs(const float4x4& x, const float4x4& y, float tol);

// equal_rel
bool equal_rel(const float2x2& x, const float2x2& y, float tol);
bool equal_rel(const float3x3& x, const float3x3& y, float tol);
bool equal_rel(const float4x4& x, const float4x4& y, float tol);

// TODO: these think they are all member functions

// operators for C++
macroMatrixOps(float2x2);
macroMatrixOps(float3x3);
// TODO: no mat ops on storage type float3x4
// macroMatrixOps(float3x4s);
macroMatrixOps(float4x4);

// fast conversions where possible
SIMD_CALL const float3x3& as_float3x3(const float4x4& m) {
    return reinterpret_cast<const float3x3&>(m);
}

//-----------------------
// quat

// Only need a fp32 quat.  double/half are pretty worthless.
struct quatf {
    // TODO: should all ctor be SIMD_CALL ?
    quatf() : v{0.0f,0.0f,0.0f,1.0f} {}
    quatf(float x, float y, float z, float w) : v{x,y,z,w} {}
    quatf(float3 vv, float angle);
    explicit quatf(float4 vv): v(vv) {}
    
    static const quatf& zero();
    static const quatf& identity();
    
    float4 v;
};

SIMD_CALL float3 operator*(quatf q, float3 v) {
    float4 qv = q.v;
    float3 t = qv.w * cross(qv.xyz, v);
    return v + 2.0f * t + cross(q.v.xyz, t);
}

float4x4 float4x4m(quatf q);

// how many quatf ops are needed?
// TODO: need matrix into quatf
// TDOO: need shortest arc correction (dot(q0.v, q1.v) < 0) negate
// TODO: need negate (or conjuagate?)
// TODO: what about math ops

SIMD_CALL quatf lerp(quatf q0, quatf q1, float t) {
    if (dot(q0.v, q1.v) < 0.0f)
        q1.v.xyz = -q1.v.xyz;
    
    float4 v = lerp(q0.v, q1.v, t);
    return quatf(v);
}
quatf slerp(quatf q0, quatf q1, float t);

void quat_bezier_cp(quatf q0, quatf q1, quatf q2, quatf q3,
                    quatf& a1, quatf& b2);
quatf quat_bezer_lerp(quatf a, quatf b, quatf c, quatf d, float t);
quatf quat_bezer_slerp(quatf a, quatf b, quatf c, quatf d, float t);

quatf inverse(quatf q);

SIMD_CALL quatf normalize(quatf q) {
    return quatf(normalize(q.v));
}

//----------------
// affine and convenience ctors

// in-place affine transose
void transpose_affine(float4x4& m);

// fast inverses for translate, rotate, scale
float4x4 inverse_tr(const float4x4& mtx);
float4x4 inverse_tru(const float4x4& mtx);
float4x4 inverse_trs(const float4x4& mtx);

float4x4 float4x4m(char axis, float angleInRadians);

SIMD_CALL float4x4 float4x4m(float3 axis, float angleInRadians) {
    return float4x4m(quatf(axis, angleInRadians));
}

float3x3 float3x3m(quatf qq);

float4x4 float4x4_tr(float3 t, quatf r);
float4x4 float4x4_trs(float3 t, quatf r, float3 scale);
float4x4 float4x4_tru(float3 t, quatf r, float scale);

} // SIMD_NAMESPACE

#endif // __cplusplus

#endif // USE_SIMDLIB && SIMD_FLOAT
