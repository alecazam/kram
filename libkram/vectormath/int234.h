// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

// This is not yet standalone.  vectormath234.h includes it.
#if USE_SIMDLIB && SIMD_INT

// clang-format off

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// define c vector types
macroVector4TypesAligned(int, int)
macroVector4TypesPacked(int, int)

#if SIMD_ACCELERATE_MATH_NAMES
macroVector4TypesStorageRenames(int, simd_int)
#endif // SIMD_ACCELERATE_MATH_NAMES

#ifdef __cplusplus
}

namespace SIMD_NAMESPACE {

macroVector4TypesStorageRenames(int, int)

    // clang-format on

    SIMD_CALL int4 zeroext(int2 x)
{
    int4 v = 0;
    v.xy = x;
    return v;
}
SIMD_CALL int4 zeroext(int3 x)
{
    int4 v = 0;
    v.xyz = x;
    return v;
}

//-----------------------------------
// imlementation - only code simd arch specific

#if SIMD_NEON

SIMD_CALL bool any(int2 x)
{
    return vmaxv_u32(x) & 0x80000000;
}
SIMD_CALL bool any(int4 x)
{
    return vmaxvq_u32(x) & 0x80000000;
}

SIMD_CALL bool all(int2 x)
{
    return vminv_u32(x) & 0x80000000;
}
SIMD_CALL bool all(int4 x)
{
    return vminvq_u32(x) & 0x80000000;
}

SIMD_CALL int reduce_add(int2 x)
{
    x = vpadd_s32(x, x);
    return x.x; // repeat x to all values
}
SIMD_CALL int reduce_add(int4 x)
{
    // 4:1 reduction
    x = vpaddq_s32(x, x); // xy = x+y,z+w
    x = vpaddq_s32(x, x); // x  = x+y
    return x.x; // repeat x to all values
}
SIMD_CALL int reduce_add(int3 x)
{
    return reduce_add(zeroext(x));
}

#endif // SIMD_NEON

// These take in int types, this is what comparison gens from a < b, etc.
#if SIMD_SSE

SIMD_CALL bool any(int2 x)
{
    return _mm_movemask_ps(vec2to4(x)) & 0x3;
}
SIMD_CALL bool any(int4 x)
{
    return _mm_movemask_ps((__m128)x);
}

SIMD_CALL bool all(int2 x)
{
    return (_mm_movemask_ps(vec2to4(x)) & 0x3) == 0x3; // 2 bits
}
SIMD_CALL bool all(int4 x)
{
    return _mm_movemask_ps((__m128)x) == 0xf; // 4 bits
}

// TODO: need SSE ops for this,
SIMD_CALL int reduce_add(int4 x)
{
    int2 r = x.lo + x.hi;
    return r.x + r.y;
}
SIMD_CALL int reduce_add(int2 x)
{
    return x.x + x.y;
}
SIMD_CALL int reduce_add(int3 x)
{
    return x.x + x.y + x.z;
}

#endif // SIMD_SSE

// any-all
SIMD_CALL bool any(int3 x)
{
    return any(vec3to4(x));
}
SIMD_CALL bool all(int3 x)
{
    return all(vec3to4(x));
}

// end of implementation
//-----------------------------------

// bitselect
SIMD_CALL int2 bitselect(int2 x, int2 y, int2 mask)
{
    return (x & ~mask) | (y & mask);
}
SIMD_CALL int3 bitselect(int3 x, int3 y, int3 mask)
{
    return (x & ~mask) | (y & mask);
}
SIMD_CALL int4 bitselect(int4 x, int4 y, int4 mask)
{
    return (x & ~mask) | (y & mask);
}

SIMD_CALL int2 int2m(int x)
{
    return x;
}
SIMD_CALL int2 int2m(int x, int y)
{
    return {x, y};
}

SIMD_CALL int3 int3m(int x)
{
    return x;
}
SIMD_CALL int3 int3m(int x, int y, int z)
{
    return {x, y, z};
}
SIMD_CALL int3 int3m(int2 v, int z)
{
    int3 r;
    r.xy = v;
    r.z = z;
    return r;
}

SIMD_CALL int4 int4m(int x)
{
    return x;
}
SIMD_CALL int4 int4m(int2 xy, int2 zw)
{
    int4 r;
    r.xy = xy;
    r.zw = zw;
    return r;
}
SIMD_CALL int4 int4m(int x, int y, int z, int w)
{
    return {x, y, z, w};
}
SIMD_CALL int4 int4m(int3 v, int w)
{
    int4 r;
    r.xyz = v;
    r.w = w;
    return r;
}

} //namespace SIMD_NAMESPACE
#endif // __cplusplus
#endif // USE_SIMDLIB && SIMD_INT
