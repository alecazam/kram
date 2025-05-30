/* SIMD (SSE1+MMX or SSE2) implementation of sin, cos, exp and log

   Inspired by Intel Approximate Math library, and based on the
   corresponding algorithms of the cephes math library

   The default is to use the SSE1 version. If you define USE_SSE2 the
   the SSE2 intrinsics will be used in place of the MMX intrinsics. Do
   not expect any significant performance improvement with SSE2.
*/

/* Copyright (C) 2007  Julien Pommier

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  (this is the zlib license)
*/

// Mods to this;
// kram - Copyright 2020-2025 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

// TODO: may want to rename to sse_mathfun.cpp
// TODO: use math ops and simd ops here and let compiler gen intrinsics?
// TODO: combine the constants into fewer registers, reference .x,..

#pragma once
// clang-format off

#include <math.h>

namespace SIMD_NAMESPACE {

//---------------------------
// Start of mathfun below

#if SIMD_FLOAT

#define _PS_CONST(Name, Val) \
  static const float4 _ps_##Name = Val
#define _PI_CONST(Name, Val) \
  static const int4 _pi_##Name = Val

/* the smallest non denormalized float number */
_PS_CONST(min_norm_pos, (float)0x00800000);
_PI_CONST(mant_mask, 0x7f800000);
_PI_CONST(inv_mant_mask, ~0x7f800000);

_PI_CONST(sign_mask, (int32_t)0x80000000);
_PI_CONST(inv_sign_mask, ~0x80000000);

_PI_CONST(1, 1);
_PI_CONST(inv1, ~1);
_PI_CONST(2, 2);
_PI_CONST(4, 4);
_PI_CONST(0x7f, 0x7f);

_PS_CONST(1  , 1.0f);
_PS_CONST(0p5, 0.5f);

_PS_CONST(cephes_SQRTHF, 0.707106781186547524);
_PS_CONST(cephes_log_p0, 7.0376836292E-2);
_PS_CONST(cephes_log_p1, - 1.1514610310E-1);
_PS_CONST(cephes_log_p2, 1.1676998740E-1);
_PS_CONST(cephes_log_p3, - 1.2420140846E-1);
_PS_CONST(cephes_log_p4, + 1.4249322787E-1);
_PS_CONST(cephes_log_p5, - 1.6668057665E-1);
_PS_CONST(cephes_log_p6, + 2.0000714765E-1);
_PS_CONST(cephes_log_p7, - 2.4999993993E-1);
_PS_CONST(cephes_log_p8, + 3.3333331174E-1);
_PS_CONST(cephes_log_q1, -2.12194440e-4);
_PS_CONST(cephes_log_q2, 0.693359375);

// not exposing yet due to calling convention and no math equiv
static void sincos(float4 x, float4& s, float4& c);

// This is just extra function overhead.  May just want to rename
float4 sin(float4 x) {
    float4 s, c;
    sincos(x, s, c);
    return s;
}
float4 cos(float4 x) {
    float4 s, c;
    sincos(x, s, c);
    return c;
}

/* natural logarithm computed for 4 simultaneous float
   return NaN for x <= 0
*/
float4 log(float4 x) {
    int4 emm0;
    float4 one = _ps_1;

    float4 invalid_mask = _mm_cmple_ps(x, _mm_setzero_ps());

    x = _mm_max_ps(x, _ps_min_norm_pos);  /* cut off denormalized stuff */

    emm0 = _mm_srli_epi32(_mm_castps_si128(x), 23);
    /* keep only the fractional part */
    x = _mm_and_ps(x, _pi_inv_mant_mask);
    x = _mm_or_ps(x, _ps_0p5);

    emm0 = _mm_sub_epi32(emm0, _pi_0x7f);
    float4 e = _mm_cvtepi32_ps(emm0);

    e = _mm_add_ps(e, one);

    /* part2:
     if( x < SQRTHF ) {
       e -= 1;
       x = x + x - 1.0;
     } else { x = x - 1.0; }
    */
    float4 mask = _mm_cmplt_ps(x, _ps_cephes_SQRTHF);
    float4 tmp = _mm_and_ps(x, mask);
    x = _mm_sub_ps(x, one);
    e = _mm_sub_ps(e, _mm_and_ps(one, mask));
    x = _mm_add_ps(x, tmp);


    float4 z = _mm_mul_ps(x,x);

    float4 y = _ps_cephes_log_p0;
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_log_p1);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_log_p2);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_log_p3);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_log_p4);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_log_p5);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_log_p6);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_log_p7);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_log_p8);
    y = _mm_mul_ps(y, x);

    y = _mm_mul_ps(y, z);


    tmp = _mm_mul_ps(e, _ps_cephes_log_q1);
    y = _mm_add_ps(y, tmp);


    tmp = _mm_mul_ps(z, _ps_0p5);
    y = _mm_sub_ps(y, tmp);

    tmp = _mm_mul_ps(e, _ps_cephes_log_q2);
    x = _mm_add_ps(x, y);
    x = _mm_add_ps(x, tmp);
    x = _mm_or_ps(x, invalid_mask); // negative arg will be NAN
    return x;
}

