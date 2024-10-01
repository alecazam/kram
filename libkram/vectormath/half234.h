// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

// This is not yet standalone.  vectormath234.h includes it.
#if USE_SIMDLIB && SIMD_HALF

// Android doesn't really have _Float16, so would need a u/int16_t mapped placeholder
// The not identifier means its a system type.
#if !__is_identifier(_Float16)
#define SIMD_HALF_FLOAT16 1
#else
#define SIMD_HALF_FLOAT16 0
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

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
macroVector2TypesAligned(half, half)
macroVector2TypesPacked(half, half)

// No matrix type defined right now.

// glue to Accelerate
#if SIMD_ACCELERATE_MATH_NAMES
macroVector8TypesStorageRenames(half, simd_half)
#endif // SIMD_ACCELERATE_MATH_NAMES

#ifdef __cplusplus
}

namespace SIMD_NAMESPACE {

macroVector2TypesStorageRenames(half, half)

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
SIMD_CALL half3 half3m(half2 v, half z) {
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


}
#endif // __cplusplus
#endif // USE_SIMDLIB && SIMD_HALF

