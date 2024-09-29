// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#if USE_SIMDLIB

// This requires __clang__ or gcc.
// Only really targeting __clang__ for development.
// Targeting x64 running AVX2 and Neon.
//
// gcc/clang vector extension:
// .lo and .hi are the first second halves of a vector,
// .even and .odd are the even- and odd-indexed elements of a vector.
// __builtin_shufflevector and __builtin_convertvector.
// also have .rgba built into the types
// So can emulate double4 on Neon using 2 registers.
// These extensions only work on a C typedef and auto convert to
//  _m128, _m128i, _m256, .. on Intel, and to float32x4_t on Neon.
//
// Apple simd lib has versioning, so the Accelerate lib provides optimized
// simd calls.  And if not present, then those calls use 4 math.h function calls
// or fail to build.  So v0 is basic functionaliy, and it's up to v6 on iOS 18.
//
// x64: Compile this with -march x86_64h (haswell) or -mavx2 -mf16c -fma
// arm64: Compile this with Neon enabled (should have fma)
//
// Intel SSE core has 3 or 4 32B units internally.
//   One core is shared by 2 Hyperthreads, but HT is being removed by Intel.
//   e-cores can only run AVX2, and don't have HT support.
// AVX-512 can drop to 1 unit rounding down.
// Chip frequencies drop using AVX2 and AVX-512.
// Trying to run AVX2 (32B) can even run slow on older Intel/AMD
//   with double-pumped 16B internal cores.
// New AMD runs double-pumped 32B instructions for AVX-512 on all cores.
//  AMD has e-cores, but no instruction limits like Intel.
//
// Intel SSE scalar ops used to run 2:1 for 1:4 ops of work
// but now it's 1:1, so these only keep value in sse register.
//
// This passes vector by value, and matrix by const reference to the ops.
// May want to reconsider const ref for long uint/float/double vectors.
// This assumes x64 calling conventions for passing registers.
// There are differing function calling conventions for passing first 4 values.
//
// Neon    32x 32B 128-bt
// SVE2    ?
// +fp16   _Float16 support
//
// SSE     16x 16B 128-bit
// AVX/2   16x 32B 256-bit
// AVX-512 16x 64B 512-bit (disabled on p-cores and dropped from e-cores on i9), 4 variants
// AVX10   32x 32B 256-bit (emulates 512-bit), 3 variants
//
// FMA     fp multiply add (clang v14)
// F16C    2 ops fp16 <-> fp32
// CRC32   instructions to enable fast crc ops (not using yet, but is in sse2neon.h)
//
// max vec size per register
// 16B      32B
// char16   char16?
// short8   short16
// uint4    uint8
// float4   float8
// double2  double4
//
// Metal Shading Language (MSL)
// supports up to half4
// supports up to float4
// no support for double.  cpu only
//
// HLSL and DX12/Vulkan support double on desktop, but not mobile?
//   and likely not on arm64 gpus.
//
//------------
// x64 -> arm64 emulators
// Prism   supports SSE4.2, no fma, no f16c
// Rosetta supports SSE4.2, no fma, no f16c
// Rosetta supports AVX2 (macOS 15.0)
//
//------------
// Types for 32B max vector size (2 Neon reg, 1 AVX/AVX2 reg)
// char2,3,4,8,16,32
// int2,3,4,8
//
// half2,3,4,8,16
// float2,3,4,8
// double2,3,4
//
//------------
// APX first introduced in 10th gen has APX extension
//   expands general purpose registers from 16 to 32.
//
// Intel chips
//  1 Nehalem,
//  2 Sandy Bridge,
//  3 Ivy Bridge,
//  4 Haswell,       AVX2
//  5 Broadwell,
//  6 Sky Lake,
//  7 Kaby Lake,
//  8 Coffee Lake,
//  9 Coffee Lake Refresh
// 10 Comet Lake,    APX
// 11 Rocket Lake,
// 12 Alder Lake,
// 13 Raptor Lake
//
// AMD chips
//
//
// Apple Silicon
// iPhone 5S has arm64 arm64-v?
//

//-----------------------------------

// Can override the namespace to your own.  This avoids collision with Apple simd.
#ifndef SIMD_NAMESPACE
#define SIMD_NAMESPACE simdk
#endif // SIMD_NAMESPACE

// only support avx2 and Neon, no avx-512 at first
#if defined __ARM_NEON
#define SIMD_SSE  0
#define SIMD_NEON 1
#elif defined __AVX2__ // x64 AVX2 or higher, can lower to AVX
#define SIMD_SSE  1
#define SIMD_NEON 0
#else
#warning unuspported simd arch
#endif

// a define to override setings from prefix file
#ifndef SIMD_CONFIG

// Vector and matrix types.  Currently only matrix types for SIMD_FLOAT, SIMD_DOUBLE.
// SIMD_INT must be kept on for conditional tests.
#define SIMD_HALF   1
#define SIMD_FLOAT  1
#define SIMD_DOUBLE 1

#define SIMD_INT    1
#define SIMD_CHAR   0
//#define SIMD_UCHAR  0
#define SIMD_SHORT  0
//#define SIMD_USHORT 0
#define SIMD_LONG   0
//#define SIMD_ULONG  0

