// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#ifndef ShaderTypes_h
#define ShaderTypes_h

#if __has_feature(modules)
@import Foundation;
@import simd;
@import Metal
#else
#ifndef __METAL_VERSION__
#import <Foundation/Foundation.h>
#else
#include <metal_stdlib>
#endif
#import <simd/simd.h>
#endif

#ifdef __METAL_VERSION__
    #define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
#endif


typedef NS_ENUM(int32_t, BufferIndex)
{
    BufferIndexMeshPositions = 0,
    BufferIndexMeshGenerics  = 1,
    BufferIndexUniforms      = 2,
    
    // for compute
    BufferIndexUniformsCS = 0,
};

typedef NS_ENUM(int32_t, VertexAttribute)
{
    VertexAttributePosition  = 0,
    VertexAttributeTexcoord  = 1,
};

typedef NS_ENUM(int32_t, TextureIndex)
{
    TextureIndexColor = 0,
    TextureIndexSamples = 1, // used for compute
};

typedef NS_ENUM(int32_t, SamplerIndex)
{
    SamplerIndexColor = 0,
};

// keep in sync with enum TextureChannels
typedef NS_ENUM(int32_t, ShaderTextureChannels)
{
    ShModeRGBA = 0,
    
    ShModeR001 = 1,
    ShMode0G01 = 2,
    ShMode00B1 = 3,
    
    // see grayscale channels
    ShModeRRR1 = 5,
    ShModeGGG1 = 6,
    ShModeBBB1 = 7,
    ShModeAAA1 = 8,
};

// keep in sync with enum DebugMode
typedef NS_ENUM(int32_t, ShaderDebugMode)
{
    ShDebugModeNone = 0,
    ShDebugModeTransparent = 1,
    ShDebugModeColor = 2,
    ShDebugModeGray = 3,
    ShDebugModeHDR = 4,
    
    ShDebugModePosX = 5,
    ShDebugModePosY = 6,
    
    ShDebugModeCount
};

// TODO: placement of these elements in the struct breaks transfer
// of data. This seems to work.  Alignment issues with mixing these differently.
struct Uniforms
{
    simd::float4x4 projectionViewMatrix;
    simd::float4x4 modelMatrix;

    bool isSigned;
    bool isNormal;
    bool isSwizzleAGToRG;
    bool isPremul;

    bool isCheckerboardShown;
    bool isWrap;
    bool isSDF;
    bool isPreview;
    
    uint32_t numChannels;
    uint32_t mipLOD;
    
    uint32_t face;
    uint32_t arrayOrSlice;
    
    // control the pixel grid dimensions, can be block size, or pixel size
    uint32_t gridX;
    uint32_t gridY;
    
    // can look at pixels that meet criteria of the debugMode
    ShaderDebugMode debugMode;
    
    ShaderTextureChannels channels; // mask
};

struct UniformsCS
{
    simd::uint2 uv;
    uint32_t arrayOrSlice;
    uint32_t face;
    uint32_t mipLOD;
};

#endif 

