#ifndef ShaderHLSL_h
#define ShaderHLSL_h

// glslc doesn't support but DXC does
// so had to add header guard
#ifdef __spirv__
#pragma once
#endif

// Don't know why HLSL doesn't support these
#define min3(x,y,z) min(x, min(y, z))
#define max3(x,y,z) max(x, max(y, z))
#define length_squared(x) ((x)*(x))
#define distance_squared(x,y) (((x)-(y))*((x)-(y)))

// no &* or ctors in HLSL limited C++
// This means operators cannot overload [+-*/>><<]=.  Only builtins work.

// HLSL2021 adds bitfields, so could define a color.
// They say they are on bw compatible with DX12 releases, but spirv backed should warn.
//struct ColorRGBA {
//  uint R : 8;
//  uint G : 8;
//  uint B : 8;
//  uint A : 8;
//};

// DX12 support SM6, DX11 support SM5 and prior.
// But only latest VS2022 supports SM6.6
// DXC should be translating calls back to Vulkan

// in HLSL 2021 logical operators can only be used with scalar values.
// new and/or constructs instead of using &&/||
// bool3 Cond = and(X, Y);
// bool3 Cond2 = or(X, Y);
// int3 Z = select(X, 1, 0);

// RW/ByteAddressBuffer added SM 5.1
// RW/StructuredBuffer added SM 5.1
// ConstantBuffer<T> foo[4] : register(b0) template supportes indexing too.
// added in SM 6.1.  So targeting DX12 6.2 seems ideal with half support.

// For float16 operations, denormal numbers must be preserved.
// No atomic operations for float16 are supported.

// no using, so do typedef
// this is ugly syntax
//typedef int16_t  short;
typedef int16_t2 short2;
typedef int16_t3 short3;
typedef int16_t4 short4;

typedef uint16_t  ushort;
typedef uint16_t2 ushort2;
typedef uint16_t3 ushort3;
typedef uint16_t4 ushort4;

//typedef int64_t  long;
typedef int64_t2 long2;
typedef int64_t3 long3;
typedef int64_t4 long4;

typedef uint64_t  ulong;
typedef uint64_t2 ulong2;
typedef uint64_t3 ulong3;
typedef uint64_t4 ulong4;

//typedef float64_t double;
typedef float64_t2 double2;
typedef float64_t3 double3;
typedef float64_t4 double4;

typedef float64_t2x2 double2x2;
typedef float64_t3x3 double3x3;
typedef float64_t4x4 double4x4;


// Note: no u/char
// Note: add double, but won't work on mobile (Android/MSL).
//  also Intel removed fp64 GPU support.  Often runs 1/64th speed.
//  But may be needed for ray-tracing large worlds.  Metal doesn't have double.

// TODO: add Atomics, more atomic u/long and float in SM 6.6
//  otherwise it's most atomic_u/int that is portable.
// Apple Metal 3 added atomic_float.

// 6.6 is cutting edge, want to target 6.2 for now
#define SM66 1
#if SM66
// compile to SM6.6 for these
typedef uint8_t4_packed uchar4_packed;
typedef int8_t4_packed char4_packed;

// signed do sign extend
ushort4 toUshort4(uchar4_packed packed)
{
    return unpack_u8u16(packed);
}
short4 toShort4(char4_packed packed)
{
    return unpack_s8s16(packed);
}
uint4 toUint4(uchar4_packed packed)
{
    return unpack_u8u32(packed);
}
int4 toInt4(char4_packed packed)
{
    return unpack_s8s32(packed);
}

// Are SM6.6 calls for pack_clamp_u8 using the wrong input type?
// https://github.com/microsoft/DirectXShaderCompiler/issues/5091
// pack lower 8
uchar4_packed fromUshort4(ushort4 v)
{
    return pack_u8(v);
}
uchar4_packed fromShort4ClampU(short4 v) 
{
    return pack_clamp_u8(v);
}
char4_packed fromShort4(short4 v, bool clamp = true)
{
    return  clamp ? pack_clamp_s8(v) : pack_s8(v);
}
uchar4_packed fromUint4(uint4 v)
{
    return pack_u8(v);
}
uchar4_packed fromInt4ClampU(int4 v)
{
    return pack_clamp_u8(v);
}
char4_packed fromInt4(int4 v, bool clamp = true)
{
    return clamp ? pack_clamp_s8(v) : pack_s8(v);
}
#endif


