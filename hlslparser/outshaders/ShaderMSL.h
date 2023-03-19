
// glslc doesn't support, but this header is metal only
#pragma once

// TODO: support function_constants in MSL, is there HLSL equivalent yet
// [[function_constant(index)]]
//
// Here's someone trying to solve for HLSL by modding DXIL from DXC
// https://twitter.com/RandomPedroJ/status/1532725156623286272
// Vulkan doesn't run on older Intel, but DX12 does.

// Header can be pulled into regular code to build
// Taken from KramShaders.h

#ifndef __METAL_VERSION__
#import <Foundation/Foundation.h>
#else
#include <metal_stdlib>
#endif

#import <simd/simd.h>

#ifdef __METAL_VERSION__
#define NS_ENUM(_type, _name) \
    enum _name : _type _name; \
    enum _name : _type
#endif

// This isn't standard enum convention where enum starts with enum name
// but this allows semantic passthrough of parser.
typedef NS_ENUM(int32_t, VA) {
    POSITION = 0,
    
    NORMAL = 2,
    TANGENT = 3,
    BITANGENT = 4,
    
    BLENDINDICES = 5,
    BLENDWEIGHT = 6,
    
    COLOR0 = 7,
    
    TEXCOORD0 = 8,
    TEXCOORD1 = 9,
    TEXCOORD2 = 10,
    TEXCOORD3 = 11,
    TEXCOORD4 = 12,
    TEXCOORD5 = 13,
    TEXCOORD6 = 14,
    TEXCOORD7 = 15,
};

// May want to only do using in the .metal files themselvs.
using namespace metal;
using namespace simd;

// can safely use half on Metal
#define USE_HALF 1

// #define mad precise::fma"
    
float mad(float a, float b, float c) {
    return a * b + c;
}
float2 mad(float2 a, float2 b, float2 c) {
    return a * b + c;
}
float3 mad(float3 a, float3 b, float3 c) {
    return a * b + c;
}
float4 mad(float4 a, float4 b, float4 c) {
    return a * b + c;
}

// DirectX couldn't simply use operator * in all these years
// so have to use a function call mul.

// Might be easier to use * instead
float2x2 mul(float a, float2x2 m) { return a * m; }
float3x3 mul(float a, float3x3 m) { return a * m; }
float4x4 mul(float a, float4x4 m) { return a * m; }

float2x2 mul(float2x2 m, float a) { return a * m; }
float3x3 mul(float3x3 m, float a) { return a * m; }
float4x4 mul(float4x4 m, float a) { return a * m; }

float2 mul(float2 a, float2x2 m) { return a * m; }
float3 mul(float3 a, float3x3 m) { return a * m; }
float4 mul(float4 a, float4x4 m) { return a * m; }

float2 mul(float2x2 m, float2 a) { return m * a; }
float3 mul(float3x3 m, float3 a) { return m * a; }
float4 mul(float4x4 m, float4 a) { return m * a; }

//float3 mul(float4 a, float3x4 m) { return a * m; } // why no macro ?
//float2 mul(float4 a, float2x4 m) { return a * m; }

#if USE_HALF

half mad(half a, half b, half c) {
    return a * b + c;
}
half2 mad(half2 a, half2 b, half2 c) {
    return a * b + c;
}
half3 mad(half3 a, half3 b, half3 c) {
    return a * b + c;
}
half4 mad(half4 a, half4 b, half4 c) {
    return a * b + c;
}

half2x2 mul(half a, half2x2 m) { return a * m; }
half3x3 mul(half a, half3x3 m) { return a * m; }
half4x4 mul(half a, half4x4 m) { return a * m; }

half2x2 mul(half2x2 m, half a) { return a * m; }
half3x3 mul(half3x3 m, half a) { return a * m; }
half4x4 mul(half4x4 m, half a) { return a * m; }

half2 mul(half2 a, half2x2 m) { return a * m; }
half3 mul(half3 a, half3x3 m) { return a * m; }
half4 mul(half4 a, half4x4 m) { return a * m; }

half2 mul(half2x2 m, half2 a) { return m * a; }
half3 mul(half3x3 m, half3 a) { return m * a; }
half4 mul(half4x4 m, half4 a) { return m * a; }

#endif

// TODO: parser could replace these intrinsic names in metal
#define lerp mix
#define rcp recip
#define ddx dfdx
#define ddy dfdy
#define frac fract
#define isinfinite isinf
#define degrees(x) ((x) / (M_PI/180.0))
#define radians(x) ((x) * (M_PI/180.0))
#define reversebits(x) reverse_bits(x))