_PS_CONST(exp_hi,	88.3762626647949f);
_PS_CONST(exp_lo,	-88.3762626647949f);

_PS_CONST(cephes_LOG2EF, 1.44269504088896341);
_PS_CONST(cephes_exp_C1, 0.693359375);
_PS_CONST(cephes_exp_C2, -2.12194440e-4);

_PS_CONST(cephes_exp_p0, 1.9875691500E-4);
_PS_CONST(cephes_exp_p1, 1.3981999507E-3);
_PS_CONST(cephes_exp_p2, 8.3334519073E-3);
_PS_CONST(cephes_exp_p3, 4.1665795894E-2);
_PS_CONST(cephes_exp_p4, 1.6666665459E-1);
_PS_CONST(cephes_exp_p5, 5.0000001201E-1);

float4 exp(float4 x) {
    float4 tmp = _mm_setzero_ps(), fx;
    int4 emm0;
    float4 one = _ps_1;

#if 0
    x = clamp(x, _ps_exp_lo, _ps_exp_hi);
    fx = x * _ps_cephes_LOG2EF + _ps_0p5;
    
    fx = floor(fx);
   
    x -= fx * (_ps_cephes_exp_C1 + _ps_cephes_exp_C2);
    float4 z = x * x; // squared
    
    // polynomial
    float4 y = ((((((
        _ps_cephes_exp_p0 * x + _ps_cephes_exp_p1) * x) +
        _ps_cephes_exp_p2 * x) + _ps_cephes_exp_p3 * x) +
        _ps_cephes_exp_p4 * x) + _ps_cephes_exp_p5 * z) + x + one;
    
    // build 2^n
    emm0 = int4(fx); // truncate to int
    emm0 = (emm0 + _pi_0x7f) << 23;
    float4 pow2n = _mm_castsi128_ps(emm0); // treat int as float
    y *= pow2n;
    
#else
    x = _mm_min_ps(x, _ps_exp_hi);
    x = _mm_max_ps(x, _ps_exp_lo);
    
    /* express exp(x) as exp(g + n*log(2)) */
    fx = _mm_mul_ps(x, _ps_cephes_LOG2EF);
    fx = _mm_add_ps(fx, _ps_0p5);
    
    /* how to perform a floorf with SSE: just below */
    emm0 = _mm_cvttps_epi32(fx);
    tmp  = _mm_cvtepi32_ps(emm0);
    /* if greater, substract 1 */
    float4 mask = _mm_cmpgt_ps(tmp, fx);
    mask = _mm_and_ps(mask, one);
    fx = _mm_sub_ps(tmp, mask);
    
    tmp = _mm_mul_ps(fx, _ps_cephes_exp_C1);
    float4 z = _mm_mul_ps(fx, _ps_cephes_exp_C2);
    x = _mm_sub_ps(x, tmp);
    x = _mm_sub_ps(x, z);

    z = _mm_mul_ps(x,x);
    
    // mads to form a polynoial
    float4 y = _ps_cephes_exp_p0;
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_exp_p1);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_exp_p2);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_exp_p3);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_exp_p4);
    y = _mm_mul_ps(y, x);
    y = _mm_add_ps(y, _ps_cephes_exp_p5);
    y = _mm_mul_ps(y, z);
    y = _mm_add_ps(y, x);
    y = _mm_add_ps(y, one);

    /* build 2^n */
    emm0 = _mm_cvttps_epi32(fx);
    emm0 = _mm_add_epi32(emm0, _pi_0x7f);
    emm0 = _mm_slli_epi32(emm0, 23);
    float4 pow2n = _mm_castsi128_ps(emm0);
    y = _mm_mul_ps(y, pow2n);
    