#define USE_HALF 1

// TODO: fix parsing, so don't have to provide these overrides
// The parser also has to rewrite params on MSL and wrap args.

//----------

float4 Sample(Texture2D<float4> t, SamplerState s, float2 texCoord, int2 offset = 0)
{
    return t.Sample(s, texCoord, offset);
}

float4 Sample(Texture2DArray<float4> t, SamplerState s, float3 texCoord, int2 offset = 0)
{
    return t.Sample(s, texCoord, offset);
}

float4 Sample(Texture3D<float4> t, SamplerState s, float3 texCoord, int3 offset = 0)
{
    return t.Sample(s, texCoord, offset);
}

// no offset
float4 Sample(TextureCube<float4> t, SamplerState s, float3 texCoord)
{
    return t.Sample(s, texCoord);
}

float4 Sample(TextureCubeArray<float4> t, SamplerState s, float4 texCoord)
{
    return t.Sample(s, texCoord);
}


//----------

// Can use these inside vertex shader
float4 SampleLevel(Texture2D<float4> t, SamplerState s, float4 texCoord, int2 offset = 0)
{
    return t.SampleLevel(s, texCoord.xy, texCoord.w, offset);
}
float4 SampleLevel(Texture2DArray t, SamplerState s, float4 texCoord, int2 offset = 0)
{
    return t.SampleLevel(s, texCoord.xyz, texCoord.w, offset);
}

float4 SampleLevel(Texture3D t, SamplerState s, float4 texCoord, int3 offset = 0)
{
    return t.SampleLevel(s, texCoord.xyz, texCoord.w, offset);
}

// no offset support
float4 SampleLevel(TextureCube t, SamplerState s, float4 texCoord)
{
    return t.SampleLevel(s, texCoord.xyz, texCoord.w);
}

// this would need more args for level
//float4 SampleLevel(TextureCubeArray t, SamplerState s, float4 texCoord)
//{
//    return t.SampleLevel(s, texCoord.xyz, texCoord.w);
//}


//----------

float4 SampleBias(Texture2D<float4> t, SamplerState s, float4 texCoord)
{
    return t.SampleBias(s, texCoord.xy, texCoord.w);
}

float4 SampleBias(TextureCube<float4> t, SamplerState s, float4 texCoord)
{
    return t.SampleBias(s, texCoord.xyz, texCoord.w);
}

//----------

float4 SampleGrad(Texture2D<float4> t, SamplerState s, float2 texCoord, float2 gradx, float2 grady)
{
   return t.SampleGrad(s, texCoord.xy, gradx, grady);
}

//----------

// This has templated elements appended, so typedef doesn't work.
// HLSL doesn't distingush depth/color, but MSL does. These calls combine
// the comparison value in the z or w element.
#define Depth2D Texture2D
#define Depth2DArray Texture2DArray
#define DepthCube TextureCube


// can just use the default for Texture2D<float4>
//float4 Sample(Texture2D<float4> t, SamplerState s, float2 texCoord, int2 offset = 0)
//{
//    return t.Sample(s, texCoord.xy, offset);
//}

// For persp shadows, remember to divide z = z/w before calling, or w = z/w on cube
float SampleCmp(Texture2D<float4> t, SamplerComparisonState s, float4 texCoord, int2 offset = 0)
{
    return t.SampleCmp(s, texCoord.xy, texCoord.z, offset);
}

float4 GatherCmp(Texture2D<float4> t, SamplerComparisonState s, float4 texCoord, int2 offset = 0)
{
    return t.GatherCmp(s, texCoord.xy, texCoord.z, offset);
}

