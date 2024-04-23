// SPDX-License-Identifier: Apache-2.0
// ----------------------------------------------------------------------------
// Copyright 2011-2021 Arm Limited
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at:
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.
// ----------------------------------------------------------------------------

#include "astcenc_mathlib.h"

/**
 * @brief 64-bit rotate left.
 *
 * @param val   The value to rotate.
 * @param count The rotation, in bits.
 */
static inline uint64_t rotl(uint64_t val, int count)
{
	return (val << count) | (val >> (64 - count));
}

/* See header for documentation. */
void astc::rand_init(uint64_t state[2])
{
	state[0] = 0xfaf9e171cea1ec6bULL;
	state[1] = 0xf1b318cc06af5d71ULL;
}

/* See header for documentation. */
uint64_t astc::rand(uint64_t state[2])
{
	uint64_t s0 = state[0];
	uint64_t s1 = state[1];
	uint64_t res = s0 + s1;
	s1 ^= s0;
	state[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16);
	state[1] = rotl(s1, 37);
	return res;
}

#if USE_SSE

/* ============================================================================
  Softfloat library with fp32 and fp16 conversion functionality.
============================================================================ */
//#if (ASTCENC_F16C == 0) && (ASTCENC_NEON == 0)
//    /* narrowing float->float conversions */
//    uint16_t float_to_sf16(float val);
//    float sf16_to_float(uint16_t val);
//#endif

vint4 float_to_float16(vfloat4 a)
{
//#if ASTCENC_F16C >= 1
    __m128i packedf16 = _mm_cvtps_ph(a.m, 0);
    __m128i f16 = _mm_cvtepu16_epi32(packedf16);
    return vint4(f16);
//#else
//    return vint4(
//        float_to_sf16(a.lane<0>()),
//        float_to_sf16(a.lane<1>()),
//        float_to_sf16(a.lane<2>()),
//        float_to_sf16(a.lane<3>()));
//#endif
}

/**
 * @brief Return a float16 value for a float scalar, using round-to-nearest.
 */
uint16_t float_to_float16(float a)
{
//#if ASTCENC_F16C >= 1
    __m128i f16 = _mm_cvtps_ph(_mm_set1_ps(a), 0);
    return  static_cast<uint16_t>(_mm_cvtsi128_si32(f16));
//#else
//    return float_to_sf16(a);
//#endif
}

/**
 * @brief Return a float value for a float16 vector.
 */
vfloat4 float16_to_float(vint4 a)
{
//#if ASTCENC_F16C >= 1
    __m128i packed = _mm_packs_epi32(a.m, a.m);
    __m128 f32 = _mm_cvtph_ps(packed);
    return vfloat4(f32);
//#else
//    return vfloat4(
//        sf16_to_float(a.lane<0>()),
//        sf16_to_float(a.lane<1>()),
//        sf16_to_float(a.lane<2>()),
//        sf16_to_float(a.lane<3>()));
//#endif
}

/**
 * @brief Return a float value for a float16 scalar.
 */
float float16_to_float(uint16_t a)
{
//#if ASTCENC_F16C >= 1
    __m128i packed = _mm_set1_epi16(a);
    __m128 f32 = _mm_cvtph_ps(packed);
    return _mm_cvtss_f32(f32);
//#else
//    return sf16_to_float(a);
//#endif
}

#endif