#endif
    
    return y;
}

_PS_CONST(minus_cephes_DP1, -0.78515625);
_PS_CONST(minus_cephes_DP2, -2.4187564849853515625e-4);
_PS_CONST(minus_cephes_DP3, -3.77489497744594108e-8);
_PS_CONST(sincof_p0, -1.9515295891E-4);
_PS_CONST(sincof_p1,  8.3321608736E-3);
_PS_CONST(sincof_p2, -1.6666654611E-1);
_PS_CONST(coscof_p0,  2.443315711809948E-005);
_PS_CONST(coscof_p1, -1.388731625493765E-003);
_PS_CONST(coscof_p2,  4.166664568298827E-002);
_PS_CONST(cephes_FOPI, 1.27323954473516); // 4 / M_PI

/* evaluation of 4 sines at onces, using only SSE1+MMX intrinsics so
   it runs also on old athlons XPs and the pentium III of your grand
   mother.

   The code is the exact rewriting of the cephes sinf function.
   Precision is excellent as long as x < 8192 (I did not bother to
   take into account the special handling they have for greater values
   -- it does not return garbage for arguments over 8192, though, but
   the extra precision is missing).

   Note that it is such that sinf((float)M_PI) = 8.74e-8, which is the
   surprising but correct result.

   Performance is also surprisingly good, 1.33 times faster than the
   macos vsinf SSE2 function, and 1.5 times faster than the
   __vrs4_sinf of amd's ACML (which is only available in 64 bits). Not
   too bad for an SSE1 function (with no special tuning) !
   However the latter libraries probably have a much better handling of NaN,
   Inf, denormalized and other special arguments..

   On my core 1 duo, the execution of this function takes approximately 95 cycles.

   From what I have observed on the experiments with Intel AMath lib, switching to an
   SSE2 version would improve the perf by only 10%.

   Since it is based on SSE intrinsics, it has to be compiled at -O2 to
   deliver full speed.
*/


/* since sin_ps and cos_ps are almost identical, sincos_ps could replace both of them..
   it is almost as fast, and gives you a free cosine with your sine */
