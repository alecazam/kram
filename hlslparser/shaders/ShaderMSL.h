#ifndef ShaderMSL_h
#define ShaderMSL_h

// glslc doesn't support #pragma once
//#pragma once

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
    
    VABlendIndices = 4,
    VABlendWeight = 5,
    
    VAColor0 = 6,
    
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

struct Texture2DSampler {
    Texture2DSampler(thread const texture2d<float>& t, thread const sampler& s) : t(t), s(s) {};
    const thread texture2d<float>& t;
    const thread sampler& s;
};

#if USE_HALF
struct Texture2DHalfSampler {
    Texture2DHalfSampler(thread const texture2d<half>& t, thread const sampler& s) : t(t), s(s) {};
    const thread texture2d<half>& t;
    const thread sampler& s;
};
#endif

struct Texture3DSampler {
    Texture3DSampler(thread const texture3d<float>& t, thread const sampler& s) : t(t), s(s) {};
    const thread texture3d<float>& t;
    const thread sampler& s;
};

struct TextureCubeSampler {
    TextureCubeSampler(thread const texturecube<float>& t, thread const sampler& s) : t(t), s(s) {};
    const thread texturecube<float>& t;
    const thread sampler& s;
};

struct Texture2DShadowSampler {
    Texture2DShadowSampler(thread const depth2d<float>& t, thread const sampler& s) : t(t), s(s) {};
    const thread depth2d<float>& t;
    const thread sampler& s;
};

struct Texture2DArraySampler {
    const thread texture2d_array<float>& t;
    const thread sampler& s;
    Texture2DArraySampler(thread const texture2d_array<float>& t, thread const sampler& s) : t(t), s(s) {};
};
    
int2 tex2Dsize(Texture2DSampler ts) {
    int2 size(ts.t.get_width(), ts.t.get_height());
    return size;
}


int3 tex3Dsize(Texture3DSampler ts) {
    int3 size(ts.t.get_width(), ts.t.get_height(), ts.t.get_depth());
    return size;
}

int texCUBEsize(TextureCubeSampler ts) {
    int size(ts.t.get_width());
    return size;
}
    
    
float4 tex2D(Texture2DSampler ts, float2 texCoord) {
    return ts.t.sample(ts.s, texCoord);
}

// don't use for PCF
float4 tex2Dproj(Texture2DSampler ts, float4 texCoord) {
    return ts.t.sample(ts.s, texCoord.xy / texCoord.w);
}

// gather only works on mip0
float4 tex2DgatherRed(Texture2DSampler ts, float2 texCoord, int2 offset=0) {
    return ts.t.gather(ts.s, texCoord, offset, component::x); // TODO: int to component
}
  
float4 tex2DgatherGreen(Texture2DSampler ts, float2 texCoord,  int2 offset=0) {
    return ts.t.gather(ts.s, texCoord, offset, component::y); // TODO: int to component
}

float4 tex2DgatherBlue(Texture2DSampler ts, float2 texCoord,  int2 offset=0) {
    return ts.t.gather(ts.s, texCoord, offset, component::z); // TODO: int to component
}

float4 tex2DgatherAlpha(Texture2DSampler ts, float2 texCoord, int2 offset=0) {
    return ts.t.gather(ts.s, texCoord, offset, component::w); // TODO: int to component
}

float4 tex2Dlod(Texture2DSampler ts, float4 texCoordMip) {
    return ts.t.sample(ts.s, texCoordMip.xy, level(texCoordMip.w));
}

float4 tex2Dgrad(Texture2DSampler ts, float2 texCoord, float2 gradx, float2 grady) {
   return ts.t.sample(ts.s, texCoord.xy, gradient2d(gradx, grady));
}

float4 tex2Dbias(Texture2DSampler ts, float4 texCoordBias) {
    return ts.t.sample(ts.s, texCoordBias.xy, bias(texCoordBias.w));
}

float4 tex2Dfetch(Texture2DSampler ts, int2 texCoord) {
    return ts.t.read((uint2)texCoord);
}

#if USE_HALF

// use samper2D<half> to specify these

half4 tex2DH(Texture2DHalfSampler ts, float2 texCoord) {
    return ts.t.sample(ts.s, texCoord);
}

half4 tex2DHlod(Texture2DHalfSampler ts, float4 texCoordMip) {
    return ts.t.sample(ts.s, texCoordMip.xy, level(texCoordMip.w));
}

half4 tex2DHbias(Texture2DHalfSampler ts, float4 texCoordBias) {
    return ts.t.sample(ts.s, texCoordBias.xy, bias(texCoordBias.w));
}

#else

#define tex2DH tex2D
#define tex2DHlod tex2Dlod
#define tex2DHbias tex2Dbias

#endif

float4 tex3D(Texture3DSampler ts, float3 texCoord) {
    return ts.t.sample(ts.s, texCoord);
}


float4 tex3Dlod(Texture3DSampler ts, float4 texCoordMip) {
    return ts.t.sample(ts.s, texCoordMip.xyz, level(texCoordMip.w));
}

float4 texCUBE(TextureCubeSampler ts, float3 texCoord) {
    return ts.t.sample(ts.s, texCoord);
}

float4 texCUBElod(TextureCubeSampler ts, float4 texCoordMip) {
    return ts.t.sample(ts.s, texCoordMip.xyz, level(texCoordMip.w));
}

float4 texCUBEbias(TextureCubeSampler ts, float4 texCoordBias) {
    return ts.t.sample(ts.s, texCoordBias.xyz, bias(texCoordBias.w));
}
    
// iOS may need shadow sampler inline
//    float4 tex2Dcmp(Texture2DShadowSampler ts, float4 texCoordCompare) {
//        constexpr sampler shadow_constant_sampler(mip_filter::none, min_filter::linear, mag_filter::linear, address::clamp_to_edge, compare_func::less);"
//        return ts.t.sample_compare(shadow_constant_sampler, texCoordCompare.xy, texCoordCompare.z);
//    }


float4 tex2Dcmp(Texture2DShadowSampler ts, float4 texCoordCompare) {
    return ts.t.sample_compare(ts.s, texCoordCompare.xy, texCoordCompare.z);
}

float4 tex2DMSfetch(texture2d_ms<float> t, int2 texCoord, int sample) {
    return t.read((uint2)texCoord, (uint)sample);
}

float4 tex2DArray(Texture2DArraySampler ts, float3 texCoord) {
    return ts.t.sample(ts.s, texCoord.xy, texCoord.z + 0.5); // 0.5 offset needed on nvidia gpus
}
        
#endif // ShaderMSL_h

