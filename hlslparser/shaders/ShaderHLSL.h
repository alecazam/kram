#ifndef ShaderHLSL_h
#define ShaderHLSL_h

// glslc doesn't support #pragma once
//#pragma once

#define USE_HALF 1

// Don't know why HLSL doesn't support these
#define min3(x,y,z) min(x, min(y, z))
#define max3(x,y,z) max(x, max(y, z))
#define length_squared(x) ((x)*(x))

struct Texture2DSampler {
    Texture2D t;
    SamplerState s;
};

// TODO: this only supports half on Texture2D
#if USE_HALF
// unique type, even though same data
struct Texture2DHalfSampler {
    Texture2D t;
    SamplerState s;
};

#endif

struct Texture3DSampler {
    Texture3D t;
    SamplerState s;
};

struct TextureCubeSampler {
    TextureCube t;
    SamplerState s;
};

struct Texture2DShadowSampler {
    Texture2D t;
    SamplerComparisonState s;
};

struct Texture2DArraySampler {
    Texture2DArray t;
    SamplerState s;
};

//------------------------------

// https://colinbarrebrisebois.com/2021/11/01/working-around-constructors-in-hlsl-or-lack-thereof/
// Stupid HLSL lacks ctors.  Ugh!
// Can't simplify to "return { t, a };" either.

Texture2DSampler Texture2DSamplerCtor(Texture2D t, SamplerState s)
{
    Texture2DSampler a = { t, s };
    return a;
}

#if USE_HALF

Texture2DHalfSampler Texture2DHalfSamplerCtor(Texture2D t, SamplerState s)
{
    Texture2DHalfSampler a = { t, s };
    return a;
}
#endif

Texture3DSampler Texture3DSamplerCtor(Texture3D t, SamplerState s)
{
    Texture3DSampler a = { t, s };
    return a;
}

TextureCubeSampler TextureCubeSamplerCtor(TextureCube t, SamplerState s)
{
    TextureCubeSampler a = { t, s };
    return a;
}

Texture2DShadowSampler Texture2DShadowSamplerCtor(Texture2D t, SamplerComparisonState s)
{
    Texture2DShadowSampler a = { t, s };
    return a;
}

Texture2DArraySampler Texture2DArraySamplerCtor(Texture2DArray t, SamplerState s)
{
    Texture2DArraySampler a = { t, s };
    return a;
}

//------------------------------

float4 tex2D(Texture2DSampler ts, float2 texCoord) {
    return ts.t.Sample(ts.s, texCoord);
}

// gather only works on mip0
float4 tex2DgatherRed(Texture2DSampler ts, float2 texCoord, int2 offset=0) {
    return ts.t.GatherRed(ts.s, texCoord, offset);
}

float4 tex2DgatherGreen(Texture2DSampler ts, float2 texCoord, int2 offset=0) {
    return ts.t.GatherGreen(ts.s, texCoord, offset);
}

float4 tex2DgatherBlue(Texture2DSampler ts, float2 texCoord, int2 offset=0) {
    return ts.t.GatherBlue(ts.s, texCoord, offset);
}

float4 tex2DgatherAlpha(Texture2DSampler ts, float2 texCoord, int2 offset=0) {
    return ts.t.GatherAlpha(ts.s, texCoord, offset);
}

int2 tex2Dsize(Texture2DSampler ts) {
    int2 size;
    ts.t.GetDimensions(size.x, size.y);
    return size;
}

int3 tex3Dsize(Texture3DSampler ts) {
    int3 size;
    ts.t.GetDimensions(size.x, size.y, size.z);
    return size;
}

int texCUBEsize(TextureCubeSampler ts) {
    int size;
    ts.t.GetDimensions(size, size); // sizexsize
    return size;
}

// don't use for PCF
float4 tex2Dproj(Texture2DSampler ts, float4 texCoord) {
    return ts.t.Sample(ts.s, texCoord.xy / texCoord.w);
}

float4 tex2Dlod(Texture2DSampler ts, float4 texCoord, int2 offset = 0) {
    return ts.t.SampleLevel(ts.s, texCoord.xy, texCoord.w, offset);
}

float4 tex2Dbias(Texture2DSampler ts, float4 texCoord) {
    return ts.t.SampleBias(ts.s, texCoord.xy, texCoord.w);
}

float4 tex2Dgrad(Texture2DSampler ts, float2 texCoord, float2 gradx, float2 grady) {
   return ts.t.SampleGrad(ts.s, texCoord.xy, gradx, grady);
}

// This only applies to mip0
float4 tex2Dcmp(Texture2DShadowSampler ts, float4 texCoord) {
    return ts.t.SampleCmpLevelZero(ts.s, texCoord.xy, texCoord.z);
}

// This is int3 returning int2 in HLSL
//float4 tex2Dfetch(Texture2DSampler ts, int2 texCoord) {
//    return ts.t.Load(texCoord);
//}

float4 tex2DMSfetch(Texture2DMS<float4> t, int2 texCoord, int sample) {
    return t.Load(texCoord, sample);
}

#if USE_HALF

half4 tex2DH(Texture2DHalfSampler ts, float2 texCoord) {
    return (half4)ts.t.Sample(ts.s, texCoord);
}

half4 tex2DHlod(Texture2DHalfSampler ts, float4 texCoordMip) {
    return (half4)ts.t.Sample(ts.s, texCoordMip.xy, texCoordMip.w);
}

half4 tex2DHbias(Texture2DHalfSampler ts, float4 texCoordBias) {
    return (half4)ts.t.SampleBias(ts.s, texCoordBias.xy, texCoordBias.w);
}

#else

#define tex2DH tex2D
#define tex2DHlod tex2Dlod
#define tex2DHbias tex2Dbias

#endif

float4 tex3D(Texture3DSampler ts, float3 texCoord) {
    return ts.t.Sample(ts.s, texCoord);
}

float4 tex3Dlod(Texture3DSampler ts, float4 texCoord) {
    return ts.t.SampleLevel(ts.s, texCoord.xyz, texCoord.w);
}

float4 texCUBE(TextureCubeSampler ts, float3 texCoord) {
    return ts.t.Sample(ts.s, texCoord);
}

float4 texCUBElod(TextureCubeSampler ts, float4 texCoord) {
    return ts.t.SampleLevel(ts.s, texCoord.xyz, texCoord.w);
}

float4 texCUBEbias(TextureCubeSampler ts, float4 texCoord) {
    return ts.t.SampleBias(ts.s, texCoord.xyz, texCoord.w);
}

// TODO: fix this
//float4 tex2DArray(Texture2DArraySampler ts, float3 texCoord) {
//    return ts.t.Sample(ts.s, texCoord.xy, texCoord.z + 0.5); // 0.5 offset needed on nvidia gpus
//}

#endif // ShaderHLSL_h
    
