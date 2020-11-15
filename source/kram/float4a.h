// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

// This is only meant to emulate float4 when lib not available
// (f.e. win or linux) but may move off simd lib to this.  So
// many open source projets skip SIMD, or only do SSE.  This is
// how to support ARM and Neon from one codebase.  This uses
// SSE2Neon.h to translate _mm calls to Neon calls
#if !USE_SIMDLIB

// this is also defined in KramConfig.h, but this keeps file independent
#if USE_NEON
#include "sse2neon.h"
#else
//#include <smmintrin.h> // SSE4.1, and includes all before it
#include <immintrin.h> // AVX
#endif

// see here for intrinsics and which instruction set they come from
// https://docs.microsoft.com/en-us/cpp/intrinsics/x64-amd64-intrinsics-list?view=msvc-160

namespace simd {

#if USE_NEON
#define _mm_fixzero_ps(a, b) _mm_and_ps(a, _mm_cmpneq_ps(b, _mm_setzero_ps()))

// rqrt (high precision)
inline float32x4_t _mm_rsqrthp_ps(const float32x4_t& a)
{
    float32x4_t est = vrsqrteq_f32(a);

    est = _mm_fixzero_ps(est, a);

    // newton raphson
    float32x4_t stepA = vrsqrtsq_f32(a, vmulq_f32(est, est));  // xn+1 = xn(3-dxn*dxn)/2

    return _mm_mul_ps(est, stepA);
}

// sqrt
inline float32x4_t _mm_sqrthp_ps(const float32x4_t& a)
{
    // sqrt(a) = a * rsqrt(a)
    return _mm_mul_ps(_mm_rsqrthp_ps(a), a);
}

// recip
inline float32x4_t _mm_rcphp_ps(const float32x4_t& a)
{
    float32x4_t est = vrecpeq_f32(a);

    est = _mm_fixzero_ps(est, a);

    float32x4_t stepA = vrecpsq_f32(est, a);  // xn+1 = xn(2-dxn)

    return _mm_mul_ps(est, stepA);
}

#else

// this is an easier to read type, since __m128 can hold different data
#define float32x4_t __m128

#define _mm_fixzero_ps(a, b) _mm_and_ps(a, _mm_cmpneq_ps(b, _mm_setzero_ps()))
#define _mm_sqrthp_ss(a) _mm_sqrt_ss(a)
#define _mm_sqrthp_ps(a) _mm_sqrt_ps(a)

inline float32x4_t _mm_rsqrthp_ps(const float32x4_t& a)
{
    static const float32x4_t kHalf = _mm_set1_ps(0.5f);
    static const float32x4_t kThree = _mm_set1_ps(3.0f);

    //for the reciprocal square root, it looks like this: (doesn't handle 0 -> Inf -> Nan), mix in min/max
    // doubles precision
    //x  = rsqrt_approx(c);
    //x *= 0.5*(3 - x*x*c); // refinement

    float32x4_t low;
    low = _mm_rsqrt_ps(a);  // low precision

    // zero out any elements that started out zero (0 -> 0)
    low = _mm_fixzero_ps(low, a);

    // this is already rolled into Neon wrapper
    // TODO: use constants
    low = _mm_mul_ps(low, _mm_mul_ps(kHalf,
                                     _mm_sub_ps(kThree, _mm_mul_ps(a, _mm_mul_ps(low, low)))));

    return low;
}

inline float32x4_t _mm_rcphp_ps(const float32x4_t& a)
{
    static const float32x4_t kTwo = _mm_set1_ps(2.0f);

    // http://www.virtualdub.org/blog/pivot/entry.php?id=229#body (doesn't handle 0 -> Inf, min in min/max
    // 20-bit precision
    //x = reciprocal_approx(c);
    //x' = x * (2 - x * c);

    float32x4_t low = _mm_rcp_ps(a);

    // zero out any elements that started out 0 (0 -> 0)
    low = _mm_fixzero_ps(low, a);

    // this is already rolled into Neon wrapper
    low = _mm_mul_ps(low, _mm_sub_ps(kTwo, _mm_mul_ps(low, a)));

    return low;
}

#define _mm_rsqrthp_ss(a) _mm_setx_ps(a, _mm_rsqrthp_ps(a))
#define _mm_rcphp_ss(a) _mm_setx_ps(a, _mm_rcphp_ps(a))

#endif

//---------------------------------------------------------------------------------------

using tSwizzle = uint32_t;

// swizzle order has to be fliped to use shuffle
#define macroSwizzle(x, y, z, w) _MM_SHUFFLE(w, z, y, x)

// replicate a lane into a new vector
#define _mm_splatx_ps(v) _mm_shuffle_ps(v, v, macroSwizzle(0, 0, 0, 0))
#define _mm_splaty_ps(v) _mm_shuffle_ps(v, v, macroSwizzle(1, 1, 1, 1))
#define _mm_splatz_ps(v) _mm_shuffle_ps(v, v, macroSwizzle(2, 2, 2, 2))
#define _mm_splatw_ps(v) _mm_shuffle_ps(v, v, macroSwizzle(3, 3, 3, 3))

// dot product app with horizontal adds, without using _mm_hadd_ps()
inline float32x4_t _mm_hadd4_ps(const float32x4_t& r)
{
#if 0 // SSE1
//    // use for hpadd
//    static const tSwizzle kSwizzleYYZW = macroSwizzle(1, 1, 2, 3);
//    //static const tSwizzle kSwizzleZYZW = macroSwizzle(2,1,2,3);
//    static const tSwizzle kSwizzleWZZW = macroSwizzle(3, 2, 2, 3);
//
//    float32x4_t t = _mm_add_ps(r, _mm_shuffle_ps(r, r, kSwizzleWZZW));  // xy + wz
//    t = _mm_add_ss(t, _mm_shuffle_ps(t, t, kSwizzleYYZW));              // x + y
//    return t;
#else // SSE3
    float32x4_t t = _mm_hadd_ps(r, r); // xy + wz
    t = _mm_hadd_ps(t, t); // x + y
    return t;
#endif
}

static const uint32_t kSignBitsF32x4i = {0x80000000};
static const float32x4_t kSignBitsF32x4 = _mm_set1_ps(*(const float*)&kSignBitsF32x4i);
static const float32x4_t kOnesF32x4 = _mm_set1_ps(1.0f);

// higher level comparisons, returns 0 or 1
#define _mm_pcmpeq_ps(a, b) _mm_and_ps(_mm_cmpeq_ps(a, b), kOnesF32x4)
#define _mm_pcmpneq_ps(a, b) _mm_and_ps(_mm_cmpneq_ps(a, b), kOnesF32x4)
#define _mm_pcmpgt_ps(a, b) _mm_and_ps(_mm_cmpgt_ps(a, b), kOnesF32x4)
#define _mm_pcmpge_ps(a, b) _mm_and_ps(_mm_cmpge_ps(a, b), kOnesF32x4)
#define _mm_pcmplt_ps(a, b) _mm_pcmpge_ps(b, a)
#define _mm_pcmple_ps(a, b) _mm_pcmpgt_ps(b, a)

//---------------------------------------------------------------------------------------

// Note float3 should be it's own type, but it should be float4 in size.
// float2 is harder since on Neon, it supports a float2 data structure.
// Needs SSE4.1, but that's most of the processors these days.
class float4 {
public:
    using tType = float32x4_t;
    float4() {}
    explicit float4(float val) { reg = _mm_set1_ps(val); }  // xyzw = val
    explicit float4(tType val) { reg = val; }
    float4(float xx, float yy, float zz, float ww) { reg = _mm_setr_ps(xx, yy, zz, ww); }
    float4(const float4& val) { reg = val.reg; }

