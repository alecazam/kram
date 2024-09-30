// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

// This is not yet standalone.  vectormath++.h includes it.
#if USE_SIMDLIB && SIMD_LONG

typedef int64_t long1;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// define c vector types
// Apple uses long type here (32-bit) instead of long32_t
macroVector8TypesStorage(long1, long)
macroVector8TypesPacked(long1, long)

#if SIMD_ACCELERATE_MATH_NAMES
macroVector8TypesStorageRenames(long, simd_long)
#endif // SIMD_ACCELERATE_MATH_NAMES

#ifdef __cplusplus
}

namespace SIMD_NAMESPACE {

macroVector8TypesStorageRenames(long, long)

//-----------------------------------
// imlementation - only code simd arch specific

#if SIMD_NEON

SIMD_CALL bool any(long2 x) {
    return (x.x | x.y) & 0x8000000000000000U;
}
SIMD_CALL bool any(long3 x) {
    return (x.x | x.y | x.z) & 0x8000000000000000U;
}
SIMD_CALL bool any(long4 x) {
    return any(x.lo | x.hi);
}

SIMD_CALL bool all(long2 x) {
    return (x.x & x.y) & 0x8000000000000000U;
}
SIMD_CALL bool all(long3 x) {
    return (x.x & x.y & x.z) & 0x8000000000000000U;
}
SIMD_CALL bool all(long4 x) {
    return all(x.lo & x.hi);
}

#endif // SIMD_NEON

// These take in long types, this is what comparison gens from a < b, etc.
#if SIMD_SSE

SIMD_CALL bool any(long2 x) {
    return _mm_movemask_pd(x) & 0x3; // 2 bits
}
SIMD_CALL bool any(long3 x) {
    // avx/2 have double4 op
    return (x.x | x.y) & 0x8000000000000000U;
}
SIMD_CALL bool any(long4 x) {
    // avx/2 have double4 op
    return any(x.lo | x.hi);
}

SIMD_CALL bool all(long2 x) {
    return (_mm_movemask_pd(x) & 0x3) == 0x3; // 2 bits
}
SIMD_CALL bool all(long3 x) {
    // avx/2 have double4 op
    return (x.x & x.y & x.z) & 0x8000000000000000U;
}
SIMD_CALL bool all(long4 x) {
    // avx/2 have double4 op
    return any(x.lo & x.hi);
}
#endif // SIMD_SSE
       
// end of implementation
//-----------------------------------

// bitselect
SIMD_CALL long2 bitselect(long2 x, long2 y, long2 mask) {
    return (x & ~mask) | (y & mask);
}
SIMD_CALL long3 bitselect(long3 x, long3 y, long3 mask) {
    return (x & ~mask) | (y & mask);
}
SIMD_CALL long4 bitselect(long4 x, long4 y, long4 mask) {
    return (x & ~mask) | (y & mask);
}

SIMD_CALL long2 long2m(long1 x) {
    return x;
}
SIMD_CALL long2 long2m(long1 x, long1 y) {
    return {x,y};
}

SIMD_CALL long3 long3m(long1 x) {
    return x;
}
SIMD_CALL long3 long3m(long1 x, long1 y, long1 z) {
    return {x,y,z};
}
SIMD_CALL long3 long3m(long2 v, long1 z) {
    long3 r; r.xy = v; r.z = z; return r;
}


SIMD_CALL long4 long4m(long1 x) {
    return x;
}
SIMD_CALL long4 long4m(long2 xy, long2 zw) {
    long4 r; r.xy = xy; r.zw = zw; return r;
}
SIMD_CALL long4 long4m(long1 x, long1 y, long1 z, long1 w) {
    return {x,y,z,w};
}
SIMD_CALL long4 long4m(long3 v, long1 w) {
    long4 r; r.xyz = v; r.w = w; return r;
}

}
#endif // __cplusplus
#endif // USE_SIMDLIB && SIMD_LONG
