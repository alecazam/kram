// kram - Copyright 2020-2025 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#ifndef ShaderTypes_h
#define ShaderTypes_h

#ifndef __METAL_VERSION__
#import <Foundation/Foundation.h>
#else
#define SIMD_NAMESPACE simd
#import <simd/simd.h>
#include <metal_stdlib>
#endif

#ifdef __METAL_VERSION__
#define NS_ENUM(_type, _name) \
    enum _name : _type _name; \
    enum _name : _type
#endif

typedef NS_ENUM(int32_t, BufferIndex) {
    // mesh
    BufferIndexMeshPosition = 0,  // pos
    BufferIndexMeshUV0 = 1,       // uv
    BufferIndexMeshNormal = 2,    // normals
    BufferIndexMeshTangent = 3,   // normals

    BufferIndexUniforms = 16,
    BufferIndexUniformsLevel = 17,
    BufferIndexUniformsDebug = 18,

    // for compute
    BufferIndexUniformsCS = 16,
};

typedef NS_ENUM(int32_t, VertexAttribute) {
    VertexAttributePosition = 0,
    VertexAttributeTexcoord = 1,
    VertexAttributeNormal = 2,
    VertexAttributeTangent = 3,
};

typedef NS_ENUM(int32_t, TextureIndex) {
    TextureIndexColor = 0,
    TextureIndexNormal = 1,
    TextureIndexDiff = 2,

    TextureIndexSamples = 3,  // used for compute
};

typedef NS_ENUM(int32_t, SamplerIndex) {
    SamplerIndexColor = 0,
};

// keep in sync with enum TextureChannels
typedef NS_ENUM(int32_t, ShaderTextureChannels) {
    ShModeRGBA = 0,

    ShModeR001 = 1,
    ShMode0G01 = 2,
    ShMode00B1 = 3,

    // see grayscale channels
    //    ShModeRRR1 = 5,
    //    ShModeGGG1 = 6,
    //    ShModeBBB1 = 7,
    ShModeAAA1 = 8,
};

// keep in sync with enum DebugMode
typedef NS_ENUM(int32_t, ShaderDebugMode) {
    ShDebugModeNone = 0,
    ShDebugModeTransparent,  // alpha < 255
    ShDebugModeNonZero,      // any(rgba) > 0
    ShDebugModeColor,
    ShDebugModeGray,
    ShDebugModeHDR,

    ShDebugModePosX,
    ShDebugModePosY,
    ShDebugModeCircleXY,

    ShDebugModeCount
};

// keep in sync with enum ShapeChannel
typedef NS_ENUM(int32_t, ShaderShapeChannel) {
    ShShapeChannelNone = 0,

    ShShapeChannelDepth,

    ShShapeChannelUV0,

    ShShapeChannelFaceNormal,

    ShShapeChannelNormal,
    ShShapeChannelTangent,
    ShShapeChannelBitangent,

    ShShapeChannelMipLevel,

    // ShShapeChannelBumpNormal,
};

typedef NS_ENUM(int32_t, ShaderLightingMode) {
    ShLightingModeDiffuse = 0,
    ShLightingModeSpecular = 1,
    ShLightingModeNone = 2,
};

// TODO: placement of these elements in the struct breaks transfer
// of data. This seems to work.  Alignment issues with mixing these differently.
struct Uniforms {
    SIMD_NAMESPACE::float4x4 projectionViewMatrix;
    SIMD_NAMESPACE::float4x4 modelMatrix;
    //SIMD_NAMESPACE::float4 modelMatrixInvScale2;  // to supply inverse, w is determinant
    SIMD_NAMESPACE::float3 cameraPosition;        // world-space
    float uvPreview;
    float uvToShapeRatio;
    
    bool isSigned;
    bool isNormal;
    bool isSwizzleAGToRG;
    bool doShaderPremul;

    bool isCheckerboardShown;
    bool isWrap;
    bool isSDF;
    bool isPreview; // render w/lighting, normals, etc
    bool isUVPreview; // show uv overlay
    bool isSrgbInput;
    bool isDiff;
    
    bool is3DView;
    bool isNormalMapPreview;  // for isNormal or combined

    bool isNormalMapSigned;
    bool isNormalMapSwizzleAGToRG;

    // this is used on wrap-around objects to avoid black transparent using
    // clampToZero
    bool isInsetByHalfPixel;

    // this means pull tangent from vertex
    bool useTangent;

    uint32_t numChannels;

    // control the pixel grid dimensions, can be block size, or pixel size
    uint32_t gridX;
    uint32_t gridY;

    // View pixels that meet criteria of the debugMode
    ShaderDebugMode debugMode;

    // View various aspects of shape geometry (depth, normal, tangent, ...)
    ShaderShapeChannel shapeChannel;

    // View the r,g,b,a channels of the texture
    ShaderTextureChannels channels;  // mask

    // Can turn on/off specular
    ShaderLightingMode lightingMode;
};

enum PassNumber
{
    kPassDefault = 0,
    kPassUVPreview = 1
};

// uploaded separately, so multiple mips, faces, array can be drawn to the
// screen at one time although modelMatrix offset changes.  Could store offset
// in here.
struct UniformsLevel {
    uint32_t mipLOD;
    uint32_t face;
    uint32_t arrayOrSlice;
    SIMD_NAMESPACE::float2 drawOffset;   // pixel offset to apply
    SIMD_NAMESPACE::float4 textureSize;  // width, height, 1/width, 1/height
    uint32_t passNumber; // switch to enum
};

// This is all tied to a single level sample
struct UniformsCS {
    SIMD_NAMESPACE::int2 uv;

    uint32_t arrayOrSlice;
    uint32_t face;
    uint32_t mipLOD;
};

struct UniformsDebug {
    SIMD_NAMESPACE::float4 rect;
};
    
#endif
