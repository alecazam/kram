// kram - Copyright 2020-2024 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.
#include "vectormath234.h"

#if USE_SIMDLIB

// Tests with godbolt are here to show code comparsions with optimizations.

// ---------------
// Note: float4a.h has a rcp and rsqrt ops, but they are approximate.
// Have real div and sqrt ops now.
// ---------------
// The storage of affine data in a column matrix is no different than rows
// translation is in (r30 r31 r32) or in (c30, c31 c32)
//
// r0: r00 r01 r02 0
// r1: r10         0
// r2: r20         0
// r3: px  py  pz  1
//
// c0  c1  c2  c3
// c00 c10 c20 px
// c01 c11     py
// c02         pz
//  0   0   0  1
//
// col: TRS * TRS * v   cameraToWorldTfm * worldToModelTfm * ..
// row: v * SRT * SRT   modelToWorldTfm * worldToCameraTfm * ...
//
// ---------------
// So there are currently 6 version of the Accelerate lib.
// This library hides implementations of some of the calls.
// So need to rely on a version of the lib to get them,
// or supply some alternative.  Many calls have fallbacks.
//
// 6: macOS 15.0, iOS 18.0
// 5: macOS 13.0, iOS 16.0
// 4: macOS 12.0, iOS 15.0
// 0: header only
//
// use 5 for macOS
// SIMD_LIBRARY_VERSION >= 5
//
// use 4 for iOS
// SIMD_LIBRARY_VERSION >= 4
//
//-----------------
//
// DONE: rename in README, and name of .cpp/h
// DONE: split up files into types, float ops, double ops
// DONE: limit !SIMD_FLOAT_EXT to only 32B vector types?  Have 64B vecs.
// DONE: ryg on 32B ops on AVX systems
//   These often only have 16B simd units, so running 32B ops isn't efficient.
//   This could apply say to PS4/AMD chips too.
// DONE: bring over fast inverses (RTS, RTU, etc)
// DONE: need translation, rotation, scale
// DONE: verify size/alignments are same across Win/macOS
// DONE: add optimized vec2 ops on Neon
// DONE: add AVX2 for double4
// DONE: build an optimized Xcode library
// DONE: check if packed to aligned conversions are automatic

//-----------------

// TODO: ryg on fp16 <-> fp32
//   Not the right gist, you want the RTNE one (nm: that only matters for float->half,
//   this was the half->float one. FWIW, other dir is https://gist.github.com/rygorous/eb3a019b99fdaa9c3064.
//   These days I use a variant of the RTNE/RN version that also preserves NaN payload bits,
//   which is slightly more ops but matches hardware conversions exactly for every input, including all NaNs.
// TODO: build Xcode library that is a clang module or framework
// TODO: build VisualStudio library with cmake, clang module too?
// TODO: need fast post-translation, post-rotation, post-scale
// TODO: need euler <-> matrix
// TODO: saturating conversions (esp. integer) would be useful too and prevent overflow
//   bit select to clamp values.
// TODO: need natvis and lldb formatting of math classes.

//-----------------
// Links

// here's a decomp
// https://github.com/erich666/GraphicsGems/blob/master/gemsii/unmatrix.c
//

// intrinsic tables
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html

// older but good talk on simd
// https://people.inf.ethz.ch/markusp/teaching/263-2300-ETH-spring14/slides/11-simd.pdf

// another article
// https://www.cs.uaf.edu/courses/cs441/notes/sse-avx/

// aarch64
// https://en.wikipedia.org/wiki/AArch64

// make win happy for va_copy in format call
#include <stdarg.h>

#if SIMD_ACCELERATE_MATH
#include <simd/base.h>