// bit ops
#define countbits(x) popcount(x)
#define firstbithigh(x) clz(x)
#define firstbitlow(x) ctz(x)
 
#define clip(x) if (all((x) < 0.0) discard_fragment()

// Use templated type to pass tex + sampler combos
//template<typename T>
//struct TexSampler
//{
//    T t;
//    sampler s;
//};


//---------

// gather only works on mip0
float4 GatherRed(texture2d<float> t, sampler s, float2 texCoord, int2 offset=0) {
    return t.gather(s, texCoord, offset, component::x);
}
  
float4 GatherGreen(texture2d<float> t, sampler s, float2 texCoord,  int2 offset=0) {
    return t.gather(s, texCoord, offset, component::y);
}

float4 GatherBlue(texture2d<float> t, sampler s, float2 texCoord,  int2 offset=0) {
    return t.gather(s, texCoord, offset, component::z);
}

float4 GatherAlpha(texture2d<float> t, sampler s, float2 texCoord, int2 offset=0) {
    return t.gather(s, texCoord, offset, component::w);
}

//---------

float4 SampleGrad(texture2d<float> t, sampler s, float2 texCoord, float2 gradx, float2 grady) {
   return t.sample(s, texCoord.xy, gradient2d(gradx, grady));
}

//---------

#if USE_HALF

half4 SampleH(texture2d<half> t, sampler s, float2 texCoord) {
    return t.sample(s, texCoord);
}

half4 SampleLevelH(texture2d<half> t, sampler s, float4 texCoordMip) {
    return t.sample(s, texCoordMip.xy, level(texCoordMip.w));
}

half4 SampleBiasH(texture2d<half> t, sampler s, float4 texCoordBias) {
    return t.sample(s, texCoordBias.xy, bias(texCoordBias.w));
}

#else

#define SampleH Sample
#define SampleLevelH SampleLevel
#define SampleBiasH SampleBias

#endif



float4 SampleLevel(texture2d<float> t, sampler s, float4 texCoordMip) {
    return t.sample(s, texCoordMip.xy, level(texCoordMip.w));
}

float4 SampleLevel(texturecube<float> t, sampler s, float4 texCoordMip) {
    return t.sample(s, texCoordMip.xyz, level(texCoordMip.w));
}

float4 SampleLevel(texture3d<float> t, sampler s, float4 texCoordMip) {
    return t.sample(s, texCoordMip.xyz, level(texCoordMip.w));
}

// TODO: may need to add to intrinsics
//float4 SampleLevel(texture2d_array<float> t, sampler s, float4 texCoordMip) {
//    return t.sample(s, texCoordMip.xyz, level(texCoordMip.w));
//}
//float4 SampleLevel(texturecube_array<float> t, sampler s, float4 texCoordMip) {
//    return t.sample(s, texCoordMip.xyz, level(texCoordMip.w));
//}

// ----

float4 SampleBias(texturecube<float> t, sampler s, float4 texCoordBias) {
    return t.sample(s, texCoordBias.xyz, bias(texCoordBias.w));
}
   
float4 SampleBias(texture2d<float> t, sampler s, float4 texCoordBias) {
    return t.sample(s, texCoordBias.xy, bias(texCoordBias.w));
}

//------

// see if some of these have offset
float4 Load(texture2d<float> t, int2 texCoord, int lod = 0)
{
    return t.read((uint2)texCoord, (uint)lod);
}

float4 Load(texture3d<float> t, int3 texCoord, int lod = 0)
{
    return t.read((uint3)texCoord, (uint)lod);
}

float4 Load(texture2d_array<float> t, int3 texCoord, int lod = 0)
{
    return t.read((uint2)texCoord.xy, (uint)texCoord.z, (uint)lod);
}

// no HLSL equivalent, so don't define for MSL.  Maybe it's just offset that doesn't.
//float4 Load(texturecube<float> t, int3 texCoord, int lod = 0)
//{
//    uv, face, lod, offset
//    return t.read((uint2)texCoord.xy, (uint)texCoord.z, (uint2)lod);
//}
//
//float4 Load(texturecube_array<float> t, int4 texCoord, int lod = 0)
//{
//    return t.read((uint2)texCoord.xy, (uint)texCoord.z, (uint)texcoord.w, (uint)lod);
//}