// Whether to support > 4 length vecs with some ops
#define SIMD_FLOAT_EXT 0

// This means simd_float4 will come from this file instead of simd.h
#define SIMD_RENAME_TO_SIMD_NAMESPACE 0

#endif // SIMD_CONFIG

#if SIMD_HALF

// Android doesn't really have _Float16, so would need a u/int16_t mapped placeholder
// The not identifier means its a system type.
#if !__is_identifier(_Float16)
#define SIMD_HALF_FLOAT16 1
#else
#define SIMD_HALF_FLOAT16 0
#endif

#endif // SIMD_HALF

//-----------------------------------

// simplify calls
// const means it doesn't pull from global changing state (what about constants)
// and inline is needed or get unused static calls, always_inline forces inline
// of these mostly wrapper calls.
#define SIMD_CALL static inline __attribute__((__always_inline__,__const__,__nodebug__))

// op *=, +=, -=, /= mods the calling object, so can't be const
#define SIMD_CALL_OP static inline __attribute__((__always_inline__,__nodebug__))

// TODO: type1 simdk::log(float1)
// type##1 cppfunc(type##1 x);

#define macroVectorRepeatFnDecl(type, cppfunc) \
type##2 cppfunc(type##2 x); \
type##3 cppfunc(type##3 x); \
type##4 cppfunc(type##4 x); \

//------------

// aligned
#define macroVector1TypesStorage(type, name) \
typedef type name##1s; \
typedef __attribute__((__ext_vector_type__(2)))  type name##2s; \
typedef __attribute__((__ext_vector_type__(3)))  type name##3s; \
typedef __attribute__((__ext_vector_type__(4)))  type name##4s; \
typedef __attribute__((__ext_vector_type__(8)))  type name##8s; \
typedef __attribute__((__ext_vector_type__(16))) type name##16s; \
typedef __attribute__((__ext_vector_type__(32),__aligned__(16))) type name##32s; \

// packed
#define macroVector1TypesPacked(type, name) \
typedef type name##1p; \
typedef __attribute__((__ext_vector_type__(2),__aligned__(1)))  type name##2p; \
typedef __attribute__((__ext_vector_type__(3),__aligned__(1)))  type name##3p; \
typedef __attribute__((__ext_vector_type__(4),__aligned__(1)))  type name##4p; \
typedef __attribute__((__ext_vector_type__(8),__aligned__(1)))  type name##8p; \
typedef __attribute__((__ext_vector_type__(16),__aligned__(1))) type name##16p; \
typedef __attribute__((__ext_vector_type__(32),__aligned__(1))) type name##32p; \

// cpp rename for u/char
#define macroVector1TypesStorageRenames(cname, cppname) \
typedef ::cname##1s cppname##1; \
typedef ::cname##2s cppname##2; \
typedef ::cname##3s cppname##3; \
typedef ::cname##4s cppname##4; \
typedef ::cname##8s cppname##8; \
typedef ::cname##16s cppname##16; \
typedef ::cname##32s cppname##32; \

//------------


// aligned
#define macroVector2TypesStorage(type, name) \
typedef type name##1s; \
typedef __attribute__((__ext_vector_type__(2)))  type name##2s; \
typedef __attribute__((__ext_vector_type__(3)))  type name##3s; \
typedef __attribute__((__ext_vector_type__(4)))  type name##4s; \
typedef __attribute__((__ext_vector_type__(8)))  type name##8s; \
typedef __attribute__((__ext_vector_type__(16),__aligned__(16))) type name##16s; \
// also a 32

// packed
#define macroVector2TypesPacked(type, name) \
typedef type name##1p; \
typedef __attribute__((__ext_vector_type__(2),__aligned__(2)))  type name##2p; \
typedef __attribute__((__ext_vector_type__(3),__aligned__(2)))  type name##3p; \
typedef __attribute__((__ext_vector_type__(4),__aligned__(2)))  type name##4p; \
typedef __attribute__((__ext_vector_type__(8),__aligned__(2)))  type name##8p; \
typedef __attribute__((__ext_vector_type__(16),__aligned__(2))) type name##16p; \

// cpp rename for half, u/short
#define macroVector2TypesStorageRenames(cname, cppname) \
typedef ::cname##1s cppname##1; \
typedef ::cname##2s cppname##2; \
typedef ::cname##3s cppname##3; \
typedef ::cname##4s cppname##4; \
typedef ::cname##8s cppname##8; \
typedef ::cname##16s cppname##16; \

//------------

// aligned
#define macroVector4TypesStorage(type, name) \
typedef type name##1s; \
typedef __attribute__((__ext_vector_type__(2)))  type name##2s; \
typedef __attribute__((__ext_vector_type__(3)))  type name##3s; \
typedef __attribute__((__ext_vector_type__(4)))  type name##4s; \
typedef __attribute__((__ext_vector_type__(8),__aligned__(16)))  type name##8s; \
typedef __attribute__((__ext_vector_type__(16),__aligned__(16))) type name##16s; \

// packed
#define macroVector4TypesPacked(type, name) \
typedef type name##1p; \
typedef __attribute__((__ext_vector_type__(2),__aligned__(4)))  type name##2p; \
typedef __attribute__((__ext_vector_type__(3),__aligned__(4)))  type name##3p; \
typedef __attribute__((__ext_vector_type__(4),__aligned__(4)))  type name##4p; \
typedef __attribute__((__ext_vector_type__(8),__aligned__(4)))  type name##8p; \
typedef __attribute__((__ext_vector_type__(16),__aligned__(4))) type name##16p; \

