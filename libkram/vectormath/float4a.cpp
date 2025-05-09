// kram - Copyright 2020-2025 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#if !USE_SIMDLIB

// https://patchwork.ozlabs.org/project/gcc/patch/559BC75A.1080606@arm.com/
// https://gcc.gnu.org/onlinedocs/gcc-7.5.0/gcc/Half-Precision.html
// https://developer.arm.com/documentation/dui0491/i/Using-NEON-Support/Converting-vectors

#include "float4a.h"

// Bury these for now.  They required -mf16c for Intel to be
// defined, and that's kind of a pain right now.
namespace SIMD_NAMESPACE {

#if SIMD_SSE

// using casts instead of vv.reg, so these calls work with Apple SIMD too

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
    __m128i reg16 = _mm_cvtps_ph(*(const __m128*)&vv, 0); // 4xfp32-> 4xfp16,  round to nearest-even

    // TODO: switch to store/steam, but don't know which one _mm_storeu_epi16 ?
    half4 val; // = 0;

    // 0 extended
    val[0] = (half)_mm_extract_epi16(reg16, 0);
    val[1] = (half)_mm_extract_epi16(reg16, 1);
    val[2] = (half)_mm_extract_epi16(reg16, 2);
    val[3] = (half)_mm_extract_epi16(reg16, 3);
    return val;
}

#endif

#if SIMD_NEON

// using casts intead of vv.reg, so these calls work with Apple SIMD too
// Note: could just use the sse2 neon version

float4 float4m(half4 vv)
{
    return float4(vcvt_f32_f16(*(const float16x4_t*)&vv));
}
half4 half4m(float4 vv)
{
    return half4(vcvt_f16_f32(*(const float32x4_t*)&vv));
}
#endif

} //namespace SIMD_NAMESPACE

#endif
