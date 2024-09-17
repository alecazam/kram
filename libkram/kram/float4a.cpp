// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "float4a.h"

// Bury these for now.  They required -mf16c for Intel to be
// defined, and that's kind of a pain right now.
namespace kram {


#if 0 // USE_FLOAT16

// This only works on Apple, and not on Android unless +fp16 arch there.
// And this is likely not faster than the simd op that does this.
simd::float4 toFloat4(const half4& vv)
{
    // https://patchwork.ozlabs.org/project/gcc/patch/559BC75A.1080606@arm.com/
    // https://gcc.gnu.org/onlinedocs/gcc-7.5.0/gcc/Half-Precision.html
    // https://developer.arm.com/documentation/dui0491/i/Using-NEON-Support/Converting-vectors
    return float4m((float)vv.x, (float)vv.y, (float)vv.z, (float)vv.w);
}
half4 toHalf4(const simd::float4& vv)
{
    return half4((_Float16)vv.x, (_Float16)vv.y, (_Float16)vv.z, (_Float16)vv.w);
}


#elif USE_SSE

// using casts instead of vv.reg, so these calls work with Apple SIMD too

simd::float4 toFloat4(const half4& vv)
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

half4 toHalf4(const simd::float4& vv)
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
// Note: could just use the sse2 neon version

simd::float4 toFloat4(const half4& vv)
{
    return simd::float4(vcvt_f32_f16(*(const float16x4_t*)&vv));
}
half4 toHalf4(const simd::float4& vv)
{
    return half4(vcvt_f16_f32(*(const float32x4_t*)&vv));
}
#endif

}