// cpp rename for float, u/int
#define macroVector4TypesStorageRenames(cname, cppname) \
typedef ::cname##1s cppname##1; \
typedef ::cname##2s cppname##2; \
typedef ::cname##3s cppname##3; \
typedef ::cname##4s cppname##4; \
typedef ::cname##8s cppname##8; \
typedef ::cname##16s cppname##16; \

//------------

// aligned
#define macroVector8TypesStorage(type, name) \
typedef type name##1s; \
typedef __attribute__((__ext_vector_type__(2))) type name##2s; \
typedef __attribute__((__ext_vector_type__(3),__aligned__(16))) type name##3s; \
typedef __attribute__((__ext_vector_type__(4),__aligned__(16))) type name##4s; \
typedef __attribute__((__ext_vector_type__(8),__aligned__(16))) type name##8s; \

// packed
#define macroVector8TypesPacked(type, name) \
typedef type name##1p; \
typedef __attribute__((__ext_vector_type__(2),__aligned__(8))) type name##2p; \
typedef __attribute__((__ext_vector_type__(3),__aligned__(8))) type name##3p; \
typedef __attribute__((__ext_vector_type__(4),__aligned__(8))) type name##4p; \
typedef __attribute__((__ext_vector_type__(8),__aligned__(8))) type name##8p; \

// cpp rename for double, u/long
#define macroVector8TypesStorageRenames(cname, cppname) \
typedef ::cname##1s cppname##1; \
typedef ::cname##2s cppname##2; \
typedef ::cname##3s cppname##3; \
typedef ::cname##4s cppname##4; \
typedef ::cname##8s cppname##8; \

//-----------------------------------

#define macroMatrixOps(type) \
SIMD_CALL_OP type& operator*=(type& x, const type& y) { x = mul(x, y); return x; } \
SIMD_CALL_OP type& operator+=(type& x, const type& y) { x = add(x, y); return x; } \
SIMD_CALL_OP type& operator-=(type& x, const type& y) { x = sub(x, y); return x; } \
SIMD_CALL bool operator==(const type& x, const type& y) { return equal(x, y); } \
SIMD_CALL bool operator!=(const type& x, const type& y) { return !(x == y); } \
\
SIMD_CALL type operator-(const type& x, const type& y) { return sub(x,y); } \
SIMD_CALL type operator+(const type& x, const type& y) { return add(x,y); } \
SIMD_CALL type operator*(const type& x, const type& y) { return mul(x,y); } \
SIMD_CALL type::column_t operator*(const type::column_t& v, const type& y) { return mul(v,y); } \
SIMD_CALL type::column_t operator*(const type& x, const type::column_t& v) { return mul(x,v); } \

//-----------------------------------

// for u/int8_t, u/int32_t, u/int64_t etc
// using the built-in types now
//#include <inttypes.h>

//------------
// define count and alignment of core types

#ifdef __cplusplus
extern "C" {
#endif

//----------

#if SIMD_CHAR

// define c vector types
macroVector1TypesStorage(char, char)
macroVector1TypesPacked(char, char)

#endif

#if SIMD_SHORT

// define c vector types
macroVector2TypesStorage(short, short)
macroVector2TypesPacked(short, short)

#endif

#if SIMD_INT

// define c vector types
// Apple uses int type here (32-bit) instead of int32_t
macroVector4TypesStorage(int, int)
macroVector4TypesPacked(int, int)

#endif // SIMD_INT

#if SIMD_LONG

// define c vector types
macroVector8TypesStorage(long, long)
macroVector8TypesPacked(long, long)

#endif

#if SIMD_HALF

#if SIMD_HALF_FLOAT16
typedef _Float16 half;
#else
// This won't work with the operators.  Consider _fp16 storage type which does all math in fp32.
// But even Android doesn't support that with +fp16.
// TODO: use half struct here that can do math slowly (prob in fp32x4, then convert back)
typedef short half;
#endif // SIMD_HALF_FLOAT16

// This means math and conversions don't work, so have to use simd ops
#define SIMD_HALF4_ONLY !SIMD_HALF_FLOAT16

// Half isn't something that should have math ops yet.  Just useful as packed type.
// This does math, but really needs _Float16 to work properly for the operators.
// That's not available on Android devices like it should be, but the Neon
// fp16x4 <-> fp32x4 conversions are.

// define c vector types
macroVector2TypesStorage(half, half)
macroVector2TypesPacked(half, half)

// No matrix type defined right now.

#endif // SIMD_HALF

#if SIMD_FLOAT

// define c++ vector/matrix types
macroVector4TypesStorage(float, float)
macroVector4TypesPacked(float, float)

// storage type for matrix
typedef struct { float2s columns[2]; } float2x2s;
typedef struct { float3s columns[3]; } float3x3s;
typedef struct { float4s columns[3]; } float3x4s;
typedef struct { float4s columns[4]; } float4x4s;

#endif // SIMD_FLOAT

#if SIMD_DOUBLE

// define c vector/matrix types
macroVector8TypesStorage(double, double)
macroVector8TypesPacked(double, double)

// storage type for matrix
typedef struct { double2s columns[2]; } double2x2s;
typedef struct { double3s columns[3]; } double3x3s;
typedef struct { double4s columns[3]; } double3x4s;
typedef struct { double4s columns[4]; } double4x4s;

#endif // SIMD_DOUBLE

//----------

// This means simd_float4 will come from this file instead of simd.h
// c typedef rename to simd_ namespace.
#if SIMD_RENAME_TO_SIMD_NAMESPACE

#if SIMD_CHAR
macroVector1TypesStorageRenames(char, simd_char)
#endif // SIMD_CHAR

#if SIMD_SHORT
macroVector2TypesStorageRenames(short, simd_short)
#endif // SIMD_SHORT

#if SIMD_INT
macroVector4TypesStorageRenames(int, simd_int)
#endif // SIMD_INT

#if SIMD_LONG
macroVector8TypesStorageRenames(long, simd_long)
#endif // SIMD_INT


#if SIMD_HALF
macroVector2TypesStorageRenames(half, simd_half)
#endif // SIMD_HALF
                     
#if SIMD_FLOAT
macroVector4TypesStorageRenames(float, simd_float)
#endif // SIMD_FLOAT

#if SIMD_DOUBLE
macroVector8TypesStorageRenames(double, simd_double)
#endif // SIMD_DOUBLE

#endif // SIMD_RENAME_TO_SIMD_NAMESPACE

#ifdef __cplusplus
}
#endif