//----------

// Use these in VS.  Why doesn't bilinear work in VS?
// TextureLevel should work in VS, since lod is specific.


// can also use this stype
// uint2 pos_xy = uint2( 0, 10 );
// texelColor = tex0[ pos_xy ] ;


// TODO: these also take offsets
float4 Load(Texture2D<float4> t, int2 texCoord, int lod = 0, int2 offset = 0)
{
    return t.Load(texCoord, lod, offset);
}

float4 Load(Texture3D<float4> t, int3 texCoord, int lod = 0, int3 offset = 0)
{
    return t.Load(texCoord, lod, offset);
}

float4 Load(Texture2DArray<float4> t, int3 texCoord, int lod = 0, int2 offset = 0)
{
    return t.Load(texCoord, lod, offset);
}

// no support in HLSL
//float4 Load(TextureCube<float4> t, int3 texCoord)
//{
//    return t.Load(texCoord);
//}
//
//float4 Load(TextureCubeArray<float4> t, int4 texCoord)
//{
//    return t.Load(texCoord);
//}

// this doesn't use SamplerState, raw load, not sampleIndex not lod
float4 Load(Texture2DMS<float4> t, int2 texCoord, int sample, int2 offset = 0)
{
    return t.Load(texCoord, sample, offset);
}

//----------

// gather only works on mip0
float4 GatherRed(Texture2D<float4> t, SamplerState s, float2 texCoord, int2 offset=0)
{
    return t.GatherRed(s, texCoord, offset);
}

float4 GatherGreen(Texture2D<float4> t, SamplerState s, float2 texCoord, int2 offset=0)
{
    return t.GatherGreen(s, texCoord, offset);
}

float4 GatherBlue(Texture2D<float4> t, SamplerState s, float2 texCoord, int2 offset=0)
{
    return t.GatherBlue(s, texCoord, offset);
}

float4 GatherAlpha(Texture2D<float4> t, SamplerState s, float2 texCoord, int2 offset=0)
{
    return t.GatherAlpha(s, texCoord, offset);
}


#if USE_HALF

// Note: HLSL to SPIRV doesn't support half Texture types, so must cast from float4
// but MSL and HLSL to DXIL can use half Texture type.
half4 SampleH(Texture2D<float4> t, SamplerState s, float2 texCoord, int2 offset = 0)
{
    return (half4)t.Sample(s, texCoord, offset);
}

half4 SampleLevelH(Texture2D<float4> t, SamplerState s, float4 texCoordMip, int2 offset = 0)
{
    return (half4)t.SampleLevel(s, texCoordMip.xy, texCoordMip.w, offset);
}

// offset?
half4 SampleBiasH(Texture2D<half> t, SamplerState s, float4 texCoordBias)
{
    return (half4)t.SampleBias(s, texCoordBias.xy, texCoordBias.w);
}

#else

// use all float4
#define SampleH Sample
#define SampleLevelH SampleLevel
#define SampleBiasH SampleBias

#endif

// TODO: these should be types, but by leaving off type, they apply to all types.
int2 GetDimensions(Texture2D t)
{
    int2 size;
    t.GetDimensions(size.x, size.y);
    return size;
}

int3 GetDimensions(Texture3D t)
{
    int3 size;
    t.GetDimensions(size.x, size.y, size.z);
    return size;
}

int2 GetDimensions(TextureCube t)
{
    int2 size;
    t.GetDimensions(size.x, size.y); // sizexsize
    return size;
}

int3 GetDimensions(TextureCubeArray t)
{
    int3 size;
    t.GetDimensions(size.x, size.y, size.z); // sizexsize
    return size;
}

int3 GetDimensions(Texture2DArray t)
{
    int3 size;
    t.GetDimensions(size.x, size.y, size.z);
    return size;
}

int2 GetDimensions(Texture2DMS t)
{
    int2 size;
    t.GetDimensions(size.x, size.y);
    return size;
}


#endif // ShaderHLSL_h
    
