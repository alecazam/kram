
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

typedef NS_ENUM(int32_t, VA) {
    VAPosition = 0,
    VATexcoord = 1,
    
    VANormal = 2,
    VATangent = 3,
    VABitangent = 4,
    
    VABlendIndices = 5,
    VABlendWeight = 6,
    
    VAColor0 = 7,
    
    VATexcoord0 = 8,
    VATexcoord1 = 9,
    VATexcoord2 = 10,
    VATexcoord3 = 11,
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

float2 mul(float2 a, float2x2 m) { return a * m; }
float3 mul(float3 a, float3x3 m) { return a * m; }
float4 mul(float4 a, float4x4 m) { return a * m; }

float2 mul(float2x2 m, float2 a) { return m * a; }
float3 mul(float3x3 m, float3 a) { return m * a; }
float4 mul(float4x4 m, float4 a) { return m * a; }

float3 mul(float4 a, float3x4 m) { return a * m; } // why no macro ?
float2 mul(float4 a, float2x4 m) { return a * m; }

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

half2 mul(half2 a, half2x2 m) { return a * m; }
half3 mul(half3 a, half3x3 m) { return a * m; }
half4 mul(half4 a, half4x4 m) { return a * m; }

half2 mul(half2x2 m, half2 a) { return m * a; }
half3 mul(half3x3 m, half3 a) { return m * a; }
half4 mul(half4x4 m, half4 a) { return m * a; }

half3 mul(half4 a, half3x4 m) { return a * m; } // why no macro ?
half2 mul(half4 a, half2x4 m) { return a * m; }
#endif

#define lerp mix
#define rcp recip
#define ddx dfdx
#define ddy dfdy
#define frac fract
#define isinfinite isinf

void clip(float x) {
    if (x < 0.0) discard_fragment();
}


    
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

#define tex2DH tex2D
#define tex2DHlod tex2Dlod
#define tex2DHbias tex2Dbias

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

// ios may need to hardcode sampler
// constexpr sampler shadowSampler(mip_filter::none, min_filter::linear, mag_filter::linear, address::clamp_to_border, compare_func::greater);

// May have to detect SamplerComparisonState, and mark texture as depth2d
float4 SampleCmp(depth2d<float> t, sampler s, float4 texCoordCompare) {
    // division for perspective shadows, but caller should handle this
    return t.sample_compare(s, texCoordCompare.xy, texCoordCompare.z / texCoordCompare.w );
}

float4 Load(texture2d_ms<float> t, int2 texCoord, int sample) {
    return t.read((uint2)texCoord, (uint)sample);
}

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


// ----

// get_num_mip_levels, get_array_size
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

int2 GetDimensions(texturecube_array<float> t)
{
    // TODO: arrayCount?
    int2 size(t.get_width(), t.get_width());
    return size;
}

int2 GetDimensions(texture2d_array<float> t)
{
    // TODO: arrayCount?
    int2 size(t.get_width(), t.get_height());
    return size;
}
