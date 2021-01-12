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

typedef NS_ENUM(int32_t, TextureChannels)
{
    ModeRGBA = 0,
    
    ModeR001 = 1,
    Mode0G01 = 2,
    Mode00B1 = 3,
    
    // see grayscale channels
    ModeRRR1 = 5,
    ModeGGG1 = 6,
    ModeBBB1 = 7,
    ModeAAA1 = 8,
};

typedef NS_ENUM(int32_t, DebugMode)
{
    DebugModeNone = 0,
    DebugModeTransparent = 1,
    DebugModeColor = 2,
    DebugModeGray = 3,
    DebugModeHDR = 4,
    
    DebugModePosX = 5,
    DebugModePosY = 6,
    
    DebugModeCount
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
    uint32_t debugMode;
    
    TextureChannels channels; // mask
};

struct UniformsCS
{
    simd::uint2 uv;
    uint32_t arrayOrSlice;
    uint32_t face;
    uint32_t mipLOD;
};

#endif 

