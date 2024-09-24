// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#if !USE_SIMDLIB

// This is Apple simd (it's huuuggge!)
// Also can't use the half4 type until iOS18 + macOS15 minspec, so need fallback.
#include <simd/simd.h>

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

#define SIMD_NAMESPACE simd

#if !__is_identifier(_Float16)
#define SIMD_HALF_FLOAT16 1
#else
#define SIMD_HALF_FLOAT16 0
#endif

namespace SIMD_NAMESPACE {

// functional ctor
inline float4 float4m(float3 v, float w) {
    return vector4(v, w);
}

inline float2 float2m(float x, float y) {
    return {x, y};
}
inline float3 float3m(float x, float y, float z) {
    return {x, y, z};
}
inline float4 float4m(float x, float y, float z, float w) {
    return {x, y, z, w};
}

inline float2 float2m(float x) {
    return x;
}

inline float3 float3m(float x) {
    return x;
}

inline float4 float4m(float x) {
    return x;
}

//inline float saturate(float v) {
//    return std::clamp(v, 0.0f, 1.0f);
//}
//inline double saturate(double v) {
//    return std::clamp(v, 0.0, 1.0);
//}
inline float2 saturate(float2 v) {
    return simd_clamp(v, 0.0f, 1.0f);
}
inline float3 saturate(float3 v) {
    return simd_clamp(v, 0.0f, 1.0f);
}
inline float4 saturate(float4 v) {
    return simd_clamp(v, 0.0f, 1.0f);
}

#if SIMD_HALF_FLOAT16
using half = _Float16;
#else
// for lack of a better type
using half = int16_t;
#endif

#define vec2to4(x) (x).xyyy
#define vec3to4(x) (x).xyzz
#define vec4to2(x) (x).xy
#define vec4to3(x) (x).xyz

// define half ops just for conversion
half4 half4m(float4 __x);
inline half2 half2m(float2 __x) { return vec4to2(half4m(vec2to4(__x))); }
inline half3 half3m(float3 __x) { return vec4to3(half4m(vec3to4(__x))); }

float4 float4m(half4 __x);
inline float2 float2m(half2 __x) { return vec4to2(float4m(vec2to4(__x))); }
inline float3 float3m(half3 __x) { return vec4to3(float4m(vec3to4(__x))); }

} // namespace SIMD_NAMESPACE

#endif
