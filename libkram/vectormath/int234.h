// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

// This is not yet standalone.  vectormath++.h includes it.
#if USE_SIMDLIB && SIMD_INT

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// define c vector types
// Apple uses int type here (32-bit) instead of int32_t
macroVector4TypesStorage(int, int)
macroVector4TypesPacked(int, int)

#if SIMD_RENAME_TO_SIMD_NAMESPACE
macroVector4TypesStorageRenames(int, simd_int)
#endif // SIMD_RENAME_TO_SIMD_NAMESPACE

#ifdef __cplusplus
}

namespace SIMD_NAMESPACE {

macroVector4TypesStorageRenames(int, int)

//-----------------------------------
// imlementation - only code simd arch specific

#if SIMD_NEON

SIMD_CALL bool any(int2 x) {
    return vmaxv_u32(x) & 0x80000000;
}
SIMD_CALL bool any(int4 x) {
    return vmaxvq_u32(x) & 0x80000000;
}

SIMD_CALL bool all(int2 x) {
    return vminv_u32(x) & 0x80000000;
}
SIMD_CALL bool all(int4 x) {
    return vminvq_u32(x) & 0x80000000;
}

#endif // SIMD_NEON

// These take in int types, this is what comparison gens from a < b, etc.
#if SIMD_SSE

SIMD_CALL bool any(int2 x) {
    return _mm_movemask_ps(vec2to4(x)) & 0x3;
}
SIMD_CALL bool any(int4 x) {
    return _mm_movemask_ps((__m128)x);
}

SIMD_CALL bool all(int2 x) {
    return (_mm_movemask_ps(vec2to4(x)) & 0x3) == 0x3; // 2 bits
}
SIMD_CALL bool all(int4 x) {
    return _mm_movemask_ps((__m128)x) == 0xf; // 4 bits
}
#endif // SIMD_SSE
       

// end of implementation
//-----------------------------------

// bitselect
SIMD_CALL int2 bitselect(int2 x, int2 y, int2 mask) {
    return (x & ~mask) | (y & mask);
}
SIMD_CALL int3 bitselect(int3 x, int3 y, int3 mask) {
    return (x & ~mask) | (y & mask);
}
SIMD_CALL int4 bitselect(int4 x, int4 y, int4 mask) {
    return (x & ~mask) | (y & mask);
}

SIMD_CALL int2 int2m(int x) {
    return x;
}
SIMD_CALL int2 int2m(int x, int y) {
    return {x,y};
}

SIMD_CALL int3 int3m(int x) {
    return x;
}
SIMD_CALL int3 int3m(int x, int y, int z) {
    return {x,y,z};
}
SIMD_CALL int3 int3m(int2 v, float z) {
    int3 r; r.xy = v; r.z = z; return r;
}


SIMD_CALL int4 int4m(int x) {
    return x;
}
SIMD_CALL int4 int4m(int2 xy, int2 zw) {
    int4 r; r.xy = xy; r.zw = zw; return r;
}
SIMD_CALL int4 int4m(int x, int y, int z, int w) {
    return {x,y,z,w};
}
SIMD_CALL int4 int4m(int3 v, float w) {
    int4 r; r.xyz = v; r.w = w; return r;
}

}
#endif // __cplusplus
#endif // USE_SIMDLIB && SIMD_INT