// NOTE: this reports 5 for macOS 13 minspec, but SIMD_LIBRARY_VERSION is set to 6.
//   This is a problem, since some lib code only exists on macOS 15 and iOS 18 then.
// Can remove this once SIMD_LIBRARY_VERSION is correct.
// Also unclear what XR_OS_1_0 library support there is.  It's not in the comparisons.
# if SIMD_COMPILER_HAS_REQUIRED_FEATURES
#  if __has_include(<TargetConditionals.h>) && __has_include(<Availability.h>)
#   include <TargetConditionals.h>
#   include <Availability.h>
#   if TARGET_OS_RTKIT
#    define SIMD_LIBRARY_VERSION SIMD_CURRENT_LIBRARY_VERSION
#   elif __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_15_0   || \
        __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_18_0 || \
        __XR_OS_VERSION_MIN_REQUIRED     >= __XROS_2_0
#    define SIMD_LIBRARY_VERSION_TEST 6
#   elif __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_13_0   || \
        __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_16_0
#    define SIMD_LIBRARY_VERSION_TEST 5
#   elif   __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_12_0   || \
        __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_15_0
#    define SIMD_LIBRARY_VERSION_TEST 4
#   endif
#  endif
#endif


#if 0
// SIMD_LIBRARY_VERSION is set to 6 regardless of the minspec
// iOS 15 = 4, and macOS 13 = 5
#if TARGET_OS_OSX
    #if SIMD_LIBRARY_VERSION_TEST != 5
    blarg1
    #endif

    #if SIMD_LIBRARY_VERSION != 5
    blarg2 // this fires
    #endif
#else
    #if SIMD_LIBRARY_VERSION_TEST != 4
    blarg1
    #endif

    #if SIMD_LIBRARY_VERSION != 4
    blarg2 // this fires
    #endif
#endif
#endif

#endif // SIMD_ACCELERATE_MATH



