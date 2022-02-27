// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#if defined(_WIN32)
#define KRAM_WIN 1
#elif __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#define KRAM_MAC 1
#else
#define KRAM_IOS 1
#endif
#elif __unix__
#define KRAM_LINUX 1
#endif

// define 0 if undfefined above
#ifndef KRAM_MAC
#define KRAM_MAC 0
#endif
#ifndef KRAM_WIN
#define KRAM_WIN 0
#endif
#ifndef KRAM_LINUX
#define KRAM_LINUX 0
#endif
#ifndef KRAM_IOS
#define KRAM_IOS 0
#endif

// TODO: add Profile build (rename RelWithDbgInfo)

#ifdef NDEBUG
#define KRAM_RELEASE 1
#define KRAM_DEBUG 0
#else
#define KRAM_RELEASE 0
#define KRAM_DEBUG 1
#endif

//------------------------

#if KRAM_WIN

// Disable warnings
// can set disable, once, or error

#pragma warning(disable : 4530 4305 4267 4996 4244 4305)

/* Couldn't seem to place these inside the open parens
    4530 // C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
    4305 // 'initializing': truncation from 'double'
    4267 // 'initializing': conversion from 'size_t' to 'uint32_t', possible loss of data
    4996 // 'fileno': The POSIX name for this item is deprecated. Instead, use the ISO C and C++ conformant name: _fileno. See online help for details.
    4244 // 'initializing': conversion from 'int32_t' to 'float', possible loss of data
    4305 // '*=': truncation from 'double' to 'float'
*/

#endif

//------------------------
#if KRAM_MAC

#if TARGET_CPU_X86_64
#define USE_SSE 1
#elif TARGET_CPU_ARM64
#define USE_NEON 1
#endif

#endif

#if KRAM_IOS
#define USE_NEON 1
#endif

//------------------------
#if KRAM_WIN

// avoid conflicts with min/max macros, use std instead
#define NOMINMAX

#include <SDKDDKVer.h>
//#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS 1
#include <tchar.h>

// For now assume Intel on Win
#define USE_SSE 1

#endif

//------------------------
// TODO: unix

//------------------------

// one of these must be set
#ifndef USE_SSE
#define USE_SSE 0
#endif
#ifndef USE_NEON
#define USE_NEON 0
#endif

// clang can compile simd/simd.h code on other platforms
// this provides vector extensions from gcc that were setup for OpenCL shaders
#ifndef USE_SIMDLIB
#if defined(__clang__)
#define USE_SIMDLIB 1
#else
#define USE_SIMDLIB 0
#endif
#endif

// use _Float16/_fp16 vs. other
#if KRAM_MAC || KRAM_IOS
#define USE_FLOAT16 1
#else
#define USE_FLOAT16 0
#endif


// can override from build system

// can't have ATE defined to 1 on other platforms
#if !(KRAM_MAC || KRAM_IOS)
#undef COMPILE_ATE
#endif

#ifndef COMPILE_ATE
#define COMPILE_ATE 0
#endif
#ifndef COMPILE_SQUISH
#define COMPILE_SQUISH 0
#endif
#ifndef COMPILE_ETCENC
#define COMPILE_ETCENC 0
#endif
#ifndef COMPILE_BCENC
#define COMPILE_BCENC 0
#endif
#ifndef COMPILE_ASTCENC
#define COMPILE_ASTCENC 0
#endif

#ifndef COMPILE_EASTL
#define COMPILE_EASTL 0
#endif

// basis transcoder only (read not writes)
#ifndef COMPILE_BASIS
#define COMPILE_BASIS 0
#endif

// rgb8/16f/32f formats only supported for import, Metal doesn't expose these formats
#ifndef SUPPORT_RGB
#define SUPPORT_RGB 1
#endif

// This needs debug support that native stl already has.
// EASTL only seems to define that for Visual Studio, and not lldb
#define USE_EASTL COMPILE_EASTL
#if USE_EASTL

// this probably breaks all STL debugging
#include <EASTL/algorithm.h>  // for max
//#include "EASTL/atomic.h"
#include <EASTL/functional.h>

#include <EASTL/deque.h>
#include <EASTL/iterator.h>    // for copy_if on Win
#include <EASTL/sort.h>
#include <EASTL/basic_string.h>

#include <EASTL/array.h>
#include <EASTL/map.h>
#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>

#include <EASTL/shared_ptr.h>  // includes thread/mutex
#include <EASTL/unique_ptr.h>
#include <EASTL/initializer_list.h>

#include <atomic>

#define NAMESPACE_STL eastl

#else

#include <algorithm>  // for max
#include <functional>

#include <deque>
#include <iterator>  // for copy_if on Win
#include <string>

#include <atomic>
#include <memory>    // for shared_ptr
#include <initializer_list>

#include <array>
#include <map>
#include <unordered_map>
#include <vector>

#define NAMESPACE_STL std

#endif

#if COMPILE_BASIS
#include "basisu_transcoder.h"
#endif

// includes that are usable across all files
#include "KramLog.h"

// this has every intrinsic header in it
#if USE_SSE
// to keep astcenc compiling
#include <immintrin.h>  // AVX1
#elif USE_NEON
#include "sse2neon.h"
#endif