// this doesn't use SamplerState, raw load
float4 Load(texture2d_ms<float> t, int2 texCoord, int sample) {
    return t.read((uint2)texCoord, (uint)sample);
}

// also write call (Store in HLSL)

// ----

float4 Sample(texture2d_array<float> t, sampler s, float3 texCoord, int2 offset=0) {
    return t.sample(s, texCoord.xy, uint(texCoord.z), offset);
}
float4 Sample(texture2d<float> t, sampler s, float2 texCoord, int2 offset=0) {
    return t.sample(s, texCoord, offset);
}
float4 Sample(texture3d<float> t, sampler s, float3 texCoord, int3 offset=0) {
    return t.sample(s, texCoord, offset);
}
float4 Sample(texturecube<float> t, sampler s, float3 texCoord) {
    return t.sample(s, texCoord);
}
float4 Sample(texturecube_array<float> t, sampler s, float4 texCoord) {
    return t.sample(s, texCoord.xyz, uint(texCoord.w));
}
//----------

float4 Sample(depth2d<float> t, sampler s, float2 texCoord, int2 offset = 0)
{
    return t.sample(s, texCoord.xy, offset);
}


// ios may need to hardcode sampler for compare
// constexpr sampler shadowSampler(mip_filter::none, min_filter::linear, mag_filter::linear, address::clamp_to_border, compare_func::greater);


// For persp shadows, remember to divide z = z/w before calling, or w = z/w on cube
float SampleCmp(depth2d<float> t, sampler s, float4 texCompareCoord, int2 offset = 0)
{
    return t.sample_compare(s, texCompareCoord.xy, texCompareCoord.z, offset);
}

float4 GatherCmp(depth2d<float> t, sampler s, float4 texCompareCoord, int2 offset = 0)
{
    return t.gather_compare(s, texCompareCoord.xy, texCompareCoord.z, offset);
}


// ----

// get_num_mip_levels, get_array_size
// get_width/height/depth(lod)
// TODO: need half versions
int2 GetDimensions(texture2d<float> t)
{
    int2 size(t.get_width(), t.get_height());
    return size;
}

int3 GetDimensions(texture3d<float> t)
{
    int3 size(t.get_width(), t.get_height(), t.get_depth());
    return size;
}

int2 GetDimensions(texturecube<float> t)
{
    int2 size(t.get_width(), t.get_width());
    return size;
}

int3 GetDimensions(texturecube_array<float> t)
{
    int3 size(t.get_width(), t.get_width(), t.get_array_size());
    return size;
}

int3 GetDimensions(texture2d_array<float> t)
{
    int3 size(t.get_width(), t.get_height(), t.get_array_size());
    return size;
}

int2 GetDimensions(texture2d_ms<float> t)
{
    int2 size(t.get_width(), t.get_height());
    return size;
}

// For textures, T can be half, float, short, ushort, int, or uint.
// For depth texture types, T must be float.
//
// texture2d_ms, texture2d_msaa_array
//
// depth2d, _ms, _ms_array, _array,
// depthcube, depthcube_array

/// TODO: add sparse_sample options
//template <typename T>
//struct sparse_color {
//public:
// constexpr sparse_color(T value, bool resident) thread;
// // Indicates whether all memory addressed to retrieve the value was
//mapped.
// constexpr bool resident() const thread;
//
// // Retrieve the color value.
// constexpr T const value() const thread;
//};
// sparse_sample(s, coord, offset), sparse_gather, sparse_sample_compare, sparse_gather_compare
// min_lod_clamp(float) option to sample

// gradientcube, gradient3d, min_lod_clamp(float lod),
// bias(float value), level(float lod)
// uint get_num_samples() const
//
// can have textures in structs, would help pass tex + sampler
//   but already had that before in hlslparser.  Could
//   bring that back, but have those built by caller.
// could code rewrite calls to pass tex/sampler into them
//   and then don't need the struct wrapper in MSL.  That
// severly limits sharing structs, functions.  The structs
//   don't really need to be in there.
//
// struct Foo {
// texture2d<float> a [[texture(0)]];
// depth2d<float> b [[texture(1)]];
// };
//


// handle access specifier RWTexture mods the template arg
// texture2d<float, access::write> a;
// on iOS, Writable textures aren’t supported within an argument buffer.
// 31/96/50000 buffers+textures for A7, A11, A13/Tier2
// 16/16/2048 samplers
// 64/128/16 on Tier1 macOS, see above for Tier2 (discrete gpu)

// MSL 2.3 has function pointers
// MSL 2.4 has compute recursion

    

