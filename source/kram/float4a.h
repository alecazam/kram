// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

// This is only meant to emulate float4 when lib not available
// (f.e. win or linux) but may move off simd lib to this.  So
// many open source projets skip SIMD, or only do SSE.  This is
// how to support ARM and Neon from one codebase.  This uses
// SSE2Neon.h to translate _mm calls to Neon calls
// TODO: update SSE2Neon to use arm64 permute/shuffle instructions.
#if !USE_SIMDLIB

#if USE_NEON
#include "SSE2Neon.h"
#else
#include <emmintrin.h>
#include <xmmintrin.h>

#endif

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

// sse1 version of sse3 op for horizontal add
inline float32x4_t _mm_hadd4_ps(const float32x4_t& r)
{
    // use for hpadd
    static const tSwizzle kSwizzleYYZW = macroSwizzle(1, 1, 2, 3);
    //static const tSwizzle kSwizzleZYZW = macroSwizzle(2,1,2,3);
    static const tSwizzle kSwizzleWZZW = macroSwizzle(3, 2, 2, 3);

    float32x4_t t = _mm_add_ps(r, _mm_shuffle_ps(r, r, kSwizzleWZZW));  // xy + wz
    t = _mm_add_ss(t, _mm_shuffle_ps(t, t, kSwizzleYYZW));              // x + y
    return t;
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
class float4 {
public:
    using tType = float32x4_t;
    float4() {}
    explicit float4(float val) { reg = _mm_set1_ps(val); }
    explicit float4(tType val) { reg = val; }
    float4(float xx, float yy, float zz, float ww) { reg = _mm_setr_ps(xx, yy, zz, ww); }
    float4(const float4& val) { reg = val.reg; }

    union {
        tType reg;

        // code tries to avoid using these, since they break using simd registers
        float v[4];
        float x, y, z, w;
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

    // use these to stay in register (not sure if
    inline float4 xvec() { return float4(_mm_splatx_ps(reg)); }
    inline float4 yvec() { return float4(_mm_splaty_ps(reg)); }
    inline float4 zvec() { return float4(_mm_splatz_ps(reg)); }
    inline float4 wvec() { return float4(_mm_splatw_ps(reg)); }

    inline float4& operator*=(float b)
    {
        return *this *= float4(b);
    }
    inline float4& operator/=(float b)
    {
        return *this /= float4(b);
    }
    inline float4& operator-=(float b)
    {
        return *this -= float4(b);
    }
    inline float4& operator+=(float b)
    {
        return *this += float4(b);
    }

    friend inline float4 operator*(const float4& a, const float4& b)
    {
        return float4(a) *= b;
    }
    friend inline float4 operator/(const float4& a, const float4& b)
    {
        return float4(a) /= b;
    }
    friend inline float4 operator+(const float4& a, const float4& b)
    {
        return float4(a) += b;
    }
    friend inline float4 operator-(const float4& a, const float4& b)
    {
        return float4(a) -= b;
    }

    // scalar ops for right side
    friend float4 operator*(const float4& a, float b)
    {
        return float4(a) *= float4(b);
    }
    friend float4 operator/(const float4& a, float b)
    {
        return float4(a) /= float4(b);
    }
    friend float4 operator+(const float4& a, float b)
    {
        return float4(a) += float4(b);
    }
    friend float4 operator-(const float4& a, float b)
    {
        return float4(a) -= float4(b);
    }

    friend inline float4 operator*(float a, const float4& b)
    {
        return float4(a) *= b;
    }
    friend inline float4 operator/(float a, const float4& b)
    {
        return float4(a) /= b;
    }
    friend inline float4 operator+(float a, const float4& b)
    {
        return float4(a) += b;
    }
    friend inline float4 operator-(float a, const float4& b)
    {
        return float4(a) -= b;
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

    friend inline float4 min(const float4& a, const float4& b)
    {
        return float4(_mm_min_ps(a.reg, b.reg));
    }
    friend inline float4 max(const float4& a, const float4& b)
    {
        return float4(_mm_max_ps(a.reg, b.reg));
    }

    // returns 1's and 0's in a float4
    inline float4 operator==(const float4& b) const { return float4(_mm_pcmpeq_ps(reg, b.reg)); }
    inline float4 operator!=(const float4& b) const { return float4(_mm_pcmpneq_ps(reg, b.reg)); }
    inline float4 operator>(const float4& b) const { return float4(_mm_pcmpgt_ps(reg, b.reg)); }
    inline float4 operator>=(const float4& b) const { return float4(_mm_pcmpge_ps(reg, b.reg)); }
    inline float4 operator<(const float4& b) const { return float4(_mm_pcmplt_ps(reg, b.reg)); }
    inline float4 operator<=(const float4& b) const { return float4(_mm_pcmple_ps(reg, b.reg)); }

    inline bool equal(const float4& b) const
    {
        int32_t maskBits = _mm_movemask_ps(_mm_cmpeq_ps(reg, b.reg));
        return maskBits == 15;
    }

    inline bool not_equal(const float4& b) const { return !equal(b); }

    // do 4 of these at once
    friend inline float4 recip(const float4& a)
    {
        return float4(_mm_rcphp_ps(a.reg));
    }
    friend inline float4 rsqrt(const float4& a)
    {
        return float4(_mm_rsqrthp_ps(a.reg));
    }
    friend inline float4 sqrt(const float4& a)
    {
        return float4(_mm_sqrthp_ps(a.reg));
    }

    friend inline float dot(const float4& a, const float4& b)
    {
        return float4(_mm_hadd4_ps(_mm_mul_ps(a.reg, b.reg)))[0];
    }
    friend inline float lengthSquared(const float4& a)
    {
        return sqrtf(dot(a, a));
    }
    friend inline float length(const float4& a)
    {
        return sqrtf(lengthSquared(a));
    }

    // TODO: need simd versions
    friend inline float4 round(const float4& a)
    {
        return float4(roundf(a.x), roundf(a.y), roundf(a.z), roundf(a.w));
    }
    friend inline float4 ceil(const float4& a)
    {
        return float4(ceilf(a.x), ceilf(a.y), ceilf(a.z), ceilf(a.w));
    }
    friend inline float4 floor(const float4& a)
    {
        return float4(floorf(a.x), floorf(a.y), floorf(a.z), floorf(a.w));
    }

    // see if any results are 1
    friend inline bool any(const float4& a)
    {
        return float4(_mm_hadd4_ps(a.reg))[0] > 0.0f;
    }

    inline float4 operator-() const
    {
        return float4(_mm_xor_ps(kSignBitsF32x4, reg));  // -a
    }

    friend inline float4 select(const float4& a, const float4& b, const float4& mask)
    {
        return float4(_mm_or_ps(_mm_andnot_ps(mask.reg, a.reg), _mm_and_ps(mask.reg, b.reg)));  // 0 picks a, 1 picks b
    }
};

};  // namespace simd

#endif