//-----------------------------------
// imlementation - only code simd arch specific

// Could have option to bury impls as function.  That would remove all simd headers from code,
// but would prevent inlining.  So these could do optional inlining.

#include <math.h> // for sqrt

#if SIMD_NEON
// neon types and intrinsics
#include <arm_neon.h>
// This converts sse to neon intrinsics, only really have 16B Neon with SVE2 on horizon
#include "sse2neon-arm64.h"
#else
// SSE intrinsics up to AVX-512, but depends on -march avx2 -mf16c -fma
#include <immintrin.h>
#endif // SIMD_NEON

//-------------------
// This is for C++ only
#ifdef __cplusplus

namespace SIMD_NAMESPACE {

// c++ typedef of the c vectors.  But these are namespaced.
// So they shouldn't conflict, or conflicts can be resolve easier than the c types.

#if SIMD_CHAR
macroVector4TypesStorageRenames(char, char)
#endif

#if SIMD_SHORT
macroVector4TypesStorageRenames(short, short)
#endif

#if SIMD_INT
macroVector4TypesStorageRenames(int, int)
#endif

#if SIMD_LONG
macroVector8TypesStorageRenames(long, long)
#endif


#if SIMD_HALF
macroVector2TypesStorageRenames(half, half)
#endif

#if SIMD_FLOAT
macroVector4TypesStorageRenames(float, float)
#endif

#if SIMD_DOUBLE
macroVector8TypesStorageRenames(double, double)
#endif

// using macros here cuts the ifdefs a lot
#define vec2to4(x) (x).xyyy
#define vec3to4(x) (x).xyzz
#define vec4to2(x) (x).xy
#define vec4to3(x) (x).xyz

// Note there is _mm_sqrt_ss() instead of this math op to keep in registers
#define sqrt_scalar(x) sqrtf(x)

#if SIMD_FLOAT && SIMD_NEON

// TODO: expose float2 ops on Neon.

SIMD_CALL float reduce_min(float4 x) {
    return vminvq_f32(x);
}

SIMD_CALL float reduce_max(float4 x) {
    return vmaxvq_f32(x);
}

SIMD_CALL float4 min(float4 x, float4 y) {
    // precise returns x on Nan
    return vminnmq_f32(x, y);
}

SIMD_CALL float4 max(float4 x, float4 y) {
    // precise returns x on Nan
    return vmaxnmq_f32(x, y);
}

SIMD_CALL float4 muladd(float4 x, float4 y, float4 t) {
    // requires __ARM_VFPV4__
    // t passed first unlike sse
    return vfmaq_f32(t, x,y);
}

SIMD_CALL float4 sqrt(float4 x) {
    return vsqrtq_f32(x);
}

// use sse2neon to port this for now
SIMD_CALL float4 reduce_addv(float4 x) {
    // 4:1 reduction
    x = _mm_hadd_ps(x, x); // xy = x+y,z+w
    x = _mm_hadd_ps(x, x); // x  = x+y
    return x.x; // repeat x to all values
}

SIMD_CALL float reduce_add(float4 x) {
    return reduce_addv(x).x;
}


#endif // SIMD_FLOAT && SIMD_NEON

#if SIMD_FLOAT && SIMD_INT && SIMD_SSE

// x64 doesn't seem to have a simd op for min/max reduce
SIMD_CALL float reduce_min(float4 x) {
    return fmin(fmin(x.x,x.y), fmin(x.z,x.w));
}

SIMD_CALL float reduce_max(float4 x) {
    return fmax(fmax(x.x,x.y), fmax(x.z,x.w));
}

// needs SIMD_INT
// needed for precise min/max calls below
SIMD_CALL float4 bitselect_forminmax(float4 x, float4 y, int4 mask) {
    return (float4)(((int4)x & ~mask) | ((int4)y & mask));
}

SIMD_CALL float4 min(float4 x, float4 y) {
    // precise returns x on Nan
    return bitselect_forminmax(_mm_min_ps(x, y), x, y != y);
}

SIMD_CALL float4 max(float4 x, float4 y) {
    // precise returns x on Nan
    return bitselect_forminmax(_mm_max_ps(x, y), x, y != y);
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

SIMD_CALL float4 sqrt(float4 x) {
    return _mm_sqrt_ps(x);
}

SIMD_CALL float4 reduce_addv(float4 x) {
    // 4:1 reduction
    x = _mm_hadd_ps(x, x); // xy = x+y,z+w
    x = _mm_hadd_ps(x, x); // x  = x+y
    return x.x; // repeat x to all values
}

SIMD_CALL float reduce_add(float4 x) {
    return reduce_addv(x).x;
}


#endif // SIMD_FLOAT && SIMD_INT && SIMD_SSE


// These take in int types, this is what comparison gens from a < b, etc.
#if SIMD_INT && SIMD_SSE

SIMD_CALL bool any(int2 x) {
    return _mm_movemask_ps(vec2to4(x)) & 0x3;
}
SIMD_CALL bool any(int4 x) {
    return _mm_movemask_ps((__m128)x);
}

SIMD_CALL bool all(int2 x) {
    return (_mm_movemask_ps(vec2to4(x)) & 0x3) == 0x3; // 2 bits
}
SIMD_CALL bool all(int4 x) {
    return _mm_movemask_ps((__m128)x) == 0xf; // 4 bits
}
#endif // SIMD_INT && SIMD_SSE

#if SIMD_INT && SIMD_NEON

SIMD_CALL bool any(int2 x) {
    return vmaxv_u32(x) & 0x80000000;
}
SIMD_CALL bool any(int4 x) {
    return vmaxvq_u32(x) & 0x80000000;
}

SIMD_CALL bool all(int2 x) {
    return vminv_u32(x) & 0x80000000;
}
SIMD_CALL bool all(int4 x) {
    return vminvq_u32(x) & 0x80000000;
}

#endif // SIMD_INT && SIMD_NEON

#if SIMD_FLOAT

// SSE4.1
SIMD_CALL float4 round(float4 vv) {
    return _mm_round_ps(vv, 0x8);  // round to nearest | exc
}
SIMD_CALL float4 ceil(float4 vv) {
    return _mm_ceil_ps(vv);
}
SIMD_CALL float4 floor(float4 vv) {
    return _mm_floor_ps(vv);
}

#endif // SIMD_FLOAT


// end of implementation
//-----------------------------------

// bit select
#if SIMD_INT

// bitselect
SIMD_CALL int2 bitselect(int2 x, int2 y, int2 mask) {
    return (x & ~mask) | (y & mask);
}
SIMD_CALL int3 bitselect(int3 x, int3 y, int3 mask) {
    return (x & ~mask) | (y & mask);
}
SIMD_CALL int4 bitselect(int4 x, int4 y, int4 mask) {
    return (x & ~mask) | (y & mask);
}
#endif // SIMD_INT

#if SIMD_FLOAT
#if SIMD_INT // && SIMD_FLOAT

// bitselect
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

#endif // SIMD_INT // && SIMD_FLOAT

// zeroext - internal helper
SIMD_CALL float4 zeroext(float2 x) {
    return (float4){x.x,x.y,0,0};
}
SIMD_CALL float4 zeroext(float3 x) {
    return (float4){x.x,x.y,x.z,0};
}

// any
SIMD_CALL bool any(int3 x) {
    return any(vec3to4(x));
}
SIMD_CALL bool all(int3 x) {
    return all(vec3to4(x));
}

// min
SIMD_CALL float2 min(float2 x, float2 y) {
    return vec4to2(min(vec2to4(x), vec2to4(y)));
}
SIMD_CALL float3 min(float3 x, float3 y) {
    return vec4to3(min(vec3to4(x), vec3to4(y)));
}

// max
SIMD_CALL float2 max(float2 x, float2 y) {
    return vec4to2(max(vec2to4(x), vec2to4(y)));
}
SIMD_CALL float3 max(float3 x, float3 y) {
    return vec4to3(max(vec3to4(x), vec3to4(y)));
}

// sqrt
SIMD_CALL float2 sqrt(float2 x) {
    return vec4to2(sqrt(vec2to4(x)));
}
SIMD_CALL float3 sqrt(float3 x) {
    return vec4to3(sqrt(vec3to4(x)));
}

// rsqrt
SIMD_CALL float4 rsqrt(float4 x) {
    // TODO: fixup near 0 ?
    // TODO: use _mm_div_ps if / doesn't use
    return 1.0f/sqrt(x);
}
SIMD_CALL float2 rsqrt(float2 x) {
    return vec4to2(rsqrt(vec2to4(x)));
}
SIMD_CALL float3 rsqrt(float3 x) {
    return vec4to3(rsqrt(vec3to4(x)));
}


// recip
SIMD_CALL float4 recip(float4 x) {
    // TODO: fixup near 0 ?
    // TODO: use _mm_div_ps if / doesn't use
    return 1.0f/x;
}
SIMD_CALL float2 recip(float2 x) {
    return vec4to2(recip(vec2to4(x)));
}
SIMD_CALL float3 recip(float3 x) {
    return vec4to3(recip(vec3to4(x)));
}

// reduce_add
SIMD_CALL float reduce_add(float2 x) {
    return reduce_add(zeroext(x));
}
SIMD_CALL float reduce_add(float3 x) {
    return reduce_add(zeroext(x));
}

// reduce_min - arm has float2 op
SIMD_CALL float reduce_min(float2 x) {
    return reduce_min(vec2to4(x));
}

SIMD_CALL float reduce_min(float3 x) {
    return reduce_min(vec3to4(x));
}

// reduce_max
SIMD_CALL float reduce_max(float2 x) {
    return reduce_max(vec2to4(x));
}

SIMD_CALL float reduce_max(float3 x) {
    return reduce_max(vec3to4(x));
}

// round (to nearest)
SIMD_CALL float2 round(float2 x) { return vec4to2(round(vec2to4(x))); }
SIMD_CALL float3 round(float3 x) { return vec4to3(round(vec3to4(x))); }

// ceil
SIMD_CALL float2 ceil(float2 x) { return vec4to2(ceil(vec2to4(x))); }
SIMD_CALL float3 ceil(float3 x) { return vec4to3(ceil(vec3to4(x))); }

// floor
SIMD_CALL float2 floor(float2 x) { return vec4to2(floor(vec2to4(x))); }
SIMD_CALL float3 floor(float3 x) { return vec4to3(floor(vec3to4(x))); }

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

float2 saturate(float2 x);
float3 saturate(float3 x);
float4 saturate(float4 x);

// muladd - arm has float2 op
SIMD_CALL float2 muladd(float2 x, float2 y, float2 t) {
    return vec4to2(muladd(vec2to4(x), vec2to4(y), vec2to4(t)));
}
SIMD_CALL float3 muladd(float3 x, float3 y, float3 t) {
    return vec4to3(muladd(vec3to4(x), vec3to4(y), vec3to4(t)));
}

// lerp - another easy one
SIMD_CALL float2 lerp(float2 x, float2 y, float2 t) {
    return x + t*(y - x);
}
SIMD_CALL float3 lerp(float3 x, float3 y, float3 t) {
    return x + t*(y - x);
}
SIMD_CALL float4 lerp(float4 x, float4 y, float4 t) {
    return x + t*(y - x);
}


// dot
SIMD_CALL float dot(float2 x, float2 y) {
    return reduce_add(x * y);
}
SIMD_CALL float dot(float3 x, float3 y) {
    return reduce_add(x * y);
}
SIMD_CALL float dot(float4 x, float4 y) {
    return reduce_add(x * y);
}

// length_squared
SIMD_CALL float length_squared(float2 x) {
    return reduce_add(x * x);
}
SIMD_CALL float length_squared(float3 x) {
    return reduce_add(x * x);
}
SIMD_CALL float length_squared(float4 x) {
    return reduce_add(x * x);
}

// length
SIMD_CALL float length(float2 x) {
    return sqrt_scalar(reduce_add(x * x));
}
SIMD_CALL float length(float3 x) {
    return sqrt_scalar(reduce_add(x * x));
}
SIMD_CALL float length(float4 x) {
    return sqrt_scalar(reduce_add(x * x));
}

// distance
SIMD_CALL float distance(float2 x, float2 y) {
    return length(x - y);
}
SIMD_CALL float distance(float3 x, float3 y) {
    return length(x - y);
}
SIMD_CALL float distance(float4 x, float4 y) {
    return length(x - y);
}

// normalize
// optimized by staying in reg
SIMD_CALL float4 normalize(float4 x) {
    // x * invlength(x)
    return x * rsqrt(reduce_addv(x * x)).x;
}
SIMD_CALL float2 normalize(float2 x) {
    return vec4to2(normalize(zeroext(x)));
}
SIMD_CALL float3 normalize(float3 x) {
    return vec4to3(normalize(zeroext(x)));
}

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


#if SIMD_FLOAT_EXT

// These are cpu only math.  None of the gpus support these long types.
// and MSL doesn't even support double.

// how important are 8/16 ops for float and 8 for double?  Could reduce with only doing up to 4.
// can see doing more ops on smaller types.  Slower when these have to route through simd4.
SIMD_CALL float8 clamp(float8 x, float8 min, float8 max) {
    return min(max(x, min), max);
}
SIMD_CALL float16 clamp(float16 x, float16 min, float16 max) {
    return min(max(x, min), max);
}

SIMD_CALL float reduce_min(float8 x) {
    return reduce_min(min(x.lo, x.hi));
}

SIMD_CALL float reduce_min(float16 x) {
    return fmin(reduce_min(x.lo), reduce_min(x.hi));
}

SIMD_CALL float reduce_max(float8 x) {
    return reduce_max(max(x.lo, x.hi));
}
SIMD_CALL float reduce_max(float16 x) {
    return fmax(reduce_max(x.lo), reduce_max(x.hi));
}

// need to convert float4 to 8/16
// TODO: float8 float8m(float4 x, float4 y)
// TODO: float16 float16m(float8 x, float8 y)

// how important are 8/16 ops for float and double?  Could reduce with only doing up to 4.
SIMD_CALL float8 muladd(float8 x, float4 y, float4 t) {
    return float8m(muladd(x.lo, y.lo, z.lo), muladd(x.hi, y.hi, z.hi));
}
SIMD_CALL float16 muladd(float4 x, float4 y, float4 t) {
    return float16m(muladd(x.lo, y.lo, z.lo), muladd(x.hi, y.hi, z.hi));
}

SIMD_CALL float8 lerp(float8 x, float8 y, float8 t) {
    return x + t*(y - x);
}
SIMD_CALL float16 lerp(float16 x, float16 y, float16 t) {
    return x + t*(y - x);
}

SIMD_CALL float reduce_add(float8 x) {
    return reduce_add(x.lo + x.hi);
}

SIMD_CALL float reduce_add(float16 x) {
    return reduce_add(x.lo + x.hi);
}

SIMD_CALL float normalize(float8 x) {
    return x / length(x);
}
SIMD_CALL float normalize(float16 x) {
    return x / length(x);
}

#endif // SIMD_FLOAT_EXT

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

// conversions
#if SIMD_INT // && SIMD_FLOAT
SIMD_CALL float2 float2m(int2 x) { return __builtin_convertvector(x, float2); }
SIMD_CALL float3 float3m(int3 x) { return __builtin_convertvector(x, float3); }
SIMD_CALL float4 float4m(int4 x) { return __builtin_convertvector(x, float4); }

SIMD_CALL int2 float2m(float2 x) { return __builtin_convertvector(x, int2); }
SIMD_CALL int3 float3m(float3 x) { return __builtin_convertvector(x, int3); }
SIMD_CALL int4 float4m(float4 x) { return __builtin_convertvector(x, int4); }

#endif

#if SIMD_HALF // && SIMD_FLOAT

#if SIMD_HALF4_ONLY

half4 half4m(float4 );
SIMD_CALL half2 half2m(float2 x) { return vec4to2(half4m(vec2to4(x))); }
SIMD_CALL half3 half3m(float3 x) { return vec4to3(half4m(vec3to4(x))); }

float4 float4m(half4 );
SIMD_CALL float2 float2m(half2 x) { return vec4to2(float4m(vec2to4(x))); }
SIMD_CALL float3 float3m(half3 x) { return vec4to3(float4m(vec3to4(x))); }

#else
SIMD_CALL float2 float2m(half2 x) { return __builtin_convertvector(x, float2); }
SIMD_CALL float3 float3m(half3 x) { return __builtin_convertvector(x, float3); }
SIMD_CALL float4 float4m(half4 x) { return __builtin_convertvector(x, float4); }

SIMD_CALL half2 half2m(float2 x) { return __builtin_convertvector(x, half2); }
SIMD_CALL half3 half3m(float3 x) { return __builtin_convertvector(x, half3); }
SIMD_CALL half4 half4m(float4 x) { return __builtin_convertvector(x, half4); }
#endif

#endif

#if SIMD_DOUBLE // && SIMD_FLOAT
SIMD_CALL double2 double2m(float2 x) { return __builtin_convertvector(x, double2); }
SIMD_CALL double3 double3m(float3 x) { return __builtin_convertvector(x, double3); }
SIMD_CALL double4 double4m(float4 x) { return __builtin_convertvector(x, double4); }

SIMD_CALL float2 float2m(double2 x) { return __builtin_convertvector(x, float2); }
SIMD_CALL float3 float3m(double3 x) { return __builtin_convertvector(x, float3); }
SIMD_CALL float4 float4m(double4 x) { return __builtin_convertvector(x, float4); }
#endif

//----

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

struct float2x2 : float2x2s
{
    // can be split out to traits
    static constexpr int col = 2;
    static constexpr int row = 2;
    using column_t = float2;
    using scalar_t = float;
    
    static const float2x2& zero();
    static const float2x2& identity();
    
    float2x2() { }  // default uninit
    explicit float2x2(float2 diag);
    float2x2(float2 c0, float2 c1)
    : float2x2s((float2x2s){c0, c1}) { }
    float2x2(const float2x2s& m)
    : float2x2s(m) { }
    
    // simd lacks these ops
    float2& operator[](int idx) { return columns[idx]; }
    const float2& operator[](int idx) const { return columns[idx]; }
};

struct float3x3 : float3x3s
{
    static constexpr int col = 3;
    static constexpr int row = 3;
    using column_t = float3;
    using scalar_t = float;
    
    // Done as wordy c funcs otherwize.  Funcs allow statics to init.
    static const float3x3& zero();
    static const float3x3& identity();
    
    float3x3() { }  // default uninit
    explicit float3x3(float3 diag);
    float3x3(float3 c0, float3 c1, float3 c2)
    : float3x3s((float3x3s){c0, c1, c2}) { }
    float3x3(const float3x3s& m)
    : float3x3s(m) { }
    
    float3& operator[](int idx) { return columns[idx]; }
    const float3& operator[](int idx) const { return columns[idx]; }
};

// This is mostly a transposed holder for a 4x4, so very few ops defined
// Can also serve as a SOA for some types of cpu math.
struct float3x4 : float3x4s
{
    static constexpr int col = 3;
    static constexpr int row = 4;
    using column_t = float4;
    using scalar_t = float;
    
    static const float3x4& zero();
    static const float3x4& identity();
    
    float3x4() { } // default uninit
    explicit float3x4(float3 diag);
    float3x4(float4 c0, float4 c1, float4 c2)
    : float3x4s((float3x4s){c0, c1, c2}) { }
    float3x4(const float3x4s& m)
    : float3x4s(m) { }
    
    float4& operator[](int idx) { return columns[idx]; }
    const float4& operator[](int idx) const { return columns[idx]; }
};

struct float4x4 : float4x4s
{
    static constexpr int col = 4;
    static constexpr int row = 4;
    using column_t = float4;
    using scalar_t = float;
    
    static const float4x4& zero();
    static const float4x4& identity();
    
    float4x4() { } // default uninit
    explicit float4x4(float4 diag);
    float4x4(float4 c0, float4 c1, float4 c2, float4 c3)
    : float4x4s((float4x4s){c0, c1, c2, c3}) { }
    float4x4(const float4x4s& m)
    : float4x4s(m) { }
    
    float4& operator[](int idx) { return columns[idx]; }
    const float4& operator[](int idx) const { return columns[idx]; }
};

// transposes to convert between matrix type
float4x4 float4x4m(const float3x4& m);
float3x4 float3x4m(const float4x4& m);

// set diangonal and rest to 0
float2x2 diagonal_matrix(float2 x);
float3x3 diagonal_matrix(float3 x);
float3x4 diagonal_matrix3x4(float3 x);
float4x4 diagonal_matrix(float4 x);

// using refs here, 3x3 and 4x4 are large to pass by value (3 simd regs)
float2x2 transpose(const float2x2& x);
float3x3 transpose(const float3x3& x);
float4x4 transpose(const float4x4& x);

// general inverses - faster ones for trs
float2x2 inverse(const float2x2& x);
float3x3 inverse(const float3x3& x);
float4x4 inverse(const float4x4& x);

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

#endif // SIMD_FLOAT

//---------------------------
// double vec/matrix

#if SIMD_DOUBLE && 0

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
SIMD_CALL double3 double3m(double2 v, float z) {
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

// TODO: would need matrix class derivations
// and all of the matrix ops, which then need vector ops, and need double
// constants.  So this starts to really add to codegen.  But double
// is one of the last bastions of cpu, since many gpu don't support it.

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

#endif // SIMD_DOUBLE

//----------------

#if SIMD_INT
SIMD_CALL int2 int2m(int x) {
    return x;
}
SIMD_CALL int2 int2m(int x, int y) {
    return {x,y};
}

SIMD_CALL int3 int3m(int x) {
    return x;
}
SIMD_CALL int3 int3m(int x, int y, int z) {
    return {x,y,z};
}
SIMD_CALL int3 int3m(int2 v, float z) {
    int3 r; r.xy = v; r.z = z; return r;
}


SIMD_CALL int4 int4m(int x) {
    return x;
}
SIMD_CALL int4 int4m(int2 xy, int2 zw) {
    int4 r; r.xy = xy; r.zw = zw; return r;
}
SIMD_CALL int4 int4m(int x, int y, int z, int w) {
    return {x,y,z,w};
}
SIMD_CALL int4 int4m(int3 v, float w) {
    int4 r; r.xyz = v; r.w = w; return r;
}
#endif

#if SIMD_HALF
SIMD_CALL half2 half2m(half x) {
    return x;
}
SIMD_CALL half2 half2m(half x, half y) {
    return {x,y};
}

SIMD_CALL half3 half3m(half x) {
    return x;
}
SIMD_CALL half3 half3m(half x, half y, half z) {
    return {x,y,z};
}
SIMD_CALL half3 half3m(half2 v, float z) {
    half3 r; r.xy = v; r.z = z; return r;
}

SIMD_CALL half4 half4m(half x) {
    return x;
}
SIMD_CALL half4 half4m(half2 xy, half2 zw) {
    half4 r; r.xy = xy; r.zw = zw; return r;
}
SIMD_CALL half4 half4m(half x, half y, half z, half w = (half)1.0) {
    return {x,y,z,w};
}
SIMD_CALL half4 half4m(half3 v, float w = (half)1.0) {
    half4 r; r.xyz = v; r.w = w; return r;
}
#endif


//---------------------------

using namespace STL_NAMESPACE;

// Usage:
// vecf vfmt(fmtToken);
// fprintf(stdout, "%s", vfmt.str(v1).c_str() );
// This may seem extreme to pass string, but it has SSO and keeps temp alive to printf.
struct vecf {
    // TODO: add formatting options too
    vecf() {
    }
   
#if SIMD_FLOAT
    // vector
    string str(float2 v) const;
    string str(float3 v) const;
    string str(float4 v) const;
    
    // matrix
    string str(const float2x2& m) const;
    string str(const float3x3& m) const;
    string str(const float4x4& m) const;
    
    string quat(quatf q) { return str(q.v); }
    
#endif // SIMD_FLOAT
    
    // Just stuffing this here for now
    string simd_configs() const;
    string simd_alignments() const;
    
    // TODO: add double, int, half printing
};

} // namespace SIMD_NAMESPACE

#endif // __cplusplus

#endif