static void sincos(float4 x, float4& s, float4& c) {
    float4 xmm1, xmm2, xmm3 = _mm_setzero_ps(), sign_bit_sin, y;
    int4 emm0, emm2, emm4;
    sign_bit_sin = x;
    /* take the absolute value */
    x = _mm_and_ps(x, _pi_inv_sign_mask);
    /* extract the sign bit (upper one) */
    sign_bit_sin = _mm_and_ps(sign_bit_sin, _pi_sign_mask);

    /* scale by 4/Pi */
    y = _mm_mul_ps(x, _ps_cephes_FOPI);

    /* store the integer part of y in emm2 */
    emm2 = _mm_cvttps_epi32(y);

    /* j=(j+1) & (~1) (see the cephes sources) */
    emm2 = _mm_add_epi32(emm2, _pi_1);
    emm2 = _mm_and_si128(emm2, _pi_inv1);
    y = _mm_cvtepi32_ps(emm2);

    emm4 = emm2;

    /* get the swap sign flag for the sine */
    emm0 = _mm_and_si128(emm2, _pi_4);
    emm0 = _mm_slli_epi32(emm0, 29);
    float4 swap_sign_bit_sin = _mm_castsi128_ps(emm0);

    /* get the polynom selection mask for the sine*/
    emm2 = _mm_and_si128(emm2, _pi_2);
    emm2 = _mm_cmpeq_epi32(emm2, _mm_setzero_si128());
    float4 poly_mask = _mm_castsi128_ps(emm2);

    /* The magic pass: "Extended precision modular arithmetic"
     x = ((x - y * DP1) - y * DP2) - y * DP3; */
    xmm1 = _ps_minus_cephes_DP1;
    xmm2 = _ps_minus_cephes_DP2;
    xmm3 = _ps_minus_cephes_DP3;
    xmm1 = _mm_mul_ps(y, xmm1);
    xmm2 = _mm_mul_ps(y, xmm2);
    xmm3 = _mm_mul_ps(y, xmm3);
    x = _mm_add_ps(x, xmm1);
    x = _mm_add_ps(x, xmm2);
    x = _mm_add_ps(x, xmm3);

    emm4 = _mm_sub_epi32(emm4, _pi_2);
    emm4 = _mm_andnot_si128(emm4, _pi_4);
    emm4 = _mm_slli_epi32(emm4, 29);
    float4 sign_bit_cos = _mm_castsi128_ps(emm4);

    sign_bit_sin = _mm_xor_ps(sign_bit_sin, swap_sign_bit_sin);


    /* Evaluate the first polynom  (0 <= x <= Pi/4) */
    float4 z = _mm_mul_ps(x,x);
    y = _ps_coscof_p0;

    y = _mm_mul_ps(y, z);
    y = _mm_add_ps(y, _ps_coscof_p1);
    y = _mm_mul_ps(y, z);
    y = _mm_add_ps(y, _ps_coscof_p2);
    y = _mm_mul_ps(y, z);
    y = _mm_mul_ps(y, z);
    float4 tmp = _mm_mul_ps(z, _ps_0p5);
    y = _mm_sub_ps(y, tmp);
    y = _mm_add_ps(y, _ps_1);

    /* Evaluate the second polynom  (Pi/4 <= x <= 0) */

    float4 y2 = _ps_sincof_p0;
    y2 = _mm_mul_ps(y2, z);
    y2 = _mm_add_ps(y2, _ps_sincof_p1);
    y2 = _mm_mul_ps(y2, z);
    y2 = _mm_add_ps(y2, _ps_sincof_p2);
    y2 = _mm_mul_ps(y2, z);
    y2 = _mm_mul_ps(y2, x);
    y2 = _mm_add_ps(y2, x);

    /* select the correct result from the two polynoms */
    xmm3 = poly_mask;
    float4 ysin2 = _mm_and_ps(xmm3, y2);
    float4 ysin1 = _mm_andnot_ps(xmm3, y);
    y2 = _mm_sub_ps(y2,ysin2);
    y = _mm_sub_ps(y, ysin1);

    xmm1 = _mm_add_ps(ysin1,ysin2);
    xmm2 = _mm_add_ps(y,y2);

    /* update the sign */
    s = _mm_xor_ps(xmm1, sign_bit_sin);
    c = _mm_xor_ps(xmm2, sign_bit_cos);
}

// This has to forward 2/3 to the 4 element version.  
#define macroVectorRepeatFnImpl(type, cppfun, func) \
type##2 cppfunc(type##2) { return vec4to2(func(zeroext(x))); } \
type##3 cppfunc(type##3) { return vec4to3(func(zeroext(x))); } \

macroVectorRepeatFnImpl(float, log, log)
macroVectorRepeatFnImpl(float, exp, exp)

macroVectorRepeatFnImpl(float, sin, sin)
macroVectorRepeatFnImpl(float, cos, cos)
macroVectorRepeatFnImpl(float, cos, tan)

// TODO: pow takes in 2 args

#endif // SIMD_FLOAT

} // namespace SIMD_NAMESPACE