    union {
        tType reg;

        // avoid using these, since they pull data out of simd registers
        float v[4];
        struct {
            float x, y, z, w;
        };
        struct {
            float r, g, b, a;
        };
    };

    // use of these pull data out of simd registers
    float& operator[](int index)
    {
        return v[index];
    }
    const float& operator[](int index) const
    {
        return v[index];
    }

    // use these to stay in register
    inline float4 xvec() { return float4(_mm_splatx_ps(reg)); }
    inline float4 yvec() { return float4(_mm_splaty_ps(reg)); }
    inline float4 zvec() { return float4(_mm_splatz_ps(reg)); }
    inline float4 wvec() { return float4(_mm_splatw_ps(reg)); }

    inline float4& operator*=(float s)
    {
        return *this *= float4(s);
    }
    inline float4& operator/=(float s)
    {
        return *this /= float4(s);
    }
    inline float4& operator-=(float s)
    {
        return *this -= float4(s);
    }
    inline float4& operator+=(float s)
    {
        return *this += float4(s);
    }

    friend inline float4 operator*(const float4& lhs, const float4& rhs)
    {
        float4 aa(lhs);
        return aa *= rhs;
    }
    friend inline float4 operator/(const float4& lhs, const float4& rhs)
    {
        float4 aa(lhs);
        return aa /= rhs;
    }
    friend inline float4 operator+(const float4& lhs, const float4& rhs)
    {
        float4 aa(lhs);
        return aa += rhs;
    }
    friend inline float4 operator-(const float4& lhs, const float4& rhs)
    {
        float4 aa(lhs);
        return aa -= rhs;
    }