// TODO: move half4 to it's own file, but always include it
// Apple's files don't have a half4 type.
namespace simd {

// This has spotty support on Android.  They left out hw support
// for _Float16 on many of the devices.  So there would need this fallback.

#if USE_FLOAT16
using half = _Float16;
#else
// for lack of a better type
using half = uint16_t;
#endif

// Really just a storage format and wrapper for half, math work done in float4.
class half4 {
public:
#if USE_SSE
    // for lack of a better type, not __m128i since that's 2x bigger
    using tType = uint64_t;
#elif USE_NEON
    using tType = float16x4_t;
#endif

    union {
        tType reg;
        half v[4];
        struct {
            half x, y, z, w;
        };
        struct {
            half r, g, b, a;
        };
    };

    half4() {}
    explicit half4(half val) : x(val), y(val), z(val), w(val) {}  // xyzw = val
    explicit half4(tType val) { reg = val; }
    half4(half xx, half yy, half zz, half ww) : x(xx), y(yy), z(zz), w(ww) {}
    half4(const half4& val) { reg = val.reg; }

    // no real ops here, althought Neon does have sevearal
    // use of these pull data out of simd registers
    half& operator[](int32_t index)
    {
        return v[index];
    }
    const half& operator[](int32_t index) const
    {
        return v[index];
    }
};

}  // namespace simd

#if !USE_EASTL

namespace std {
inline float clamp(float x, float minValue, float maxValue) { return min(max(x, minValue), maxValue); }
inline double clamp(double x, double minValue, double maxValue) { return min(max(x, minValue), maxValue); }

inline double clamp(int8_t x, int8_t minValue, int8_t maxValue) { return min(max(x, minValue), maxValue); }
inline double clamp(uint8_t x, uint8_t minValue, uint8_t maxValue) { return min(max(x, minValue), maxValue); }

inline double clamp(int16_t x, int16_t minValue, int16_t maxValue) { return min(max(x, minValue), maxValue); }
inline double clamp(uint16_t x, uint16_t minValue, uint16_t maxValue) { return min(max(x, minValue), maxValue); }

inline double clamp(int32_t x, int32_t minValue, int32_t maxValue) { return min(max(x, minValue), maxValue); }
inline double clamp(int64_t x, int64_t minValue, int64_t maxValue) { return min(max(x, minValue), maxValue); }
}  // namespace std

#endif

#if USE_SIMDLIB
#include "simd/simd.h"
#else
// emulate float4
#include "float4a.h"
#endif

namespace simd {

#if USE_SIMDLIB

// functional ctor
inline float4 float4m(float3 v, float w)
{
    return vector4(v, w);
}

inline float2 float2m(float x, float y)
{
    return {x, y};
}
inline float3 float3m(float x, float y, float z)
{
    return {x, y, z};
}
inline float4 float4m(float x, float y, float z, float w)
{
    return {x, y, z, w};
}

inline float2 float2m(float x)
{
    return x;
}

inline float3 float3m(float x)
{
    return x;
}

inline float4 float4m(float x)
{
    return x;
}

inline float saturate(float v)
{
    return NAMESPACE_STL::clamp(v, 0.0f, 1.0f);
}
inline double saturate(double v)
{
    return NAMESPACE_STL::clamp(v, 0.0, 1.0);
}
inline float2 saturate(const float2& v)
{
    return simd_clamp(v, 0.0f, 1.0f);
}
inline float3 saturate(const float3& v)
{
    return simd_clamp(v, 0.0f, 1.0f);
}
inline float4 saturate(const float4& v)
{
    return simd_clamp(v, 0.0f, 1.0f);
}

#endif

#if USE_FLOAT16

inline float4 toFloat4(const half4& vv)
{
    // https://patchwork.ozlabs.org/project/gcc/patch/559BC75A.1080606@arm.com/
    // https://gcc.gnu.org/onlinedocs/gcc-7.5.0/gcc/Half-Precision.html
    // https://developer.arm.com/documentation/dui0491/i/Using-NEON-Support/Converting-vectors
    return float4m((float)vv.x, (float)vv.y, (float)vv.z, (float)vv.w);
}
inline half4 toHalf4(const float4& vv)
{
    return half4((_Float16)vv.x, (_Float16)vv.y, (_Float16)vv.z, (_Float16)vv.w);
}

#elif USE_SSE

// using casts instead of vv.reg, so these calls work with Apple SIMD too

inline float4 toFloat4(const half4& vv)
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

    return float4(_mm_cvtph_ps(reg16));
}
inline half4 toHalf4(const float4& vv)
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

#elif USE_NEON

// using casts intead of vv.reg, so these calls work with Apple SIMD too

inline float4 toFloat4(const half4& vv)
{
    return float4(vcvt_f32_f16(*(const float32x4_t*)&vv));
}
inline half4 toHalf4(const float4& vv)
{
    return half(vcvt_f16_f32(*(const float32x4_t*)&vv));
}
#endif

}  // namespace simd

//---------------------------------------

// this just strips args
#define macroUnusedArg(x)

// this just strips args
#define macroUnusedVar(x) (void)x

//---------------------------------------

namespace kram {
using namespace NAMESPACE_STL;

// Use this on vectors
template <typename T>
inline size_t vsizeof(const vector<T>& v)
{
    return sizeof(T) * v.size();
}
}  // namespace kram