namespace SIMD_NAMESPACE {

void TestCalls()
{
#if SIMD_FLOAT
    float4a va = 0;
    float4p vp = (float)1;
    
    va = vp;
    vp = va;
#endif
    
}

// Check format arguments.
#ifndef __printflike
#define __printflike(fmtIndex, varargIndex)
#endif

inline string format(const char* format, ...) __printflike(1, 2);

// was using kram::format, but wanted to decouple this lib
inline string format(const char* format, ...)
{
    string str;
    
    va_list args;
    va_start(args, format);
    
    // format once to get length (without NULL at end)
    va_list argsCopy;
    va_copy(argsCopy, args);
    int32_t len = vsnprintf(NULL, 0, format, argsCopy);
    va_end(argsCopy);

    // replace string
    str.resize(len, 0);
    vsnprintf((char*)str.c_str(), len + 1, format, args);
    
    va_end(args);
    
    return str;
}

#if SIMD_DOUBLE

string vecf::str(double2 v) const {
    return format("(%f %f)", v.x, v.y);
}
string vecf::str(double3 v) const {
    return format("(%f %f %f)", v.x, v.y, v.z);
}
string vecf::str(double4 v) const {
    return format("(%f %f %f %f)", v.x, v.y, v.z, v.w);
}
 
string vecf::str(const double2x2& m) const {
    return format("%s\n%s\n",
        str(m[0]).c_str(), str(m[1]).c_str());
}
string vecf::str(const double3x3& m) const {
    return format("%s\n%s\n%s\n",
        str(m[0]).c_str(), str(m[1]).c_str(), str(m[2]).c_str());
}
string vecf::str(const double4x4& m) const {
  return format("%s\n%s\n%s\n%s\n",
      str(m[0]).c_str(), str(m[1]).c_str(),
      str(m[2]).c_str(), str(m[3]).c_str());
}

#endif

//-----------------------------

#if SIMD_FLOAT

string vecf::str(float2 v) const {
    return format("(%f %f)", v.x, v.y);
}
string vecf::str(float3 v) const {
    return format("(%f %f %f)", v.x, v.y, v.z);
}
string vecf::str(float4 v) const {
    return format("(%f %f %f %f)", v.x, v.y, v.z, v.w);
}
 
string vecf::str(const float2x2& m) const {
    return format("%s\n%s\n",
        str(m[0]).c_str(), str(m[1]).c_str());
}
string vecf::str(const float3x3& m) const {
    return format("%s\n%s\n%s\n",
        str(m[0]).c_str(), str(m[1]).c_str(), str(m[2]).c_str());
}
string vecf::str(const float4x4& m) const {
  return format("%s\n%s\n%s\n%s\n",
      str(m[0]).c_str(), str(m[1]).c_str(),
      str(m[2]).c_str(), str(m[3]).c_str());
}

#endif // SIMD_FLOAT

#if SIMD_HALF

#if SIMD_HALF_FLOAT16

string vecf::str(half2 v) const {
    return format("(%f %f)", (double)v.x, (double)v.y);
}
string vecf::str(half3 v) const {
    return format("(%f %f %f)", (double)v.x, (double)v.y, (double)v.z);
}
string vecf::str(half4 v) const {
    return format("(%f %f %f %f)", (double)v.x, (double)v.y, (double)v.z, (double)v.w);
}

#elif SIMD_HALF4_ONLY

// this converts half4 to float, then just prints that
string vecf::str(half2 v) const {
    float4 vv = float4m(zeroext(v));
    return format("(%f %f)", vv.x, vv.y);
}
string vecf::str(half3 v) const {
    float4 vv = float4m(zeroext(v));
    return format("(%f %f %f)", vv.x, vv.y, vv.z);
}
string vecf::str(half4 v) const {
    float4 vv = float4m(v);
    return format("(%f %f %f %f)", vv.x, vv.y, vv.z, vv.w);
}

#endif // SIMD_HALF_FLOAT16

#endif // SIMD_HALF

#if SIMD_INT
string vecf::str(int2 v) const {
    return format("(%d %d)", v.x, v.y);
}
string vecf::str(int3 v) const {
    return format("(%d %d %d)", v.x, v.y, v.z);
}
string vecf::str(int4 v) const {
    return format("(%d %d %d %d)", v.x, v.y, v.z, v.w);
}
#endif

#if SIMD_LONG

// This works across Win and macOS, so don't need to use PRId64.
#define long1cast long long

string vecf::str(long2 v) const {
    return format("(%lld %lld)", (long1cast)v.x, (long1cast)v.y);
}
string vecf::str(long3 v) const {
    return format("(%lld %lld %lld)", (long1cast)v.x, (long1cast)v.y, (long1cast)v.z);
}
string vecf::str(long4 v) const {
    return format("(%lld %lld %lld %lld)", (long1cast)v.x, (long1cast)v.y, (long1cast)v.z, (long1cast)v.w);
}
#endif



//-----------------------------

#define FMT_SEP() s += "-----------\n"

string vecf::simd_configs() const {
    string s;
    
#define FMT_CONFIG(val) s += format("%s: %d\n", #val, val);
    
    FMT_CONFIG(SIMD_SSE);
    FMT_CONFIG(SIMD_NEON);
    
#if SIMD_SSE
    bool hasSSE42 = false;
    bool hasAVX = false;
    bool hasAVX2 = false;
    
    bool hasF16C = false;
    bool hasFMA = false;
    
    #if SIMD_SSE
    hasSSE42 = true;
    #endif
    #ifdef __AVX__
    hasAVX = true;
    #endif
    #if SIMD_AVX2
    hasAVX2 = true;
    #endif
    
    // TODO: AVX-512 flags (combine into one?)
    // (__AVX512F__) && (__AVX512DQ__) && (__AVX512CD__) && (__AVX512BW__) && (__AVX512VL__) && (__AVX512VBMI2__)
   
    #ifdef __F16C__
    hasF16C = true;
    #endif
    #ifdef __FMA__
    hasFMA = true;
    #endif
    
    if (hasAVX2)
        s += format("%s: %d\n", "AVX2 ", hasAVX2);
    else if (hasAVX)
        s += format("%s: %d\n", "AVX  ", hasAVX);
    else if (hasSSE42)
        s += format("%s: %d\n", "SSE42 ", hasSSE42);
    
    s += format("%s: %d\n", "F16C  ", hasF16C);
    s += format("%s: %d\n", "FMA   ", hasFMA);
    
    // fp-contract, etc ?
    // CRC (may not be worth it)
                                                    
#endif
    
#if SIMD_NEON
    // any neon setting, arm64 version
    // __ARM_VFPV4__
    // CRC (may not be worth it)
    
#endif
    
    FMT_CONFIG(SIMD_FLOAT_EXT);
    FMT_CONFIG(SIMD_HALF_FLOAT16);
#if SIMD_HALF
    FMT_CONFIG(SIMD_HALF4_ONLY);
#endif
    
    FMT_SEP();
    
    FMT_CONFIG(SIMD_CMATH_MATH);
    FMT_CONFIG(SIMD_ACCELERATE_MATH);
#if SIMD_ACCELERATE_MATH
    // Dump the min version. This is supposed to control SIMD_LIBRARY_VERSION
    #if __APPLE__
    #if TARGET_OS_OSX
        FMT_CONFIG(__MAC_OS_X_VERSION_MIN_REQUIRED);
    #elif TARGET_OS_VISION
        FMT_CONFIG(__XR_OS_VERSION_MIN_REQUIRED);
    #else
        FMT_CONFIG(__IPHONE_OS_VERSION_MIN_REQUIRED);
    #endif
    #endif
    
    FMT_CONFIG(SIMD_LIBRARY_VERSION); // lib based on min os target
    FMT_CONFIG(SIMD_CURRENT_LIBRARY_VERSION); // max lib based on sdk
    FMT_CONFIG(SIMD_LIBRARY_VERSION_TEST);
    FMT_CONFIG(SIMD_ACCELERATE_MATH_NAMES);
#endif
    
    FMT_SEP();
    
    FMT_CONFIG(SIMD_HALF);
    FMT_CONFIG(SIMD_FLOAT);
    FMT_CONFIG(SIMD_DOUBLE);
   
    FMT_CONFIG(SIMD_INT);
    FMT_CONFIG(SIMD_LONG);
    
    // don't have these implemented yet
    //FMT_CONFIG(SIMD_CHAR);
    //FMT_CONFIG(SIMD_SHORT);
    
#undef FMT_CONFIG
    
    return s;
}

string vecf::simd_alignments() const {
    string s;
    
#define FMT_CONFIG(val) s += format("%s: %zu %zu\n", #val, sizeof(val), __alignof(val));
    
    // TODO: add other types int, half?
    
#if SIMD_FLOAT
    FMT_SEP();
    
    FMT_CONFIG(float2);
    FMT_CONFIG(float3);
    FMT_CONFIG(float4);
    FMT_CONFIG(float8);
    //FMT_CONFIG(float16);
    
    FMT_CONFIG(float2x2);
    FMT_CONFIG(float3x3);
    FMT_CONFIG(float3x4);
    FMT_CONFIG(float4x4);
#endif
    
#if SIMD_DOUBLE
    FMT_SEP();
    
    FMT_CONFIG(double2);
    FMT_CONFIG(double3);
    FMT_CONFIG(double4);
    // FMT_CONFIG(double8);
    
    FMT_CONFIG(double2x2);
    FMT_CONFIG(double3x3);
    FMT_CONFIG(double3x4);
    FMT_CONFIG(double4x4);
#endif
    
#if SIMD_INT
    FMT_SEP();
    
    FMT_CONFIG(int2);
    FMT_CONFIG(int3);
    FMT_CONFIG(int4);
    FMT_CONFIG(int8);
    //FMT_CONFIG(int16);
#endif
    
#if SIMD_LONG
    FMT_SEP();
    
    FMT_CONFIG(long2);
    FMT_CONFIG(long3);
    FMT_CONFIG(long4);
    //FMT_CONFIG(long8);
#endif

    
#undef FMT_CONFIG
    
    return s;
}
    

//---------------

#if SIMD_HALF4_ONLY
 
#if SIMD_NEON

float4 float4m(half4 vv)
{
    return float4(vcvt_f32_f16(*(const float16x4_t*)&vv));
}
half4 half4m(float4 vv)
{
    return half4(vcvt_f16_f32(*(const float32x4_t*)&vv));
}

#endif // SIMD_NEON

#if SIMD_SSE

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

#endif // SIMD_SSE
#endif // SIMD_HALF4_ONLY

} // namespace SIMD_NAMESPACE
#endif // USE_SIMDLIB