    // scalar ops for right side
    friend float4 operator*(const float4& vv, float s)
    {
        float4 aa(vv);
        return aa *= float4(s);
    }
    friend float4 operator/(const float4& vv, float s)
    {
        float4 aa(vv);
        return aa /= float4(s);
    }
    friend float4 operator+(const float4& vv, float s)
    {
        float4 aa(vv);
        return aa += float4(s);
    }
    friend float4 operator-(const float4& vv, float s)
    {
        float4 aa(vv);
        return aa -= float4(s);
    }

    friend inline float4 operator*(float a, const float4& b)
    {
        float4 aa(a);
        return aa *= b;
    }
    friend inline float4 operator/(float a, const float4& b)
    {
        float4 aa(a);
        return aa /= b;
    }
    friend inline float4 operator+(float a, const float4& b)
    {
        float4 aa(a);
        return aa += b;
    }
    friend inline float4 operator-(float a, const float4& b)
    {
        float4 aa(a);
        return aa -= b;
    }

    // sse ops start here
    inline float4& operator/=(const float4& b)
    {
        reg = _mm_div_ps(reg, b.reg);
        return *this;
    }
    inline float4& operator*=(const float4& b)
    {
        reg = _mm_mul_ps(reg, b.reg);
        return *this;
    }
    inline float4& operator-=(const float4& b)
    {
        reg = _mm_sub_ps(reg, b.reg);
        return *this;
    }
    inline float4& operator+=(const float4& b)
    {
        reg = _mm_add_ps(reg, b.reg);
        return *this;
    }

    friend inline float4 min(const float4& lhs, const float4& rhs)
    {
        return float4(_mm_min_ps(lhs.reg, rhs.reg));
    }
    friend inline float4 max(const float4& lhs, const float4& rhs)
    {
        return float4(_mm_max_ps(lhs.reg, rhs.reg));
    }

    // returns 1's and 0's in a float4
    inline float4 operator==(const float4& vv) const { return float4(_mm_pcmpeq_ps(reg, vv.reg)); }
    inline float4 operator!=(const float4& vv) const { return float4(_mm_pcmpneq_ps(reg, vv.reg)); }
    inline float4 operator>(const float4& vv) const { return float4(_mm_pcmpgt_ps(reg, vv.reg)); }
    inline float4 operator>=(const float4& vv) const { return float4(_mm_pcmpge_ps(reg, vv.reg)); }
    inline float4 operator<(const float4& vv) const { return float4(_mm_pcmplt_ps(reg, vv.reg)); }
    inline float4 operator<=(const float4& vv) const { return float4(_mm_pcmple_ps(reg, vv.reg)); }

    inline bool equal(const float4& vv) const
    {
        int32_t maskBits = _mm_movemask_ps(_mm_cmpeq_ps(reg, vv.reg));
        return maskBits == 15;
    }

    inline bool not_equal(const float4& vv) const { return !equal(vv); }

    // do 4 of these at once
    friend inline float4 recip(const float4& vv)
    {
        return float4(_mm_rcphp_ps(vv.reg));
    }
    friend inline float4 rsqrt(const float4& vv)
    {
        return float4(_mm_rsqrthp_ps(vv.reg));
    }
    friend inline float4 sqrt(const float4& vv)
    {
        return float4(_mm_sqrthp_ps(vv.reg));
    }

    friend inline float length_squared(const float4& vv)
    {
        return float4(_mm_hadd4_ps(_mm_mul_ps(vv.reg, vv.reg)))[0];
    }
    friend inline float length(const float4& vv)
    {
        return float4(_mm_sqrt_ss(_mm_hadd4_ps(_mm_mul_ps(vv.reg, vv.reg))))[0];
    }

    // sse4.1 ops
    friend inline float4 round(const float4& vv)
    {
        return float4(_mm_round_ps(vv.reg, 0x8)); // round to nearest | exc
    }
    friend inline float4 ceil(const float4& vv)
    {
        return float4(_mm_ceil_ps(vv.reg));
    }
    friend inline float4 floor(const float4& vv)
    {
        return float4(_mm_floor_ps(vv.reg)); // SSE4.1
    }

    // see if any results are 1
    friend inline bool any(const float4& vv)
    {
        return float4(_mm_hadd4_ps(vv.reg))[0] > 0.0f;
    }

    inline float4 operator-() const
    {
        return float4(_mm_xor_ps(kSignBitsF32x4, reg));  // -a
    }

    friend inline float4 select(const float4& lhs, const float4& rhs, const float4& mask)
    {
        return float4(_mm_or_ps(_mm_andnot_ps(mask.reg, lhs.reg), _mm_and_ps(mask.reg, rhs.reg)));  // 0 picks a, 1 picks b
    }
    
#if !USE_FLOAT16
    // Win does not have _Float16 compiler support as of VS2019.  So have to resort to AVX.
    inline float4 fromFloat16(const uint16_t* fp16, int count = 4) {
        // note this zero's out any of the exiting values
        __m128i reg16 = _mm_setzero_si128();
        
        // fill low bits of m128i with the 4x fp16 values
        // fallthrough.  Can't do a loop since imm is harcoded into instruction
        // TODO: might be faster to load bottom 64-bytes _mm_loadu_epi64 for case 3 and break?
        switch(count - 1) {
            case 3: reg16 = _mm_insert_epi16(reg16, fp16[3], 3);
            case 2: reg16 = _mm_insert_epi16(reg16, fp16[2], 2);
            case 1: reg16 = _mm_insert_epi16(reg16, fp16[1], 1);
            case 0: reg16 = _mm_insert_epi16(reg16, fp16[0], 0); break;
            default: assert(false); break;
        }

        return float4(_mm_cvtph_ps(reg16)); // 4xfp16 -> 4xfp32, round to nearest-even
    }
    
    inline void toFloat16(uint16_t* fp16, int count = 4) const {
        __m128i reg16 = _mm_cvtps_ph(reg, 0); // 4xfp32-> 4xfp16,  round to nearest-even
       
        // extract low 16-bits out of the 4 values
        // fallthrough.  Can't do a loop since imm is harcoded into instruction
        switch(count - 1) {
            case 3: fp16[3] = _mm_extract_epi16(reg16, 3);
            case 2: fp16[2] = _mm_extract_epi16(reg16, 2);
            case 1: fp16[1] = _mm_extract_epi16(reg16, 1);
            case 0: fp16[0] = _mm_extract_epi16(reg16, 0); break;
            default: assert(false); break;
        }
    }
#endif
};

};  // namespace simd

#endif
