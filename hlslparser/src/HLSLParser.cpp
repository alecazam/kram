//=============================================================================
//
// Render/HLSLParser.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#include "HLSLParser.h"

#include "Engine.h"

#include "HLSLTree.h"

#include <ctype.h>
#include <string.h>

// stl
#include <algorithm>
#include <vector>
#include <unordered_map>

namespace M4
{

enum CompareFunctionsResult
{
    FunctionsEqual,
    Function1Better,
    Function2Better
};

enum CoreType
{
    CoreType_None,
    
    CoreType_Scalar,
    CoreType_Vector,
    CoreType_Matrix,
    
    CoreType_Sampler,
    CoreType_Texture,
    CoreType_Struct,
    CoreType_Void,
    CoreType_Expression,
    CoreType_Comment,
    CoreType_Buffer,
    
    CoreType_Count // must be last
};

enum DimensionType
{
    DimensionType_None,

    DimensionType_Scalar,

    DimensionType_Vector2,
    DimensionType_Vector3,
    DimensionType_Vector4,

    DimensionType_Matrix2x2,
    DimensionType_Matrix3x3,
    DimensionType_Matrix4x4,
    
    //DimensionType_Matrix4x3, // TODO: no 3x4
    //DimensionType_Matrix4x2
};

// Can use this to break apart type to useful constructs
struct BaseTypeDescription
{
    const char*     typeName = "";
    const char*     typeNameMetal = "";
    
    HLSLBaseType    baseType = HLSLBaseType_Unknown;
    CoreType        coreType = CoreType_None;
    DimensionType   dimensionType = DimensionType_None;
    NumericType     numericType = NumericType_NaN;
    
    // TODO: is this useful ?
   // int             numDimensions; // scalar = 0, vector = 1, matrix = 2
    uint8_t             numDimensions = 0;
    uint8_t             numComponents = 0;
    uint8_t             height = 0;
    
    int8_t              binaryOpRank = -1; // or was this supposed to be max (-1 in uint8_t)
};

// really const
extern BaseTypeDescription baseTypeDescriptions[HLSLBaseType_Count];

bool IsSamplerType(HLSLBaseType baseType)
{
    return baseTypeDescriptions[baseType].coreType == CoreType_Sampler;
}

bool IsMatrixType(HLSLBaseType baseType)
{
    return baseTypeDescriptions[baseType].coreType == CoreType_Matrix;
}

bool IsVectorType(HLSLBaseType baseType)
{
    return baseTypeDescriptions[baseType].coreType == CoreType_Vector;
}

bool IsScalarType(HLSLBaseType baseType)
{
    return baseTypeDescriptions[baseType].coreType == CoreType_Scalar;
}

bool IsTextureType(HLSLBaseType baseType)
{
    return baseTypeDescriptions[baseType].coreType == CoreType_Texture;
}

bool IsDepthTextureType(HLSLBaseType baseType)
{
    // return baseTypeDescriptions[baseType].coreType == CoreType_DepthTexture;
    return  baseType == HLSLBaseType_Depth2D ||
            baseType == HLSLBaseType_Depth2DArray ||
            baseType == HLSLBaseType_DepthCube;
}


bool IsBufferType(HLSLBaseType baseType)
{
    return baseTypeDescriptions[baseType].coreType == CoreType_Buffer;
}


bool IsCoreTypeEqual(HLSLBaseType lhsType, HLSLBaseType rhsType)
{
    return baseTypeDescriptions[lhsType].coreType ==
           baseTypeDescriptions[rhsType].coreType;
}

bool IsDimensionEqual(HLSLBaseType lhsType, HLSLBaseType rhsType)
{
    return baseTypeDescriptions[lhsType].numComponents ==
           baseTypeDescriptions[rhsType].numComponents &&
           baseTypeDescriptions[lhsType].height ==
           baseTypeDescriptions[rhsType].height;
}

bool IsCrossDimensionEqual(HLSLBaseType lhsType, HLSLBaseType rhsType)
{
    return baseTypeDescriptions[lhsType].height ==
           baseTypeDescriptions[rhsType].numComponents;
}


bool IsNumericTypeEqual(HLSLBaseType lhsType, HLSLBaseType rhsType)
{
    return baseTypeDescriptions[lhsType].numericType ==
           baseTypeDescriptions[rhsType].numericType;
}

// TODO: with so many types, should just request the numeric type
bool IsHalf(HLSLBaseType type)
{
    return baseTypeDescriptions[type].numericType == NumericType_Half;
}

bool IsFloat(HLSLBaseType type)
{
    return baseTypeDescriptions[type].numericType == NumericType_Float;
}

bool IsDouble(HLSLBaseType type)
{
    return baseTypeDescriptions[type].numericType == NumericType_Double;
}

// TODO: IsUint, IsInt, ... or just get type.


bool IsSamplerType(const HLSLType & type)
{
    return IsSamplerType(type.baseType);
}

bool IsScalarType(const HLSLType & type)
{
    return IsScalarType(type.baseType);
}

bool IsVectorType(const HLSLType & type)
{
    return IsVectorType(type.baseType);
}

bool IsMatrixType(const HLSLType & type)
{
    return IsMatrixType(type.baseType);
}

bool IsTextureType(const HLSLType & type)
{
    return IsTextureType(type.baseType);
}

bool IsNumericType(HLSLBaseType baseType)
{
    return IsVectorType(baseType) || IsScalarType(baseType) || IsMatrixType(baseType);
}

HLSLBufferType ConvertTokenToBufferType(HLSLToken token)
{
    HLSLBufferType type = HLSLBufferType_CBuffer;
    
    switch(token)
    {
        // DX9
        case HLSLToken_CBuffer:
            type = HLSLBufferType_CBuffer; break;
        case HLSLToken_TBuffer:
            type = HLSLBufferType_TBuffer; break;
        
        // DX10
        case HLSLToken_ConstantBuffer:
            type = HLSLBufferType_ConstantBuffer; break;
        case HLSLToken_StructuredBuffer:
            type = HLSLBufferType_StructuredBuffer; break;
        case HLSLToken_RWStructuredBuffer:
            type = HLSLBufferType_RWStructuredBuffer; break;
        case HLSLToken_ByteAddressBuffer:
            type = HLSLBufferType_ByteAddressBuffer; break;
        case HLSLToken_RWByteAddressBuffer:
            type = HLSLBufferType_RWByteAddressBuffer; break;
            
        default:
            break;
    }
    
    return type;
}

HLSLBaseType NumericToBaseType(NumericType numericType)
{
    HLSLBaseType baseType = HLSLBaseType_Unknown;
    switch(numericType)
    {
        case NumericType_Float: baseType = HLSLBaseType_Float; break;
        case NumericType_Half: baseType = HLSLBaseType_Half; break;
        case NumericType_Double: baseType = HLSLBaseType_Bool; break;
       
        case NumericType_Int: baseType = HLSLBaseType_Int; break;
        case NumericType_Uint: baseType = HLSLBaseType_Uint; break;
        case NumericType_Ushort: baseType = HLSLBaseType_Ushort; break;
        case NumericType_Short: baseType = HLSLBaseType_Short; break;
        case NumericType_Ulong: baseType = HLSLBaseType_Ulong; break;
        case NumericType_Long: baseType = HLSLBaseType_Long; break;
        case NumericType_Bool: baseType = HLSLBaseType_Bool; break;
        
        // MSL has 8-bit, but HLSL/Vulkan don't
        //case NumericType_Uint8: baseType = HLSLBaseType_Uint8; break;
        //case NumericType_Int8: baseType = HLSLBaseType_Int8; break;
    
        default:
            break;
    }
    return baseType;
}

HLSLBaseType HalfToFloatBaseType(HLSLBaseType type)
{
    if (IsHalf(type))
        type = (HLSLBaseType)(HLSLBaseType_Float + (type - HLSLBaseType_Half));
    return type;
}

HLSLBaseType DoubleToFloatBaseType(HLSLBaseType type)
{
    if (IsDouble(type))
        type = (HLSLBaseType)(HLSLBaseType_Float + (type - HLSLBaseType_Double));
    return type;
}


static HLSLBaseType ArithmeticOpResultType(HLSLBinaryOp binaryOp, HLSLBaseType t1, HLSLBaseType t2);

const char* GetNumericTypeName(HLSLBaseType type)
{
    if (!IsNumericType(type))
        return nullptr;
    
    // MSL/HLSL share the same type names
    const auto& b = baseTypeDescriptions[type];
    return b.typeName;
}

HLSLBaseType PromoteType(HLSLBaseType toType, HLSLBaseType type)
{
    return HLSLBaseType(NumericToBaseType(baseTypeDescriptions[type].numericType) +
                        baseTypeDescriptions[type].dimensionType - DimensionType_Scalar);
}



/** This structure stores a HLSLFunction-like declaration for an intrinsic function */
struct Intrinsic
{
    explicit Intrinsic(const char* name, uint32_t numArgs)
    {
        function.name         = name;
        function.numArguments = numArgs;
        
        if (numArgs == 0) return;
        
        for (uint32_t i = 0; i < numArgs; ++i)
        {
            argument[i].type.flags = HLSLTypeFlag_Const;
        }
    }
    
    void ChainArgumentPointers()
    {
        function.argument = argument + 0;
        
        uint32_t numArgs = function.numArguments;
        // This chain pf pointers won't surive copy
        for (uint32_t i = 0; i < numArgs; ++i)
        {
            if (i < numArgs - 1)
                argument[i].nextArgument = argument + i + 1;
        }
    }
    
    void SetArgumentTypes(HLSLBaseType returnType, HLSLBaseType args[4])
    {
        function.returnType.baseType = returnType;
        for (uint32_t i = 0; i < function.numArguments; ++i)
        {
            ASSERT(args[i] != HLSLBaseType_Unknown);
            argument[i].type.baseType = args[i];
        }
    }
    
    void ArgsToArray(HLSLBaseType args[4], uint32_t& numArgs, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3, HLSLBaseType arg4)
    {
        numArgs = 0;
        if (arg1 == HLSLBaseType_Unknown) return;
        args[numArgs++] = arg1;
        if (arg2 == HLSLBaseType_Unknown) return;
        args[numArgs++] = arg2;
        if (arg3 == HLSLBaseType_Unknown) return;
        args[numArgs++] = arg3;
        if (arg4 == HLSLBaseType_Unknown) return;
        args[numArgs++] = arg4;
    }
    
    explicit Intrinsic(const char* name, HLSLBaseType returnType, HLSLBaseType arg1 = HLSLBaseType_Unknown, HLSLBaseType arg2 = HLSLBaseType_Unknown, HLSLBaseType arg3 = HLSLBaseType_Unknown, HLSLBaseType arg4 = HLSLBaseType_Unknown)
    {
        function.name                   = name;
        
        HLSLBaseType argumentTypes[4];
        uint32_t numArgs = 0;
        ArgsToArray(argumentTypes, numArgs, arg1, arg2, arg3, arg4);
        
        *this = Intrinsic(name, numArgs);
        SetArgumentTypes(returnType, argumentTypes);
    }
    
    // TODO: allow member function intrinsices on buffers/textures
    HLSLFunction    function;
    HLSLArgument    argument[4];
};
    
// So many calls are member functions in modern HLSL/MSL.
// This means the parser has to work harder to write out these intrinsics
// since some have default args, and some need level(), bias() wrappers in MSL.
// That complexity is currently hidden away in wrapper C-style calls in ShaderMSL.h.
#define USE_MEMBER_FUNCTIONS 0

static void AddIntrinsic(const Intrinsic& intrinsic);

void AddTextureIntrinsic(const char* name, HLSLBaseType returnType, HLSLBaseType textureType, HLSLBaseType textureHalfOrFloat, HLSLBaseType uvType)
{
#if USE_MEMBER_FUNCTIONS
    Intrinsic i(name, returnType, HLSLBaseType_SamplerState, uvType);
    i.function.memberType = textureType;
#else
    Intrinsic i(name, returnType, textureType, HLSLBaseType_SamplerState, uvType);
#endif
    i.argument[0].type.formatType = textureHalfOrFloat;

    AddIntrinsic(i);
}

// DepthCmp takes additional arg for comparison value, but this rolls it into uv
void AddDepthIntrinsic(const char* name, HLSLBaseType returnType, HLSLBaseType textureType, HLSLBaseType textureHalfOrFloat, HLSLBaseType uvType)
{
    // ComparisonState is only for SampleCmp/GatherCmp
    bool isCompare = String_Equal(name, "GatherCmp") || String_Equal(name, "SampleCmp");
    HLSLBaseType samplerType = isCompare ? HLSLBaseType_SamplerComparisonState : HLSLBaseType_SamplerState;
    
#if USE_MEMBER_FUNCTIONS
    Intrinsic i(name, returnType, samplerType, uvType); 
    i.function.memberType = textureType;
#else
    Intrinsic i(name, returnType, textureType, samplerType, uvType);
#endif
    
    i.argument[0].type.formatType = textureHalfOrFloat;
    AddIntrinsic(i);
}

static const int _numberTypeRank[NumericType_Count][NumericType_Count] = 
{
    // across is what type list on right is converted into (5 means don't, 0 means best)
    //F  H  D  B  I  UI  S US  L UL
    { 0, 3, 3, 4, 4, 4,  4, 4, 4, 4 },  // NumericType_Float
    { 2, 0, 4, 4, 4, 4,  4, 4, 4, 4 },  // NumericType_Half
    { 1, 4, 0, 4, 4, 4,  4, 4, 4, 4 },  // NumericType_Double
   
    { 5, 5, 5, 0, 5, 5,  5, 5, 5, 5 },  // NumericType_Bool
    { 5, 5, 5, 4, 0, 3,  4, 3, 5, 5 },  // NumericType_Int
    { 5, 5, 5, 4, 2, 0,  3, 4, 5, 5 },  // NumericType_Uint
    { 5, 5, 5, 4, 0, 3,  0, 5, 5, 5 },  // NumericType_Short
    { 5, 5, 5, 4, 2, 0,  5, 0, 5, 5 },  // NumericType_Ushort
    
    { 5, 5, 5, 4, 0, 3,  5, 5, 0, 5 },  // NumericType_Long
    { 5, 5, 5, 4, 2, 0,  5, 5, 5, 0 },  // NumericType_Ulong
};

/* All FX state
struct EffectStateValue
{
    const char * name;
    int value;
};

static const EffectStateValue textureFilteringValues[] = {
    {"None", 0},
    {"Point", 1},
    {"Linear", 2},
    {"Anisotropic", 3},
    {NULL, 0}
};

static const EffectStateValue textureAddressingValues[] = {
    {"Wrap", 1},
    {"Mirror", 2},
    {"Clamp", 3},
    {"Border", 4},
    {"MirrorOnce", 5},
    {NULL, 0}
};

static const EffectStateValue booleanValues[] = {
    {"False", 0},
    {"True", 1},
    {NULL, 0}
};

static const EffectStateValue cullValues[] = {
    {"None", 1},
    {"CW", 2},
    {"CCW", 3},
    {NULL, 0}
};

static const EffectStateValue cmpValues[] = {
    {"Never", 1},
    {"Less", 2},
    {"Equal", 3},
    {"LessEqual", 4},
    {"Greater", 5},
    {"NotEqual", 6},
    {"GreaterEqual", 7},
    {"Always", 8},
    {NULL, 0}
};

static const EffectStateValue blendValues[] = {
    {"Zero", 1},
    {"One", 2},
    {"SrcColor", 3},
    {"InvSrcColor", 4},
    {"SrcAlpha", 5},
    {"InvSrcAlpha", 6},
    {"DestAlpha", 7},
    {"InvDestAlpha", 8},
    {"DestColor", 9},
    {"InvDestColor", 10},
    {"SrcAlphaSat", 11},
    {"BothSrcAlpha", 12},
    {"BothInvSrcAlpha", 13},
    {"BlendFactor", 14},
    {"InvBlendFactor", 15},
    {"SrcColor2", 16},          // Dual source blending. D3D9Ex only.
    {"InvSrcColor2", 17},
    {NULL, 0}
};

static const EffectStateValue blendOpValues[] = {
    {"Add", 1},
    {"Subtract", 2},
    {"RevSubtract", 3},
    {"Min", 4},
    {"Max", 5},
    {NULL, 0}
};

static const EffectStateValue fillModeValues[] = {
    {"Point", 1},
    {"Wireframe", 2},
    {"Solid", 3},
    {NULL, 0}
};

static const EffectStateValue stencilOpValues[] = {
    {"Keep", 1},
    {"Zero", 2},
    {"Replace", 3},
    {"IncrSat", 4},
    {"DecrSat", 5},
    {"Invert", 6},
    {"Incr", 7},
    {"Decr", 8},
    {NULL, 0}
};

// These are flags.
static const EffectStateValue colorMaskValues[] = {
    {"False", 0},
    {"Red",   1<<0},
    {"Green", 1<<1},
    {"Blue",  1<<2},
    {"Alpha", 1<<3},
    {"X", 1<<0},
    {"Y", 1<<1},
    {"Z", 1<<2},
    {"W", 1<<3},
    {NULL, 0}
};

static const EffectStateValue integerValues[] = {
    {NULL, 0}
};

static const EffectStateValue floatValues[] = {
    {NULL, 0}
};


struct EffectState
{
    const char * name;
    int d3drs;
    const EffectStateValue * values;
};

static const EffectState samplerStates[] = {
    {"AddressU", 1, textureAddressingValues},
    {"AddressV", 2, textureAddressingValues},
    {"AddressW", 3, textureAddressingValues},
    // limited choices for bordercolor on mobile, so assume transparent
    // "BorderColor", 4, D3DCOLOR
    {"MagFilter", 5, textureFilteringValues},
    {"MinFilter", 6, textureFilteringValues},
    {"MipFilter", 7, textureFilteringValues},
    {"MipMapLodBias", 8, floatValues},
    // TODO: also MinMipLevel
    {"MaxMipLevel", 9, integerValues},
    {"MaxAnisotropy", 10, integerValues},
    // Format conveys this now {"sRGBTexture", 11, booleanValues},
};

// can set these states in an Effect block from FX files
static const EffectState effectStates[] = {
    {"VertexShader", 0, NULL},
    {"PixelShader", 0, NULL},
    {"AlphaBlendEnable", 27, booleanValues},
    {"SrcBlend", 19, blendValues},
    {"DestBlend", 20, blendValues},
    {"BlendOp", 171, blendOpValues},
    {"SeparateAlphaBlendEanble", 206, booleanValues},
    {"SrcBlendAlpha", 207, blendValues},
    {"DestBlendAlpha", 208, blendValues},
    {"BlendOpAlpha", 209, blendOpValues},
    {"AlphaTestEnable", 15, booleanValues},
    {"AlphaRef", 24, integerValues},
    {"AlphaFunc", 25, cmpValues},
    {"CullMode", 22, cullValues},
    {"ZEnable", 7, booleanValues},
    {"ZWriteEnable", 14, booleanValues},
    {"ZFunc", 23, cmpValues},
    {"StencilEnable", 52, booleanValues},
    {"StencilFail", 53, stencilOpValues},
    {"StencilZFail", 54, stencilOpValues},
    {"StencilPass", 55, stencilOpValues},
    {"StencilFunc", 56, cmpValues},
    {"StencilRef", 57, integerValues},
    {"StencilMask", 58, integerValues},
    {"StencilWriteMask", 59, integerValues},
    {"TwoSidedStencilMode", 185, booleanValues},
    {"CCW_StencilFail", 186, stencilOpValues},
    {"CCW_StencilZFail", 187, stencilOpValues},
    {"CCW_StencilPass", 188, stencilOpValues},
    {"CCW_StencilFunc", 189, cmpValues},
    {"ColorWriteEnable", 168, colorMaskValues},
    {"FillMode", 8, fillModeValues},
    {"MultisampleAlias", 161, booleanValues},
    {"MultisampleMask", 162, integerValues},
    {"ScissorTestEnable", 174, booleanValues},
    {"SlopeScaleDepthBias", 175, floatValues},
    {"DepthBias", 195, floatValues}
};


static const EffectStateValue witnessCullModeValues[] = {
    {"None", 0},
    {"Back", 1},
    {"Front", 2},
    {NULL, 0}
};

static const EffectStateValue witnessFillModeValues[] = {
    {"Solid", 0},
    {"Wireframe", 1},
    {NULL, 0}
};

static const EffectStateValue witnessBlendModeValues[] = {
    {"Disabled", 0},
    {"AlphaBlend", 1},          // src * a + dst * (1-a)
    {"Add", 2},                 // src + dst
    {"Mixed", 3},               // src + dst * (1-a)
    {"Multiply", 4},            // src * dst
    {"Multiply2", 5},           // 2 * src * dst
    {NULL, 0}
};

static const EffectStateValue witnessDepthFuncValues[] = {
    {"LessEqual", 0},
    {"Less", 1},
    {"Equal", 2},
    {"Greater", 3},
    {"Always", 4},
    {NULL, 0}
};

static const EffectStateValue witnessStencilModeValues[] = {
    {"Disabled", 0},
    {"Set", 1},
    {"Test", 2},
    {NULL, 0}
};

static const EffectState pipelineStates[] = {
    {"VertexShader", 0, NULL},
    {"PixelShader", 0, NULL},

    // Depth_Stencil_State
    {"DepthWrite", 0, booleanValues},
    {"DepthEnable", 0, booleanValues},
    {"DepthFunc", 0, witnessDepthFuncValues},
    {"StencilMode", 0, witnessStencilModeValues},

    // Raster_State
    {"CullMode", 0, witnessCullModeValues},
    {"FillMode", 0, witnessFillModeValues},
    {"MultisampleEnable", 0, booleanValues},
    {"PolygonOffset", 0, booleanValues},

    // Blend_State
    {"BlendMode", 0, witnessBlendModeValues},
    {"ColorWrite", 0, booleanValues},
    {"AlphaWrite", 0, booleanValues},
    {"AlphaTest", 0, booleanValues},       // This is really alpha to coverage.
};
*/

/*
#define INTRINSIC_FLOAT1_FUNCTION(name) \
        Intrinsic( name, HLSLBaseType_Float,   HLSLBaseType_Float  ),   \
        Intrinsic( name, HLSLBaseType_Float2,  HLSLBaseType_Float2 ),   \
        Intrinsic( name, HLSLBaseType_Float3,  HLSLBaseType_Float3 ),   \
        Intrinsic( name, HLSLBaseType_Float4,  HLSLBaseType_Float4 ),   \
        Intrinsic( name, HLSLBaseType_Half,    HLSLBaseType_Half   ),   \
        Intrinsic( name, HLSLBaseType_Half2,   HLSLBaseType_Half2  ),   \
        Intrinsic( name, HLSLBaseType_Half3,   HLSLBaseType_Half3  ),   \
        Intrinsic( name, HLSLBaseType_Half4,   HLSLBaseType_Half4  )

#define INTRINSIC_FLOAT2_FUNCTION(name) \
        Intrinsic( name, HLSLBaseType_Float,   HLSLBaseType_Float,   HLSLBaseType_Float  ),   \
        Intrinsic( name, HLSLBaseType_Float2,  HLSLBaseType_Float2,  HLSLBaseType_Float2 ),   \
        Intrinsic( name, HLSLBaseType_Float3,  HLSLBaseType_Float3,  HLSLBaseType_Float3 ),   \
        Intrinsic( name, HLSLBaseType_Float4,  HLSLBaseType_Float4,  HLSLBaseType_Float4 ),   \
        Intrinsic( name, HLSLBaseType_Half,    HLSLBaseType_Half,    HLSLBaseType_Half   ),   \
        Intrinsic( name, HLSLBaseType_Half2,   HLSLBaseType_Half2,   HLSLBaseType_Half2  ),   \
        Intrinsic( name, HLSLBaseType_Half3,   HLSLBaseType_Half3,   HLSLBaseType_Half3  ),   \
        Intrinsic( name, HLSLBaseType_Half4,   HLSLBaseType_Half4,   HLSLBaseType_Half4  )

#define INTRINSIC_FLOAT3_FUNCTION(name) \
        Intrinsic( name, HLSLBaseType_Float,   HLSLBaseType_Float,   HLSLBaseType_Float,  HLSLBaseType_Float ),   \
        Intrinsic( name, HLSLBaseType_Float2,  HLSLBaseType_Float2,  HLSLBaseType_Float2,  HLSLBaseType_Float2 ),  \
        Intrinsic( name, HLSLBaseType_Float3,  HLSLBaseType_Float3,  HLSLBaseType_Float3,  HLSLBaseType_Float3 ),  \
        Intrinsic( name, HLSLBaseType_Float4,  HLSLBaseType_Float4,  HLSLBaseType_Float4,  HLSLBaseType_Float4 ),  \
        Intrinsic( name, HLSLBaseType_Half,    HLSLBaseType_Half,    HLSLBaseType_Half,   HLSLBaseType_Half ),    \
        Intrinsic( name, HLSLBaseType_Half2,   HLSLBaseType_Half2,   HLSLBaseType_Half2,  HLSLBaseType_Half2 ),    \
        Intrinsic( name, HLSLBaseType_Half3,   HLSLBaseType_Half3,   HLSLBaseType_Half3,  HLSLBaseType_Half3 ),    \
        Intrinsic( name, HLSLBaseType_Half4,   HLSLBaseType_Half4,   HLSLBaseType_Half4,  HLSLBaseType_Half4 )

*/

// TODO: elim the H version once have member functions, can check the member textuer format.
#define TEXTURE_INTRINSIC_FUNCTION(name, textureType, uvType) \
    AddTextureIntrinsic( name, HLSLBaseType_Float4, textureType, HLSLBaseType_Float, uvType)

#define TEXTURE_INTRINSIC_FUNCTION_H(name, textureType, uvType) \
        AddTextureIntrinsic( name "H", HLSLBaseType_Half4, textureType, HLSLBaseType_Half, uvType  )

// Note: these strings need to live until end of the app
StringPool gStringPool(NULL);

enum All
{
    AllHalf = (1<<0),
    AllFloat = (1<<1),
    AllDouble = (1<<2),
    
    AllFloats = AllHalf | AllFloat | AllDouble,
    
    AllUint = (1<<3),
    AllInt = (1<<4),
    AllShort = (1<<5),
    AllUshort = (1<<6),
    AllLong = (1<<7),
    AllUlong = (1<<8),
    AllBool = (1<<9),
    
    AllInts = AllUint | AllInt | AllShort | AllUshort | AllLong | AllUlong | AllBool,
    
    //AllScalar  = (1<<15),
    AllVecs = (1<<16),
    AllMats = (1<<17),
    AllDims = AllVecs | AllMats,
};
using AllMask = uint32_t;

// TODO: want to use Array, but it needs Allocator passed
struct Range
{
    uint32_t start;
    uint32_t count;
};
using IntrinsicRangeMap = std::unordered_map<const char*, Range, CompareAndHandStrings, CompareAndHandStrings>;

static std::vector<Intrinsic> _intrinsics;

// This will help with comparison to avoid O(n) search of all 5000 intrinsics
static IntrinsicRangeMap _intrinsicRangeMap;

static void AddIntrinsic(const Intrinsic& intrinsic)
{
    const char* name = intrinsic.function.name;
    
    // Put in string pool since using this as a key.  Also means equals just ptr compar.
   name = gStringPool.AddString(name);
    
    // track intrinsic range in a map, also the name lookup helps speed the parser up
    auto it = _intrinsicRangeMap.find(name);
    if (it != _intrinsicRangeMap.end())
    {
        it->second.count++;
    }
    else
    {
        _intrinsicRangeMap[name] = { (uint32_t)_intrinsics.size(), 1 };
    }
    
    // To avoid having growth destroy the argument chains
    if (_intrinsics.empty())
        _intrinsics.reserve(10000);
        
    _intrinsics.push_back(intrinsic);
    _intrinsics.back().function.name = name;
    
    // These pointers change when copied or when vector grows, so do a reserve
    _intrinsics.back().ChainArgumentPointers();
}

void AddIntrinsic(const char* name, HLSLBaseType returnType, HLSLBaseType arg1 = HLSLBaseType_Unknown, HLSLBaseType arg2 = HLSLBaseType_Unknown, HLSLBaseType arg3 = HLSLBaseType_Unknown, HLSLBaseType arg4 = HLSLBaseType_Unknown)
{
    Intrinsic intrinsic(name, returnType, arg1, arg2, arg3, arg4);
    AddIntrinsic(intrinsic);
}

void RegisterBaseTypeIntrinsic(Intrinsic& intrinsic, uint32_t numArgs, HLSLBaseType returnType, HLSLBaseType baseType, uint32_t start, uint32_t end)
{
    HLSLBaseType args[4] = {};
    
    for (uint32_t i = start; i < end; ++i)
    {
        HLSLBaseType baseTypeIter = (HLSLBaseType)(baseType + i);
        
        HLSLBaseType newReturnType = (returnType == HLSLBaseType_Unknown) ? baseTypeIter : returnType;
        
        for (uint32_t a = 0; a < numArgs; ++a)
            args[a] = baseTypeIter;
        
        intrinsic.SetArgumentTypes(newReturnType, args);
        AddIntrinsic(intrinsic);
    }
}

bool TestBits(AllMask mask, AllMask maskTest)
{
    return (mask & maskTest) == maskTest;
}

void RegisterIntrinsics(const char* name, uint32_t numArgs, AllMask mask, HLSLBaseType returnType = HLSLBaseType_Unknown)
{
    Intrinsic intrinsic(name, numArgs);

    {
        const uint32_t kNumTypes = 3;
        HLSLBaseType baseTypes[kNumTypes] = { HLSLBaseType_Float, HLSLBaseType_Half, HLSLBaseType_Double };
    
        bool skip[kNumTypes] = {};
        if (!TestBits(mask, AllFloat))
           skip[0] = true;
        if (!TestBits(mask, AllHalf))
           skip[1] = true;
        if (!TestBits(mask, AllDouble))
            skip[2] = true;
        
        for (uint32_t i = 0; i < kNumTypes; ++i)
        {
            if (skip[i]) continue;
            HLSLBaseType baseType = baseTypes[i];
            
            if (mask & AllVecs)
                RegisterBaseTypeIntrinsic(intrinsic, numArgs, returnType, baseType, 0, 4);
            if (mask & AllMats)
                RegisterBaseTypeIntrinsic(intrinsic, numArgs, returnType, baseType, 4, 7);
        }
    }

    if ((mask & AllInts) == AllInts)
    {
        const uint32_t kNumTypes = 7;
        HLSLBaseType baseTypes[kNumTypes] = {
            HLSLBaseType_Long, HLSLBaseType_Ulong,
            HLSLBaseType_Int,  HLSLBaseType_Uint,
            HLSLBaseType_Short, HLSLBaseType_Ushort,
            HLSLBaseType_Bool
        };
        
        bool skip[kNumTypes] = {};
        if (!TestBits(mask, AllLong))
            skip[0] = true;
        if (!TestBits(mask, AllUlong))
            skip[1] = true;
        if (!TestBits(mask, AllInt))
            skip[2] = true;
        if (!TestBits(mask, AllUint))
            skip[3] = true;
        if (!TestBits(mask, AllShort))
            skip[4] = true;
        if (!TestBits(mask, AllUshort))
            skip[5] = true;
        if (!TestBits(mask, AllBool))
            skip[6] = true;
        
        
        for (uint32_t i = 0; i < kNumTypes; ++i)
        {
            if (skip[i]) continue;
            HLSLBaseType baseType = baseTypes[i];
            
            if (mask & AllVecs)
                RegisterBaseTypeIntrinsic(intrinsic, numArgs, returnType, baseType, 0, 4);
            
            // TODO: No int matrices yet, but could add them
            //if (mask & AllMats)
            //    RegisterBaseTypeIntrinsic(intrinsic, numArgs, returnType, 4, 7);
        }
    }
}

#define ArrayCount(array) (sizeof(array) / sizeof(array[0]) )

bool InitIntrinsics()
{
    // TODO: these arrays shouldn't need to be static, but getting corrupt strings
    static const char* ops1[] = {
        "abs",
        "acos", "asin", "atan",
        "cos", "sin", "tan",
        "floor", "ceil", "frac", "fmod",
        "normalize", "saturate", "sqrt", "rcp", "exp", "exp2",
        "log", "Log2",
        "ddx", "ddy",
        "isNan", "isInf", "sign",
    };
    
    static const char* ops2[] = {
        "atan2", "pow", // can't pow take sclar?
        "step", "reflect",
        "min", "max",
    };
    
    static const char* ops3[] = {
        "clamp", "lerp", "smoothstep",
        "min3", "max3",
    };

    // MSL constructs, may be in HLSL
    // "distance"
    // "distance_squared"
    // "refract"
    // "deteriminant"
    // "median3" - takes 3 args
    // "select"
    
    for (uint32_t i = 0, iEnd = ArrayCount(ops1); i < iEnd; ++i)
    {
        RegisterIntrinsics( ops1[i], 1, AllFloats | AllVecs );
    }
    for (uint32_t i = 0, iEnd = ArrayCount(ops2); i < iEnd; ++i)
    {
        RegisterIntrinsics( ops2[i], 2, AllFloats | AllVecs );
    }
    for (uint32_t i = 0, iEnd = ArrayCount(ops3); i < iEnd; ++i)
    {
        RegisterIntrinsics( ops3[i], 3, AllFloats | AllVecs );
    }
    
    RegisterIntrinsics("sincos", 2, AllFloats | AllVecs, HLSLBaseType_Void);

    RegisterIntrinsics( "mad", 3, AllFloats | AllVecs);
   
    RegisterIntrinsics( "any", 1, AllFloats | AllInts | AllVecs, HLSLBaseType_Bool);
    RegisterIntrinsics( "all", 1, AllFloats | AllInts | AllVecs, HLSLBaseType_Bool);
    
    RegisterIntrinsics( "clip", 1, AllFloats | AllVecs, HLSLBaseType_Void);
    
    RegisterIntrinsics( "dot", 2, AllHalf | AllVecs, HLSLBaseType_Half);
    RegisterIntrinsics( "dot", 2, AllFloat | AllVecs, HLSLBaseType_Float);
    RegisterIntrinsics( "dot", 2, AllDouble | AllVecs, HLSLBaseType_Double);
  
    // 3d cross product only
    AddIntrinsic( "cross", HLSLBaseType_Float3,  HLSLBaseType_Float3,  HLSLBaseType_Float3 );
    AddIntrinsic( "cross", HLSLBaseType_Half3,   HLSLBaseType_Half3,   HLSLBaseType_Half3 );
    AddIntrinsic( "cross", HLSLBaseType_Double3, HLSLBaseType_Double3, HLSLBaseType_Double3 );

    RegisterIntrinsics( "length", 1, AllHalf | AllVecs, HLSLBaseType_Half);
    RegisterIntrinsics( "length", 1, AllFloat | AllVecs, HLSLBaseType_Float);
    RegisterIntrinsics( "length", 1, AllDouble | AllVecs, HLSLBaseType_Double);
  
    // MSL construct
    RegisterIntrinsics( "length_squared", 1, AllHalf | AllVecs, HLSLBaseType_Half);
    RegisterIntrinsics( "length_squared", 1, AllFloat | AllVecs, HLSLBaseType_Float);
    RegisterIntrinsics( "length_squared", 1, AllDouble | AllVecs, HLSLBaseType_Double);

    // scalar/vec ops
    RegisterIntrinsics( "mul", 2, AllFloat | AllVecs | AllMats );
    
    // scalar mul, since * isn't working on Metal properly
    // m = s * m
    AddIntrinsic( "mul", HLSLBaseType_Float2x2, HLSLBaseType_Float, HLSLBaseType_Float2x2 );
    AddIntrinsic( "mul", HLSLBaseType_Float3x3, HLSLBaseType_Float, HLSLBaseType_Float3x3 );
    AddIntrinsic( "mul", HLSLBaseType_Float4x4, HLSLBaseType_Float, HLSLBaseType_Float4x4 );
    AddIntrinsic( "mul", HLSLBaseType_Float2x2, HLSLBaseType_Float2x2, HLSLBaseType_Float );
    AddIntrinsic( "mul", HLSLBaseType_Float3x3, HLSLBaseType_Float3x3, HLSLBaseType_Float );
    AddIntrinsic( "mul", HLSLBaseType_Float4x4, HLSLBaseType_Float4x4, HLSLBaseType_Float );
    
    // v = v * m
    AddIntrinsic( "mul", HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2x2 );
    AddIntrinsic( "mul", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3x3 );
    AddIntrinsic( "mul", HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Float4x4 );
    AddIntrinsic( "mul", HLSLBaseType_Float2, HLSLBaseType_Float2x2, HLSLBaseType_Float2 );
    AddIntrinsic( "mul", HLSLBaseType_Float3, HLSLBaseType_Float3x3, HLSLBaseType_Float3 );
    AddIntrinsic( "mul", HLSLBaseType_Float4, HLSLBaseType_Float4x4, HLSLBaseType_Float4 );
    
    // m = s * m
    AddIntrinsic( "mul", HLSLBaseType_Half2x2, HLSLBaseType_Half, HLSLBaseType_Half2x2 );
    AddIntrinsic( "mul", HLSLBaseType_Half3x3, HLSLBaseType_Half, HLSLBaseType_Half3x3 );
    AddIntrinsic( "mul", HLSLBaseType_Half4x4, HLSLBaseType_Half, HLSLBaseType_Half4x4 );
    AddIntrinsic( "mul", HLSLBaseType_Half2x2, HLSLBaseType_Half2x2, HLSLBaseType_Half );
    AddIntrinsic( "mul", HLSLBaseType_Half3x3, HLSLBaseType_Half3x3, HLSLBaseType_Half );
    AddIntrinsic( "mul", HLSLBaseType_Half4x4, HLSLBaseType_Half4x4, HLSLBaseType_Half );
    
    // v = v * m
    AddIntrinsic( "mul", HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Half2x2 );
    AddIntrinsic( "mul", HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Half3x3 );
    AddIntrinsic( "mul", HLSLBaseType_Half4, HLSLBaseType_Half4, HLSLBaseType_Half4x4 );
    AddIntrinsic( "mul", HLSLBaseType_Half2, HLSLBaseType_Half2x2, HLSLBaseType_Half2 );
    AddIntrinsic( "mul", HLSLBaseType_Half3, HLSLBaseType_Half3x3, HLSLBaseType_Half3 );
    AddIntrinsic( "mul", HLSLBaseType_Half4, HLSLBaseType_Half4x4, HLSLBaseType_Half4 );
    
    // m = s * m
    AddIntrinsic( "mul", HLSLBaseType_Double2x2, HLSLBaseType_Double, HLSLBaseType_Double2x2 );
    AddIntrinsic( "mul", HLSLBaseType_Double3x3, HLSLBaseType_Double, HLSLBaseType_Double3x3 );
    AddIntrinsic( "mul", HLSLBaseType_Double4x4, HLSLBaseType_Double, HLSLBaseType_Double4x4 );
    AddIntrinsic( "mul", HLSLBaseType_Double2x2, HLSLBaseType_Double2x2, HLSLBaseType_Double );
    AddIntrinsic( "mul", HLSLBaseType_Double3x3, HLSLBaseType_Double3x3, HLSLBaseType_Double );
    AddIntrinsic( "mul", HLSLBaseType_Double4x4, HLSLBaseType_Double4x4, HLSLBaseType_Double );
    
    // v = v * m
    AddIntrinsic( "mul", HLSLBaseType_Double2, HLSLBaseType_Double2, HLSLBaseType_Double2x2 );
    AddIntrinsic( "mul", HLSLBaseType_Double3, HLSLBaseType_Double3, HLSLBaseType_Double3x3 );
    AddIntrinsic( "mul", HLSLBaseType_Double4, HLSLBaseType_Double4, HLSLBaseType_Double4x4 );
    AddIntrinsic( "mul", HLSLBaseType_Double2, HLSLBaseType_Double2x2, HLSLBaseType_Double2 );
    AddIntrinsic( "mul", HLSLBaseType_Double3, HLSLBaseType_Double3x3, HLSLBaseType_Double3 );
    AddIntrinsic( "mul", HLSLBaseType_Double4, HLSLBaseType_Double4x4, HLSLBaseType_Double4 );
    
    // matrix transpose
    RegisterIntrinsics("transpose", 2, AllFloats | AllMats);
    
    // TODO: more conversions fp16, double, etc.
    AddIntrinsic("asuint", HLSLBaseType_Uint, HLSLBaseType_Float);
    AddIntrinsic("asuint", HLSLBaseType_Uint, HLSLBaseType_Double);
    AddIntrinsic("asuint", HLSLBaseType_Uint, HLSLBaseType_Half);

    
    // TODO: split off sampler intrinsics from math above
    //------------------------
    
    TEXTURE_INTRINSIC_FUNCTION("Sample", HLSLBaseType_Texture2D,  HLSLBaseType_Float2);
    TEXTURE_INTRINSIC_FUNCTION("Sample", HLSLBaseType_Texture3D, HLSLBaseType_Float3);
    TEXTURE_INTRINSIC_FUNCTION("Sample", HLSLBaseType_Texture2DArray, HLSLBaseType_Float3);
    TEXTURE_INTRINSIC_FUNCTION("Sample", HLSLBaseType_TextureCube, HLSLBaseType_Float3);
    TEXTURE_INTRINSIC_FUNCTION("Sample", HLSLBaseType_TextureCubeArray, HLSLBaseType_Float4);
    
    // Depth
    AddDepthIntrinsic("Sample", HLSLBaseType_Float, HLSLBaseType_Depth2D, HLSLBaseType_Float,  HLSLBaseType_Float2);
    AddDepthIntrinsic("Sample", HLSLBaseType_Float, HLSLBaseType_Depth2DArray, HLSLBaseType_Float,  HLSLBaseType_Float3);
    AddDepthIntrinsic("Sample", HLSLBaseType_Float, HLSLBaseType_DepthCube, HLSLBaseType_Float,  HLSLBaseType_Float3);
    
    TEXTURE_INTRINSIC_FUNCTION_H("Sample", HLSLBaseType_Texture2D,  HLSLBaseType_Float2);
    TEXTURE_INTRINSIC_FUNCTION_H("Sample", HLSLBaseType_Texture3D, HLSLBaseType_Float3);
    TEXTURE_INTRINSIC_FUNCTION_H("Sample", HLSLBaseType_Texture2DArray, HLSLBaseType_Float3);
    TEXTURE_INTRINSIC_FUNCTION_H("Sample", HLSLBaseType_TextureCube, HLSLBaseType_Float3);
    TEXTURE_INTRINSIC_FUNCTION_H("Sample", HLSLBaseType_TextureCubeArray, HLSLBaseType_Float4);
    
    
    // xyz are used, this doesn't match HLSL which is 2 + compare
    AddDepthIntrinsic("SampleCmp", HLSLBaseType_Float, HLSLBaseType_Depth2D, HLSLBaseType_Float, HLSLBaseType_Float4);
    AddDepthIntrinsic("SampleCmp", HLSLBaseType_Float, HLSLBaseType_Depth2DArray, HLSLBaseType_Float, HLSLBaseType_Float4);
    AddDepthIntrinsic("SampleCmp", HLSLBaseType_Float, HLSLBaseType_DepthCube, HLSLBaseType_Float, HLSLBaseType_Float4);
    
    // returns float4 w/comparisons, probably only on mip0
    // TODO: add GatherRed? to read 4 depth values
    AddDepthIntrinsic("GatherCmp", HLSLBaseType_Float4, HLSLBaseType_Depth2D, HLSLBaseType_Float, HLSLBaseType_Float4);
    AddDepthIntrinsic("GatherCmp", HLSLBaseType_Float4, HLSLBaseType_Depth2DArray, HLSLBaseType_Float, HLSLBaseType_Float4);
    AddDepthIntrinsic("GatherCmp", HLSLBaseType_Float4, HLSLBaseType_DepthCube, HLSLBaseType_Float, HLSLBaseType_Float4);
    
    // one more dimension than Sample
    TEXTURE_INTRINSIC_FUNCTION("SampleLevel", HLSLBaseType_Texture2D, HLSLBaseType_Float3);
    TEXTURE_INTRINSIC_FUNCTION("SampleLevel", HLSLBaseType_Texture3D, HLSLBaseType_Float4);
    TEXTURE_INTRINSIC_FUNCTION("SampleLevel", HLSLBaseType_Texture2DArray, HLSLBaseType_Float4);
    TEXTURE_INTRINSIC_FUNCTION("SampleLevel", HLSLBaseType_TextureCube, HLSLBaseType_Float4);
    // TEXTURE_INTRINSIC_FUNCTION("SampleLevel", HLSLBaseType_TextureCubeArray, HLSLBaseType_Float4, Float);
    
    TEXTURE_INTRINSIC_FUNCTION_H("SampleLevel", HLSLBaseType_Texture2D, HLSLBaseType_Float3);
    TEXTURE_INTRINSIC_FUNCTION_H("SampleLevel", HLSLBaseType_Texture3D, HLSLBaseType_Float4);
    TEXTURE_INTRINSIC_FUNCTION_H("SampleLevel", HLSLBaseType_Texture2DArray, HLSLBaseType_Float4);
    TEXTURE_INTRINSIC_FUNCTION_H("SampleLevel", HLSLBaseType_TextureCube, HLSLBaseType_Float4);
   
    
    // bias always in w
    TEXTURE_INTRINSIC_FUNCTION("SampleBias", HLSLBaseType_Texture2D, HLSLBaseType_Float4);
    TEXTURE_INTRINSIC_FUNCTION("SampleBias", HLSLBaseType_Texture3D, HLSLBaseType_Float4);
    TEXTURE_INTRINSIC_FUNCTION("SampleBias", HLSLBaseType_Texture2DArray, HLSLBaseType_Float4);
    TEXTURE_INTRINSIC_FUNCTION("SampleBias", HLSLBaseType_TextureCube, HLSLBaseType_Float4);
    // TEXTURE_INTRINSIC_FUNCTION("SampleBias", HLSLBaseType_TextureCubeArray, HLSLBaseType_Float4, Float);
    
    TEXTURE_INTRINSIC_FUNCTION_H("SampleBias", HLSLBaseType_Texture2D, HLSLBaseType_Float4);
    TEXTURE_INTRINSIC_FUNCTION_H("SampleBias", HLSLBaseType_Texture3D, HLSLBaseType_Float4);
    TEXTURE_INTRINSIC_FUNCTION_H("SampleBias", HLSLBaseType_Texture2DArray, HLSLBaseType_Float4);
    TEXTURE_INTRINSIC_FUNCTION_H("SampleBias", HLSLBaseType_TextureCube, HLSLBaseType_Float4);
    
    
    //TEXTURE_INTRINSIC_FUNCTION("SampleGrad", HLSLBaseType_Texture3D, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2);
    //TEXTURE_INTRINSIC_FUNCTION("SampleGrad", HLSLBaseType_Texture3D, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2);
    //TEXTURE_INTRINSIC_FUNCTION("SampleGrad", HLSLBaseType_Texture3D, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2);
    
    // TODO: for 2D tex (int2 offset is optional, how to indicate that?)
    TEXTURE_INTRINSIC_FUNCTION("GatherRed", HLSLBaseType_Texture2D,  HLSLBaseType_Float2);
    TEXTURE_INTRINSIC_FUNCTION("GatherGreen", HLSLBaseType_Texture2D,  HLSLBaseType_Float2);
    TEXTURE_INTRINSIC_FUNCTION("GatherBlue", HLSLBaseType_Texture2D,  HLSLBaseType_Float2);
    TEXTURE_INTRINSIC_FUNCTION("GatherAlpha", HLSLBaseType_Texture2D,  HLSLBaseType_Float2);
    
    TEXTURE_INTRINSIC_FUNCTION_H("GatherRed", HLSLBaseType_Texture2D,  HLSLBaseType_Float2);
    TEXTURE_INTRINSIC_FUNCTION_H("GatherGreen", HLSLBaseType_Texture2D,  HLSLBaseType_Float2);
    TEXTURE_INTRINSIC_FUNCTION_H("GatherBlue", HLSLBaseType_Texture2D,  HLSLBaseType_Float2);
    TEXTURE_INTRINSIC_FUNCTION_H("GatherAlpha", HLSLBaseType_Texture2D,  HLSLBaseType_Float2);
    
    // TODO: GetDimensions
    return true;
};

static bool initIntrinsics = InitIntrinsics();

// The order in this array must match up with HLSLBinaryOp
const int _binaryOpPriority[] =
    {
        2, 1, //  &&, ||
        8, 8, //  +,  -
        9, 9, //  *,  /
        7, 7, //  <,  >,
        7, 7, //  <=, >=,
        6, 6, //  ==, !=
        5, 3, 4, // &, |, ^
    };



BaseTypeDescription baseTypeDescriptions[HLSLBaseType_Count];

void RegisterMatrix(HLSLBaseType type, uint32_t typeOffset, NumericType numericType,  int binaryOpRank, const char* typeName, uint32_t dim1, uint32_t dim2)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%dx%d", typeName, dim1, dim2);
    const char* name = gStringPool.AddString(buf);
    
    HLSLBaseType baseType = (HLSLBaseType)(type + typeOffset);
    
    BaseTypeDescription& desc = baseTypeDescriptions[baseType];
    desc.typeName = name;
    desc.typeNameMetal = name;
    
    desc.baseType = baseType;
    desc.coreType = CoreType_Matrix;
    desc.dimensionType = DimensionType(DimensionType_Matrix2x2 + (dim2 - 2));
    desc.numericType = numericType;
    
    desc.numDimensions = 2;
    desc.numComponents = dim1;
    desc.height = dim2;
    desc.binaryOpRank = binaryOpRank;
}

void RegisterVector(HLSLBaseType type, uint32_t typeOffset, NumericType numericType,  int binaryOpRank,  const char* typeName, uint32_t dim)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%d", typeName, dim);
    const char* name = gStringPool.AddString(buf);
    
    HLSLBaseType baseType = (HLSLBaseType)(type + typeOffset);
    
    BaseTypeDescription& desc = baseTypeDescriptions[type + typeOffset];
    desc.typeName = name;
    desc.typeNameMetal = name;
    
    // 4 types
    desc.baseType = baseType;
    desc.coreType = CoreType_Vector;
    desc.dimensionType = DimensionType(DimensionType_Vector2 + (dim - 2));
    desc.numericType = numericType;
    
    desc.numDimensions = 1;
    desc.numComponents = dim;
    desc.height = 1;
    desc.binaryOpRank = binaryOpRank;
}

void RegisterScalar(HLSLBaseType type, uint32_t typeOffset, NumericType numericType,  int binaryOpRank, const char* typeName)
{
    const char* name = gStringPool.AddString(typeName);
    
    HLSLBaseType baseType = (HLSLBaseType)(type + typeOffset);
    
    BaseTypeDescription& desc = baseTypeDescriptions[baseType];
    desc.typeName = name;
    desc.typeNameMetal = name;
    
    // 4 types
    desc.baseType = baseType;
    desc.coreType = CoreType_Scalar;
    desc.dimensionType = DimensionType_Scalar;
    desc.numericType = numericType;
    
    desc.numDimensions = 0;
    desc.numComponents = 1;
    desc.height = 1;
    desc.binaryOpRank = binaryOpRank;
}

void RegisterTexture(HLSLBaseType baseType, const char* typeName, const char* typeNameMetal)
{
    BaseTypeDescription& desc = baseTypeDescriptions[baseType];
    desc.baseType = baseType;
    desc.typeName = typeName;
    desc.typeNameMetal = typeNameMetal;
    
    desc.coreType = CoreType_Texture;
}

void RegisterSampler(HLSLBaseType baseType, const char* typeName, const char* typeNameMetal)
{
    BaseTypeDescription& desc = baseTypeDescriptions[baseType];
    desc.baseType = baseType;
    desc.typeName = typeName;
    desc.typeNameMetal = typeNameMetal;
    
    desc.coreType = CoreType_Sampler;
}

void RegisterType(HLSLBaseType baseType, CoreType coreType, const char* typeName)
{
    BaseTypeDescription& desc = baseTypeDescriptions[baseType];
    desc.baseType = baseType;
    desc.typeName = typeName;
    desc.typeNameMetal = typeName;
    
    desc.coreType = coreType;
}


bool InitBaseTypeDescriptions()
{
    {
        const uint32_t kNumTypes = 3;
        const char* typeNames[kNumTypes] = { "float", "half", "double" };
        const HLSLBaseType baseTypes[kNumTypes] = { HLSLBaseType_Float, HLSLBaseType_Half, HLSLBaseType_Double };
        const NumericType numericTypes[kNumTypes] = { NumericType_Float, NumericType_Half, NumericType_Double };
        const int binaryOpRanks[kNumTypes] = { 0, 1, 2 };
        
        for (uint32_t i = 0; i < kNumTypes; ++i)
        {
            const char* typeName = typeNames[i];
            HLSLBaseType baseType = baseTypes[i];
            NumericType numericType = numericTypes[i];
            int binaryOpRank = binaryOpRanks[i];
            
            RegisterScalar(baseType, 0, numericType, binaryOpRank, typeName);
            RegisterVector(baseType, 1, numericType, binaryOpRank, typeName, 2);
            RegisterVector(baseType, 2, numericType, binaryOpRank, typeName, 3);
            RegisterVector(baseType, 3, numericType, binaryOpRank, typeName, 4);
            
            RegisterMatrix(baseType, 4, numericType, binaryOpRank, typeName, 2, 2);
            RegisterMatrix(baseType, 5, numericType, binaryOpRank, typeName, 3, 3);
            RegisterMatrix(baseType, 6, numericType, binaryOpRank, typeName, 4, 4);
        }
    }
    
    {
        const uint32_t kNumTypes = 7;
        const char* typeNames[kNumTypes] = {
            "int", "uint",
            "long", "ulong",
            "short", "ushort",
            "bool"
        };
        const HLSLBaseType baseTypes[kNumTypes] = {
            HLSLBaseType_Int,  HLSLBaseType_Uint,
            HLSLBaseType_Long, HLSLBaseType_Ulong,
            HLSLBaseType_Short, HLSLBaseType_Ushort,
            HLSLBaseType_Bool
        };
        const NumericType numericTypes[kNumTypes] = {
            NumericType_Int,  NumericType_Uint,
            NumericType_Long,  NumericType_Ulong,
            NumericType_Short, NumericType_Ushort,
            NumericType_Bool
        };
        const int binaryOpRanks[kNumTypes] = {
            2, 1, // Note: int seems like it should be highest
            3, 2,
            4, 3,
            4
        };
        
        for (uint32_t i = 0; i < kNumTypes; ++i)
        {
            const char* typeName = typeNames[i];
            HLSLBaseType baseType = baseTypes[i];
            NumericType numericType = numericTypes[i];
            int binaryOpRank = binaryOpRanks[i];
            
            RegisterScalar(baseType, 0, numericType, binaryOpRank, typeName);
            RegisterVector(baseType, 1, numericType, binaryOpRank, typeName, 2);
            RegisterVector(baseType, 2, numericType, binaryOpRank, typeName, 3);
            RegisterVector(baseType, 3, numericType, binaryOpRank, typeName, 4);
        }
    }
    
    // TODO: add u/char, but HLSL2021 doesn't have support, but MSL does
    
    // TODO: would it be better to use "texture" base type (see "buffer")
    // and then have a TextureSubType off that?
    
    // texutres
    RegisterTexture(HLSLBaseType_Texture2D, "Texture2D", "texture2d");
    RegisterTexture(HLSLBaseType_Texture2DArray, "Texture2DArray", "texture2d_array");
    RegisterTexture(HLSLBaseType_Texture3D, "Texture3D", "texture3d");
    RegisterTexture(HLSLBaseType_TextureCube, "TextureCube", "texturecube");
    RegisterTexture(HLSLBaseType_TextureCubeArray, "TextureCubeArray", "texturecube_rray");
    RegisterTexture(HLSLBaseType_Texture2DMS, "Texture2DMS", "texture2d_ms");
    
    RegisterTexture(HLSLBaseType_Depth2D, "Depth2D", "depth2d");
    RegisterTexture(HLSLBaseType_Depth2DArray, "Depth2DArray", "depth2d_array");
    RegisterTexture(HLSLBaseType_DepthCube, "DepthCube", "depthcube");
    
    RegisterTexture(HLSLBaseType_RWTexture2D, "RWTexture2D", "texture2d");
    
    // samplers
    RegisterSampler(HLSLBaseType_SamplerState, "SamplerState", "sampler");
    RegisterSampler(HLSLBaseType_SamplerComparisonState, "SamplerComparisonState", "sampler");
    
    RegisterType(HLSLBaseType_UserDefined, CoreType_Struct, "struct");
    RegisterType(HLSLBaseType_Void, CoreType_Void, "void");
    RegisterType(HLSLBaseType_Unknown, CoreType_None, "unknown");
    RegisterType(HLSLBaseType_Expression, CoreType_Expression, "expression");
    RegisterType(HLSLBaseType_Comment, CoreType_Comment, "comment");
    RegisterType(HLSLBaseType_Buffer, CoreType_Buffer, "buffer");
    
    return true;
}

static bool _initBaseTypeDescriptions = InitBaseTypeDescriptions();


HLSLBaseType ArithmeticOpResultType(HLSLBinaryOp binaryOp, HLSLBaseType t1, HLSLBaseType t2)
{
    // check that both are same numeric types
    
    // add, sub, div are similar
    // mul is it's own test

    // most mixing of types is invalid here
    
    if (IsNumericTypeEqual(t1, t2))
    {
        bool isSameDimensions = IsDimensionEqual(t1, t2);
        
            
        
        if (IsScalarType(t1) && IsScalarType(t2))
        {
            if (isSameDimensions) return t1;
        }
        else if (IsVectorType(t1) && IsVectorType(t2))
        {
            if (isSameDimensions) return t1;
        }
        else if (IsMatrixType(t1) && IsMatrixType(t2))
        {
            if (isSameDimensions) return t1;
        }
        
        else if ((binaryOp == HLSLBinaryOp_Add || binaryOp == HLSLBinaryOp_Sub) &&
                 (IsScalarType(t1) || IsScalarType(t2)))
        {
            // allow v + 1, and 1 - v
            return (IsVectorType(t1) || IsMatrixType(t1)) ? t1 : t2;
        }
         
        else if ((binaryOp == HLSLBinaryOp_Mul || binaryOp == HLSLBinaryOp_Div) &&
                 (IsScalarType(t1) || IsScalarType(t2)))
        {
            // v * s
            return (IsVectorType(t1) || IsMatrixType(t1)) ? t1 : t2;
        }
        
        // this has to check dimension across the mul
        else if (binaryOp == HLSLBinaryOp_Mul)
        {
            bool isSameCrossDimension = IsCrossDimensionEqual(t1, t2);
            
            if (IsMatrixType(t1) && IsVectorType(t2))
            {
                if (isSameCrossDimension) return t2;
            }
            else if (IsVectorType(t1) && IsMatrixType(t2))
            {
                if (isSameCrossDimension) return t1;
            }
        }
    }
    
    return HLSLBaseType_Unknown;
}
    
// Priority of the ? : operator.
const int _conditionalOpPriority = 1;

const char* GetTypeNameHLSL(const HLSLType& type)
{
    if (type.baseType == HLSLBaseType_UserDefined)
    {
        return type.typeName;
    }
    else
    {
        return baseTypeDescriptions[type.baseType].typeName;
    }
}

const char* GetTypeNameMetal(const HLSLType& type)
{
    if (type.baseType == HLSLBaseType_UserDefined)
    {
        return type.typeName;
    }
    else
    {
        return baseTypeDescriptions[type.baseType].typeNameMetal;
    }
}

static const char* GetBinaryOpName(HLSLBinaryOp binaryOp)
{
    switch (binaryOp)
    {
    case HLSLBinaryOp_And:          return "&&";
    case HLSLBinaryOp_Or:           return "||";
            
    case HLSLBinaryOp_Add:          return "+";
    case HLSLBinaryOp_Sub:          return "-";
    case HLSLBinaryOp_Mul:          return "*";
    case HLSLBinaryOp_Div:          return "/";
            
    case HLSLBinaryOp_Less:         return "<";
    case HLSLBinaryOp_Greater:      return ">";
    case HLSLBinaryOp_LessEqual:    return "<=";
    case HLSLBinaryOp_GreaterEqual: return ">=";
    case HLSLBinaryOp_Equal:        return "==";
    case HLSLBinaryOp_NotEqual:     return "!=";
            
    case HLSLBinaryOp_BitAnd:       return "&";
    case HLSLBinaryOp_BitOr:        return "|";
    case HLSLBinaryOp_BitXor:       return "^";
            
    case HLSLBinaryOp_Assign:       return "=";
    case HLSLBinaryOp_AddAssign:    return "+=";
    case HLSLBinaryOp_SubAssign:    return "-=";
    case HLSLBinaryOp_MulAssign:    return "*=";
    case HLSLBinaryOp_DivAssign:    return "/=";
    default:
        ASSERT(false);
        return "???";
    }
}


/*
 * 1.) Match
 * 2.) Scalar dimension promotion (scalar -> vector/matrix)
 * 3.) Conversion
 * 4.) Conversion + scalar dimension promotion
 * 5.) Truncation (vector -> scalar or lower component vector, matrix -> scalar or lower component matrix)
 * 6.) Conversion + truncation
 */    
static int GetTypeCastRank(HLSLTree * tree, const HLSLType& srcType, const HLSLType& dstType)
{
    /*if (srcType.array != dstType.array || srcType.arraySize != dstType.arraySize)
    {
        return -1;
    }*/

    if (srcType.array != dstType.array)
    {
        return -1;
    }

    if (srcType.array == true)
    {
        ASSERT(dstType.array == true);
        int srcArraySize = -1;
        int dstArraySize = -1;

        tree->GetExpressionValue(srcType.arraySize, srcArraySize);
        tree->GetExpressionValue(dstType.arraySize, dstArraySize);

        if (srcArraySize != dstArraySize) {
            return -1;
        }
    }

    if (srcType.baseType == HLSLBaseType_UserDefined && dstType.baseType == HLSLBaseType_UserDefined)
    {
        return String_Equal(srcType.typeName, dstType.typeName) ? 0 : -1;
    }

    if (srcType.baseType == dstType.baseType)
    {
        // This only works if textures are half or float, but not hwne
        // there are more varied texture that can be cast.
        if (IsTextureType(srcType.baseType))
        {
            return srcType.formatType == dstType.formatType ? 0 : -1;
        }
        
        return 0;
    }

    const BaseTypeDescription& srcDesc = baseTypeDescriptions[srcType.baseType];
    const BaseTypeDescription& dstDesc = baseTypeDescriptions[dstType.baseType];
    if (srcDesc.numericType == NumericType_NaN || dstDesc.numericType == NumericType_NaN)
    {
        return -1;
    }

    // Result bits: T R R R P (T = truncation, R = conversion rank, P = dimension promotion)
    int result = _numberTypeRank[srcDesc.numericType][dstDesc.numericType] << 1;

    if (srcDesc.numDimensions == 0 && dstDesc.numDimensions > 0)
    {
        // Scalar dimension promotion
        result |= (1 << 0);
    }
    else if ((srcDesc.numDimensions == dstDesc.numDimensions && (srcDesc.numComponents > dstDesc.numComponents || srcDesc.height > dstDesc.height)) ||
             (srcDesc.numDimensions > 0 && dstDesc.numDimensions == 0))
    {
        // Truncation
        result |= (1 << 4);
    }
    else if (srcDesc.numDimensions != dstDesc.numDimensions ||
             srcDesc.numComponents != dstDesc.numComponents ||
             srcDesc.height != dstDesc.height)
    {
        // Can't convert
        return -1;
    }
    
    return result;
    
}

static bool GetFunctionCallCastRanks(HLSLTree* tree, const HLSLFunctionCall* call, const HLSLFunction* function, int* rankBuffer)
{

    if (function == NULL || function->numArguments < call->numArguments)
    {
        // Function not viable
        return false;
    }

    const HLSLExpression* expression = call->argument;
    const HLSLArgument* argument = function->argument;
   
    for (int i = 0; i < call->numArguments; ++i)
    {
        int rank = GetTypeCastRank(tree, expression->expressionType, argument->type);
        if (rank == -1)
        {
            return false;
        }

        rankBuffer[i] = rank;
        
        argument = argument->nextArgument;
        expression = expression->nextExpression;
    }

    for (int i = call->numArguments; i < function->numArguments; ++i)
    {
        if (argument->defaultValue == NULL)
        {
            // Function not viable.
            return false;
        }
    }

    return true;

}

struct CompareRanks
{
    bool operator() (const int& rank1, const int& rank2) { return rank1 > rank2; }
};

static CompareFunctionsResult CompareFunctions(HLSLTree* tree, const HLSLFunctionCall* call, const HLSLFunction* function1, const HLSLFunction* function2)
{ 

    int* function1Ranks = static_cast<int*>(alloca(sizeof(int) * call->numArguments));
    int* function2Ranks = static_cast<int*>(alloca(sizeof(int) * call->numArguments));

    const bool function1Viable = GetFunctionCallCastRanks(tree, call, function1, function1Ranks);
    const bool function2Viable = GetFunctionCallCastRanks(tree, call, function2, function2Ranks);

    // Both functions have to be viable to be able to compare them
    if (!(function1Viable && function2Viable))
    {
        if (function1Viable)
        {
            return Function1Better;
        }
        else if (function2Viable)
        {
            return Function2Better;
        }
        else
        {
            return FunctionsEqual;
        }
    }

    std::sort(function1Ranks, function1Ranks + call->numArguments, CompareRanks());
    std::sort(function2Ranks, function2Ranks + call->numArguments, CompareRanks());
    
    for (int i = 0; i < call->numArguments; ++i)
    {
        if (function1Ranks[i] < function2Ranks[i])
        {
            return Function1Better;
        }
        else if (function2Ranks[i] < function1Ranks[i])
        {
            return Function2Better;
        }
    }

    return FunctionsEqual;

}

static bool GetBinaryOpResultType(HLSLBinaryOp binaryOp, const HLSLType& type1, const HLSLType& type2, HLSLType& result)
{
    // only allow numeric types for binary operators
    if (type1.baseType < HLSLBaseType_FirstNumeric || type1.baseType > HLSLBaseType_LastNumeric || type1.array ||
        type2.baseType < HLSLBaseType_FirstNumeric || type2.baseType > HLSLBaseType_LastNumeric || type2.array)
    {
         return false;
    }

    if (IsBitOp(binaryOp))
    {
        if (type1.baseType < HLSLBaseType_FirstInteger ||
            type1.baseType > HLSLBaseType_LastInteger)
        {
            return false;
        }
    }

    if (IsLogicOp(binaryOp) || IsCompareOp(binaryOp))
    {
        int numComponents = std::max( baseTypeDescriptions[ type1.baseType ].numComponents, baseTypeDescriptions[ type2.baseType ].numComponents );
        result.baseType = HLSLBaseType( HLSLBaseType_Bool + numComponents - 1 );
    }
    else
    {
        // TODO: allso mulAssign, ...
        assert(!IsAssignOp(binaryOp));
        
        result.baseType = ArithmeticOpResultType(binaryOp, type1.baseType, type2.baseType);
    }

    result.typeName     = NULL;
    result.array        = false;
    result.arraySize    = NULL;
    result.flags        = (type1.flags & type2.flags) & HLSLTypeFlag_Const; // Propagate constness.
    
    return result.baseType != HLSLBaseType_Unknown;

}

HLSLParser::HLSLParser(Allocator* allocator, const char* fileName, const char* buffer, size_t length) : 
    m_tokenizer(fileName, buffer, length),
    m_userTypes(allocator),
    m_variables(allocator),
    m_functions(allocator)
{
    m_numGlobals = 0;
    m_tree = NULL;
}

bool HLSLParser::Accept(int token)
{
    if (m_tokenizer.GetToken() == token)
    {
       m_tokenizer.Next();
       return true;
    }
    return false;
}

bool HLSLParser::Accept(const char* token)
{
    if (m_tokenizer.GetToken() == HLSLToken_Identifier && String_Equal( token, m_tokenizer.GetIdentifier() ) )
    {
        m_tokenizer.Next();
        return true;
    }
    return false;
}

bool HLSLParser::Expect(int token)
{
    if (!Accept(token))
    {
        char want[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(token, want);
        char near[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(near);
        m_tokenizer.Error("Syntax error: expected '%s' near '%s'", want, near);
        return false;
    }
    return true;
}

bool HLSLParser::Expect(const char * token)
{
    if (!Accept(token))
    {
        const char * want = token;
        char near[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(near);
        m_tokenizer.Error("Syntax error: expected '%s' near '%s'", want, near);
        return false;
    }
    return true;
}


bool HLSLParser::AcceptIdentifier(const char*& identifier)
{
    if (m_tokenizer.GetToken() == HLSLToken_Identifier)
    {
        identifier = m_tree->AddString( m_tokenizer.GetIdentifier() );
        m_tokenizer.Next();
        return true;
    }
    return false;
}

bool HLSLParser::ExpectIdentifier(const char*& identifier)
{
    if (!AcceptIdentifier(identifier))
    {
        char near[HLSLTokenizer::s_maxIdentifier] = {};
        m_tokenizer.GetTokenName(near);
        m_tokenizer.Error("Syntax error: expected identifier near '%s'", near);
        identifier = "";
        return false;
    }
    return true;
}

bool HLSLParser::AcceptFloat(float& value)
{
    if (m_tokenizer.GetToken() == HLSLToken_FloatLiteral)
    {
        value = m_tokenizer.GetFloat();
        m_tokenizer.Next();
        return true;
    }
    return false;
}

bool HLSLParser::AcceptHalf( float& value )
{
	if( m_tokenizer.GetToken() == HLSLToken_HalfLiteral )
	{
		value = m_tokenizer.GetFloat();
		m_tokenizer.Next();
		return true;
	}
	return false;
}

bool HLSLParser::AcceptInt(int& value)
{
    if (m_tokenizer.GetToken() == HLSLToken_IntLiteral)
    {
        value = m_tokenizer.GetInt();
        m_tokenizer.Next();
        return true;
    }
    return false;
}

bool HLSLParser::ParseTopLevel(HLSLStatement*& statement)
{
    HLSLAttribute * attributes = NULL;
    ParseAttributeBlock(attributes);

    int line             = GetLineNumber();
    const char* fileName = GetFileName();
    
    HLSLType type;
    //HLSLBaseType type;
    //const char*  typeName = NULL;
    //int          typeFlags = false;

    // TODO: this cast likely isn't safe
    HLSLToken token = (HLSLToken)m_tokenizer.GetToken();
    
    bool doesNotExpectSemicolon = false;

    // Alec add comment
    if (ParseComment(statement))
    {
        doesNotExpectSemicolon = true;
    }
    else if (Accept(HLSLToken_Struct))
    {
        // Struct declaration.

        const char* structName = NULL;
        if (!ExpectIdentifier(structName))
        {
            return false;
        }
        if (FindUserDefinedType(structName) != NULL)
        {
            m_tokenizer.Error("struct %s already defined", structName);
            return false;
        }

        if (!Expect('{'))
        {
            return false;
        }

        HLSLStruct* structure = m_tree->AddNode<HLSLStruct>(fileName, line);
        structure->name = structName;

        m_userTypes.PushBack(structure);
 
        HLSLStructField* lastField = NULL;

        // Add the struct to our list of user defined types.
        while (!Accept('}'))
        {
            if (CheckForUnexpectedEndOfStream('}'))
            {
                return false;
            }
            
            // chain fields onto struct
            HLSLStructField* field = NULL;
            if (!ParseFieldDeclaration(field))
            {
                return false;
            }
            ASSERT(field != NULL);
            if (lastField == NULL)
            {
                structure->field = field;
            }
            else
            {
                lastField->nextField = field;
            }
            lastField = field;
        }

        statement = structure;
    }
    else if (Accept(HLSLToken_ConstantBuffer) ||
             Accept(HLSLToken_StructuredBuffer) ||
             Accept(HLSLToken_RWStructuredBuffer) ||
             Accept(HLSLToken_ByteAddressBuffer) ||
             Accept(HLSLToken_RWByteAddressBuffer))
    {
        HLSLBuffer* buffer = m_tree->AddNode<HLSLBuffer>(fileName, line);
        
        // these can appear on t or u slots for read vs. read/write
        // need to track what the user specified.  Load vs. Store calls.
        buffer->bufferType = ConvertTokenToBufferType(token);
    
        // Is template struct type required?
        if (Expect('<'))
        {
            const char* structName = nullptr;
            
            // Read the templated type, should reference a struct
            // don't need to support fields on this.
            if (!ExpectIdentifier(structName) || !Expect('>'))
            {
                return false;
            }
           
            buffer->bufferStruct = const_cast<HLSLStruct*>(FindUserDefinedType(structName));
            if (!buffer->bufferStruct)
            {
                return false;
            }
        }
        
        // get name of buffer
        AcceptIdentifier(buffer->name);
    
        // Parse ": register(t0/u0)"
        if (Accept(':'))
        {
            if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(buffer->registerName) || !Expect(')'))
            {
                return false;
            }
            // TODO: Check that we aren't re-using a register.
        }
        
        // Buffer needs to show up to reference the fields
        // of the struct of the templated type.
        HLSLType bufferType(HLSLBaseType_UserDefined);
        bufferType.typeName = buffer->bufferStruct->name; // this is for userDefined name (f.e. struct)
        
        DeclareVariable( buffer->name, bufferType );
       
        // TODO: add fields as variables too?
        
        statement = buffer;
    }
    else if (Accept(HLSLToken_CBuffer) || Accept(HLSLToken_TBuffer))
    {
        // cbuffer/tbuffer declaration.

        HLSLBuffer* buffer = m_tree->AddNode<HLSLBuffer>(fileName, line);
        AcceptIdentifier(buffer->name);

        buffer->bufferType = ConvertTokenToBufferType(token);
        
        // Optional register assignment.
        if (Accept(':'))
        {
            if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(buffer->registerName) || !Expect(')'))
            {
                return false;
            }
            // TODO: Check that we aren't re-using a register.
        }

        // Fields are defined inside the c/tbuffer.
        // These represent globals to the rest of the codebase which
        // is simply evil.
        
        if (!Expect('{'))
        {
            return false;
        }
        HLSLDeclaration* lastField = NULL;
        while (!Accept('}'))
        {
            if (CheckForUnexpectedEndOfStream('}'))
            {
                return false;
            }
            
            // TODO: can't convert statement to fields
            if (ParseComment(statement))
            {
                continue;
            }
           
            HLSLDeclaration* field = NULL;
            if (!ParseDeclaration(field))
            {
                m_tokenizer.Error("Expected variable declaration");
                return false;
            }
            
            // These show up as global variables of the fields
            DeclareVariable( field->name, field->type );
            
            // chain fields onto buffer
            field->buffer = buffer;
            if (buffer->field == NULL)
            {
                buffer->field = field;
            }
            else
            {
                lastField->nextStatement = field;
            }
            lastField = field;
            
            if (!Expect(';')) {
                return false;
            }

        }

        statement = buffer;
    }
    else if (AcceptType(true, type))
    {
        // Global declaration (uniform or function).
        const char* globalName = NULL;
        if (!ExpectIdentifier(globalName))
        {
            return false;
        }

        if (Accept('('))
        {
            // Function declaration.

            HLSLFunction* function = m_tree->AddNode<HLSLFunction>(fileName, line);
            function->name                  = globalName;
            function->returnType.baseType   = type.baseType;
            function->returnType.typeName   = type.typeName;
            function->attributes            = attributes;

            BeginScope();

            if (!ParseArgumentList(function->argument, function->numArguments, function->numOutputArguments))
            {
                return false;
            }

            const HLSLFunction* declaration = FindFunction(function);

            // Forward declaration
            if (Accept(';'))
            {
                // Add a function entry so that calls can refer to it
                if (!declaration)
                {
                    m_functions.PushBack( function );
                    statement = function;
                }
                EndScope();
                return true;
            }

            // Optional semantic.
            if (Accept(':') && !ExpectIdentifier(function->semantic))
            {
                return false;
            }

            if (declaration)
            {
                if (declaration->forward || declaration->statement)
                {
                    m_tokenizer.Error("Duplicate function definition");
                    return false;
                }

                const_cast<HLSLFunction*>(declaration)->forward = function;
            }
            else
            {
                m_functions.PushBack( function );
            }

            if (!Expect('{') || !ParseBlock(function->statement, function->returnType))
            {
                return false;
            }

            EndScope();

            // Note, no semi-colon at the end of a function declaration.
            statement = function;
            
            return true;
        }
        else
        {
            // Uniform declaration.
            HLSLDeclaration* declaration = m_tree->AddNode<HLSLDeclaration>(fileName, line);
            declaration->name            = globalName;
            declaration->type            = type;

            // Handle array syntax.
            if (Accept('['))
            {
                if (!Accept(']'))
                {
                    if (!ParseExpression(declaration->type.arraySize) || !Expect(']'))
                    {
                        return false;
                    }
                }
                declaration->type.array = true;
            }

            // Handle optional register.
            if (Accept(':'))
            {
                // @@ Currently we support either a semantic or a register, but not both.
                if (AcceptIdentifier(declaration->semantic)) {
                    // int k = 1;
                }
                else if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(declaration->registerName) || !Expect(')'))
                {
                    return false;
                }
            }

            DeclareVariable( globalName, declaration->type );

            if (!ParseDeclarationAssignment(declaration))
            {
                return false;
            }

            // TODO: Multiple variables declared on one line.
            
            statement = declaration;
        }
    }
    
    /*
    // These three are from .fx file syntax
    else if (ParseTechnique(statement)) {
        doesNotExpectSemicolon = true;
    }
    else if (ParsePipeline(statement)) {
        doesNotExpectSemicolon = true;
    }
    else if (ParseStage(statement)) {
        doesNotExpectSemicolon = true;
    }
    */
    
    if (statement != NULL) {
        statement->attributes = attributes;
    }

    return doesNotExpectSemicolon || Expect(';');
}

bool HLSLParser::ParseStatementOrBlock(HLSLStatement*& firstStatement, const HLSLType& returnType, bool scoped/*=true*/)
{
    if (scoped)
    {
        BeginScope();
    }
    if (Accept('{'))
    {
        if (!ParseBlock(firstStatement, returnType))
        {
            return false;
        }
    }
    else
    {
        if (!ParseStatement(firstStatement, returnType))
        {
            return false;
        }
    }
    if (scoped)
    {
        EndScope();
    }
    return true;
}

bool HLSLParser::ParseComment(HLSLStatement*& statement)
{
    if (m_tokenizer.GetToken() != HLSLToken_Comment)
        return false;
    
    const char* textName = m_tree->AddString(m_tokenizer.GetComment());
    
    // This has already parsed the next comment before have had a chance to
    // grab the string from the previous comment, if they were sequenential comments.
    // So grabbing a copy of comment before this parses the next comment.
    if (!Accept(HLSLToken_Comment))
        return false;
    
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    HLSLComment* comment = m_tree->AddNode<HLSLComment>(fileName, line);
    comment->text = textName;
    
    // pass it back
    statement = comment;
    return true;
}

bool HLSLParser::ParseBlock(HLSLStatement*& firstStatement, const HLSLType& returnType)
{
    HLSLStatement* lastStatement = NULL;
    while (!Accept('}'))
    {
        if (CheckForUnexpectedEndOfStream('}'))
        {
            return false;
        }
        
        HLSLStatement* statement = NULL;
        
        if (!ParseStatement(statement, returnType))
        {
            return false;
        }
        
        // chain statements onto the list
        if (statement != NULL)
        {
            if (firstStatement == NULL)
            {
                firstStatement = statement;
            }
            else
            {
                lastStatement->nextStatement = statement;
            }
            lastStatement = statement;
            
            // some statement parsing can gen more than one statement, so find end
            while (lastStatement->nextStatement)
                lastStatement = lastStatement->nextStatement;
        }
    }
    return true;
}

bool HLSLParser::ParseStatement(HLSLStatement*& statement, const HLSLType& returnType)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    // Empty statements.
    if (Accept(';'))
    {
        return true;
    }

    HLSLAttribute * attributes = NULL;
    ParseAttributeBlock(attributes);    // @@ Leak if not assigned to node? 

#if 0
    // @@ Work in progress.
    // Alec? - @If, @Else blocks, are these like specialization constants?
/*
    // Static statements: @if only for now.
    if (Accept('@'))
    {
        if (Accept(HLSLToken_If))
        {
            //HLSLIfStatement* ifStatement = m_tree->AddNode<HLSLIfStatement>(fileName, line);
            //ifStatement->isStatic = true;
            //ifStatement->attributes = attributes;
            
            HLSLExpression * condition = NULL;
            
            m_allowUndeclaredIdentifiers = true;    // Not really correct... better to push to stack?
            if (!Expect('(') || !ParseExpression(condition) || !Expect(')'))
            {
                m_allowUndeclaredIdentifiers = false;
                return false;
            }
            m_allowUndeclaredIdentifiers = false;
            
            if ((condition->expressionType.flags & HLSLTypeFlag_Const) == 0)
            {
                m_tokenizer.Error("Syntax error: @if condition is not constant");
                return false;
            }
            
            int conditionValue;
            if (!m_tree->GetExpressionValue(condition, conditionValue))
            {
                m_tokenizer.Error("Syntax error: Cannot evaluate @if condition");
                return false;
            }
            
            if (!conditionValue) m_disableSemanticValidation = true;
            
            HLSLStatement * ifStatements = NULL;
            HLSLStatement * elseStatements = NULL;
            
            if (!ParseStatementOrBlock(ifStatements, returnType, false))
            {
                m_disableSemanticValidation = false;
                return false;
            }
            if (Accept(HLSLToken_Else))
            {
                if (conditionValue) m_disableSemanticValidation = true;
                
                if (!ParseStatementOrBlock(elseStatements, returnType, false))
                {
                    m_disableSemanticValidation = false;
                    return false;
                }
            }
            m_disableSemanticValidation = false;
            
            if (conditionValue) statement = ifStatements;
            else statement = elseStatements;
            
            // @@ Free the pruned statements?
            
            return true;
        }
        else {
            m_tokenizer.Error("Syntax error: unexpected token '@'");
        }
    }
*/
#endif
    
    // Getting 2 copies of some comments, why is that
    if (ParseComment(statement))
    {
        return true;
    }
    
    // If statement.
    if (Accept(HLSLToken_If))
    {
        HLSLIfStatement* ifStatement = m_tree->AddNode<HLSLIfStatement>(fileName, line);
        ifStatement->attributes = attributes;
        if (!Expect('(') || !ParseExpression(ifStatement->condition) || !Expect(')'))
        {
            return false;
        }
        statement = ifStatement;
        if (!ParseStatementOrBlock(ifStatement->statement, returnType))
        {
            return false;
        }
        if (Accept(HLSLToken_Else))
        {
            return ParseStatementOrBlock(ifStatement->elseStatement, returnType);
        }
        return true;
    }
    
    // For statement.
    if (Accept(HLSLToken_For))
    {
        HLSLForStatement* forStatement = m_tree->AddNode<HLSLForStatement>(fileName, line);
        forStatement->attributes = attributes;
        if (!Expect('('))
        {
            return false;
        }
        BeginScope();
        if (!ParseDeclaration(forStatement->initialization))
        {
            return false;
        }
        if (!Expect(';'))
        {
            return false;
        }
        ParseExpression(forStatement->condition);
        if (!Expect(';'))
        {
            return false;
        }
        ParseExpression(forStatement->increment);
        if (!Expect(')'))
        {
            return false;
        }
        statement = forStatement;
        if (!ParseStatementOrBlock(forStatement->statement, returnType))
        {
            return false;
        }
        EndScope();
        return true;
    }

    if (attributes != NULL)
    {
        // @@ Error. Unexpected attribute. We only support attributes associated to if and for statements.
    }

    // Block statement.
    if (Accept('{'))
    {
        HLSLBlockStatement* blockStatement = m_tree->AddNode<HLSLBlockStatement>(fileName, line);
        statement = blockStatement;
        BeginScope();
        bool success = ParseBlock(blockStatement->statement, returnType);
        EndScope();
        return success;
    }

    // Discard statement.
    if (Accept(HLSLToken_Discard))
    {
        HLSLDiscardStatement* discardStatement = m_tree->AddNode<HLSLDiscardStatement>(fileName, line);
        statement = discardStatement;
        return Expect(';');
    }

    // Break statement.
    if (Accept(HLSLToken_Break))
    {
        HLSLBreakStatement* breakStatement = m_tree->AddNode<HLSLBreakStatement>(fileName, line);
        statement = breakStatement;
        return Expect(';');
    }

    // Continue statement.
    if (Accept(HLSLToken_Continue))
    {
        HLSLContinueStatement* continueStatement = m_tree->AddNode<HLSLContinueStatement>(fileName, line);
        statement = continueStatement;
        return Expect(';');
    }

    // Return statement
    if (Accept(HLSLToken_Return))
    {
        HLSLReturnStatement* returnStatement = m_tree->AddNode<HLSLReturnStatement>(fileName, line);
        if (!Accept(';') && !ParseExpression(returnStatement->expression))
        {
            return false;
        }
        // Check that the return expression can be cast to the return type of the function.
        HLSLType voidType(HLSLBaseType_Void);
        if (!CheckTypeCast(returnStatement->expression ? returnStatement->expression->expressionType : voidType, returnType))
        {
            return false;
        }

        statement = returnStatement;
        return Expect(';');
    }

    HLSLDeclaration* declaration = NULL;
    HLSLExpression*  expression  = NULL;

    if (ParseDeclaration(declaration))
    {
        statement = declaration;
    }
    else if (ParseExpression(expression))
    {
        HLSLExpressionStatement* expressionStatement;
        expressionStatement = m_tree->AddNode<HLSLExpressionStatement>(fileName, line);
        expressionStatement->expression = expression;
        statement = expressionStatement;
    }

    return Expect(';');
}


// IC: This is only used in block statements, or within control flow statements. So, it doesn't support semantics or layout modifiers.
// @@ We should add suport for semantics for inline input/output declarations.
bool HLSLParser::ParseDeclaration(HLSLDeclaration*& declaration)
{
    const char* fileName    = GetFileName();
    int         line        = GetLineNumber();

    HLSLType type;
    if (!AcceptType(/*allowVoid=*/false, type))
    {
        return false;
    }

    bool allowUnsizedArray = true;  // This is needed for SSBO
    
    HLSLDeclaration * firstDeclaration = NULL;
    HLSLDeclaration * lastDeclaration = NULL;

    do {
        const char* name;
        if (!ExpectIdentifier(name))
        {
            // TODO: false means we didn't accept a declaration and we had an error!
            return false;
        }
        // Handle array syntax.
        if (Accept('['))
        {
            type.array = true;
            // Optionally allow no size to the specified for the array.
            if (Accept(']') && allowUnsizedArray)
            {
                return true;
            }
            if (!ParseExpression(type.arraySize) || !Expect(']'))
            {
                return false;
            }
        }

        HLSLDeclaration * parsedDeclaration = m_tree->AddNode<HLSLDeclaration>(fileName, line);
        parsedDeclaration->type  = type;
        parsedDeclaration->name  = name;

        DeclareVariable( parsedDeclaration->name, parsedDeclaration->type );

        // Handle option assignment of the declared variables(s).
        if (!ParseDeclarationAssignment( parsedDeclaration )) {
            return false;
        }

        if (firstDeclaration == NULL) firstDeclaration = parsedDeclaration;
        if (lastDeclaration != NULL) lastDeclaration->nextDeclaration = parsedDeclaration;
        lastDeclaration = parsedDeclaration;

    } while(Accept(','));

    declaration = firstDeclaration;

    return true;
}

bool HLSLParser::ParseDeclarationAssignment(HLSLDeclaration* declaration)
{
    if (Accept('='))
    {
        // Handle array initialization syntax.
        if (declaration->type.array)
        {
            int numValues = 0;
            if (!Expect('{') || !ParseExpressionList('}', true, declaration->assignment, numValues))
            {
                return false;
            }
        }
//        else if (IsSamplerType(declaration->type.baseType)) // TODO: should be for SamplerStateBlock, not Sampler
//        {
//            if (!ParseSamplerState(declaration->assignment))
//            {
//                return false;
//            }
//        }
        else if (!ParseExpression(declaration->assignment))
        {
            return false;
        }
    }
    return true;
}

bool HLSLParser::ParseFieldDeclaration(HLSLStructField*& field)
{
    field = m_tree->AddNode<HLSLStructField>( GetFileName(), GetLineNumber() );
    if (!ExpectDeclaration(false, field->type, field->name))
    {
        return false;
    }
    // Handle optional semantics.
    if (Accept(':'))
    {
        if (!ExpectIdentifier(field->semantic))
        {
            return false;
        }
    }
    return Expect(';');
}

// @@ Add support for packoffset to general declarations.
/*bool HLSLParser::ParseBufferFieldDeclaration(HLSLBufferField*& field)
{
    field = m_tree->AddNode<HLSLBufferField>( GetFileName(), GetLineNumber() );
    if (AcceptDeclaration(false, field->type, field->name))
    {
        // Handle optional packoffset.
        if (Accept(':'))
        {
            if (!Expect("packoffset"))
            {
                return false;
            }
            const char* constantName = NULL;
            const char* swizzleMask  = NULL;
            if (!Expect('(') || !ExpectIdentifier(constantName) || !Expect('.') || !ExpectIdentifier(swizzleMask) || !Expect(')'))
            {
                return false;
            }
        }
        return Expect(';');
    }
    return false;
}*/

bool HLSLParser::CheckTypeCast(const HLSLType& srcType, const HLSLType& dstType)
{
    if (GetTypeCastRank(m_tree, srcType, dstType) == -1)
    {
        const char* srcTypeName = GetTypeNameHLSL(srcType);
        const char* dstTypeName = GetTypeNameHLSL(dstType);
        m_tokenizer.Error("Cannot implicitly convert from '%s' to '%s'", srcTypeName, dstTypeName);
        return false;
    }
    return true;
}

bool HLSLParser::ParseExpression(HLSLExpression*& expression)
{
    if (!ParseBinaryExpression(0, expression))
    {
        return false;
    }

    HLSLBinaryOp assignOp;
    if (AcceptAssign(assignOp))
    {
        HLSLExpression* expression2 = NULL;
        if (!ParseExpression(expression2))
        {
            return false;
        }
        HLSLBinaryExpression* binaryExpression = m_tree->AddNode<HLSLBinaryExpression>(expression->fileName, expression->line);
        binaryExpression->binaryOp = assignOp;
        binaryExpression->expression1 = expression;
        binaryExpression->expression2 = expression2;
        // This type is not strictly correct, since the type should be a reference.
        // However, for our usage of the types it should be sufficient.
        binaryExpression->expressionType = expression->expressionType;

        if (!CheckTypeCast(expression2->expressionType, expression->expressionType))
        {
            const char* srcTypeName = GetTypeNameHLSL(expression2->expressionType);
            const char* dstTypeName = GetTypeNameHLSL(expression->expressionType);
            m_tokenizer.Error("Cannot implicitly convert from '%s' to '%s'", srcTypeName, dstTypeName);
            return false;
        }

        expression = binaryExpression;
    }

    return true;
}

bool HLSLParser::AcceptBinaryOperator(int priority, HLSLBinaryOp& binaryOp)
{
    int token = m_tokenizer.GetToken();
    switch (token)
    {
    case HLSLToken_LogicalAnd:      binaryOp = HLSLBinaryOp_And;          break;
    case HLSLToken_LogicalOr:       binaryOp = HLSLBinaryOp_Or;           break;
    case '+':                       binaryOp = HLSLBinaryOp_Add;          break;
    case '-':                       binaryOp = HLSLBinaryOp_Sub;          break;
    case '*':                       binaryOp = HLSLBinaryOp_Mul;          break;
    case '/':                       binaryOp = HLSLBinaryOp_Div;          break;
    case '<':                       binaryOp = HLSLBinaryOp_Less;         break;
    case '>':                       binaryOp = HLSLBinaryOp_Greater;      break;
    case HLSLToken_LessEqual:       binaryOp = HLSLBinaryOp_LessEqual;    break;
    case HLSLToken_GreaterEqual:    binaryOp = HLSLBinaryOp_GreaterEqual; break;
    case HLSLToken_EqualEqual:      binaryOp = HLSLBinaryOp_Equal;        break;
    case HLSLToken_NotEqual:        binaryOp = HLSLBinaryOp_NotEqual;     break;
    case '&':                       binaryOp = HLSLBinaryOp_BitAnd;       break;
    case '|':                       binaryOp = HLSLBinaryOp_BitOr;        break;
    case '^':                       binaryOp = HLSLBinaryOp_BitXor;       break;
    default:
        return false;
    }
    if (_binaryOpPriority[binaryOp] > priority)
    {
        m_tokenizer.Next();
        return true;
    }
    return false;
}

bool HLSLParser::AcceptUnaryOperator(bool pre, HLSLUnaryOp& unaryOp)
{
    int token = m_tokenizer.GetToken();
    if (token == HLSLToken_PlusPlus)
    {
        unaryOp = pre ? HLSLUnaryOp_PreIncrement : HLSLUnaryOp_PostIncrement;
    }
    else if (token == HLSLToken_MinusMinus)
    {
        unaryOp = pre ? HLSLUnaryOp_PreDecrement : HLSLUnaryOp_PostDecrement;
    }
    else if (pre && token == '-')
    {
        unaryOp = HLSLUnaryOp_Negative;
    }
    else if (pre && token == '+')
    {
        unaryOp = HLSLUnaryOp_Positive;
    }
    else if (pre && token == '!')
    {
        unaryOp = HLSLUnaryOp_Not;
    }
    else if (pre && token == '~')
    {
        unaryOp = HLSLUnaryOp_Not;
    }
    else
    {
        return false;
    }
    m_tokenizer.Next();
    return true;
}

bool HLSLParser::AcceptAssign(HLSLBinaryOp& binaryOp)
{
    if (Accept('='))
    {
        binaryOp = HLSLBinaryOp_Assign;
    }
    else if (Accept(HLSLToken_PlusEqual))
    {
        binaryOp = HLSLBinaryOp_AddAssign;
    }
    else if (Accept(HLSLToken_MinusEqual))
    {
        binaryOp = HLSLBinaryOp_SubAssign;
    }     
    else if (Accept(HLSLToken_TimesEqual))
    {
        binaryOp = HLSLBinaryOp_MulAssign;
    }     
    else if (Accept(HLSLToken_DivideEqual))
    {
        binaryOp = HLSLBinaryOp_DivAssign;
    }     
    else
    {
        return false;
    }
    return true;
}

bool HLSLParser::ParseBinaryExpression(int priority, HLSLExpression*& expression)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    bool needsEndParen;

    if (!ParseTerminalExpression(expression, needsEndParen))
    {
        return false;
    }

	// reset priority cause openned parenthesis
	if( needsEndParen )
		priority = 0;

    while (1)
    {
        HLSLBinaryOp binaryOp;
        if (AcceptBinaryOperator(priority, binaryOp))
        {

            HLSLExpression* expression2 = NULL;
            ASSERT( binaryOp < sizeof(_binaryOpPriority) / sizeof(int) );
            if (!ParseBinaryExpression(_binaryOpPriority[binaryOp], expression2))
            {
                return false;
            }
            HLSLBinaryExpression* binaryExpression = m_tree->AddNode<HLSLBinaryExpression>(fileName, line);
            binaryExpression->binaryOp    = binaryOp;
            binaryExpression->expression1 = expression;
            binaryExpression->expression2 = expression2;
            if (!GetBinaryOpResultType( binaryOp, expression->expressionType, expression2->expressionType, binaryExpression->expressionType ))
            {
                const char* typeName1 = GetTypeNameHLSL( binaryExpression->expression1->expressionType );
                const char* typeName2 = GetTypeNameHLSL( binaryExpression->expression2->expressionType );
                m_tokenizer.Error("binary '%s' : no global operator found which takes types '%s' and '%s' (or there is no acceptable conversion)",
                    GetBinaryOpName(binaryOp), typeName1, typeName2);

                return false;
            }
            
            // Propagate constness.
            binaryExpression->expressionType.flags = (expression->expressionType.flags | expression2->expressionType.flags) & HLSLTypeFlag_Const;
            
            expression = binaryExpression;
        }
        else if (_conditionalOpPriority > priority && Accept('?'))
        {

            HLSLConditionalExpression* conditionalExpression = m_tree->AddNode<HLSLConditionalExpression>(fileName, line);
            conditionalExpression->condition = expression;
            
            HLSLExpression* expression1 = NULL;
            HLSLExpression* expression2 = NULL;
            if (!ParseBinaryExpression(_conditionalOpPriority, expression1) || !Expect(':') || !ParseBinaryExpression(_conditionalOpPriority, expression2))
            {
                return false;
            }

            // Make sure both cases have compatible types.
            if (GetTypeCastRank(m_tree, expression1->expressionType, expression2->expressionType) == -1)
            {
                const char* srcTypeName = GetTypeNameHLSL(expression2->expressionType);
                const char* dstTypeName = GetTypeNameHLSL(expression1->expressionType);
                m_tokenizer.Error("':' no possible conversion from from '%s' to '%s'", srcTypeName, dstTypeName);
                return false;
            }

            conditionalExpression->trueExpression  = expression1;
            conditionalExpression->falseExpression = expression2;
            conditionalExpression->expressionType  = expression1->expressionType;

            expression = conditionalExpression;
        }
        else
        {
            break;
        }

		if( needsEndParen )
		{
			if( !Expect( ')' ) )
				return false;
			needsEndParen = false;
		}
    }

    return !needsEndParen || Expect(')');
}

bool HLSLParser::ParsePartialConstructor(HLSLExpression*& expression, HLSLBaseType type, const char* typeName)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    HLSLConstructorExpression* constructorExpression = m_tree->AddNode<HLSLConstructorExpression>(fileName, line);
    constructorExpression->type.baseType = type;
    constructorExpression->type.typeName = typeName;
    int numArguments = 0;
    if (!ParseExpressionList(')', false, constructorExpression->argument, numArguments))
    {
        return false;
    }    
    constructorExpression->expressionType = constructorExpression->type;
    constructorExpression->expressionType.flags = HLSLTypeFlag_Const;
    expression = constructorExpression;
    return true;
}

bool HLSLParser::ParseTerminalExpression(HLSLExpression*& expression, bool& needsEndParen)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    needsEndParen = false;

    HLSLUnaryOp unaryOp;
    if (AcceptUnaryOperator(true, unaryOp))
    {
        HLSLUnaryExpression* unaryExpression = m_tree->AddNode<HLSLUnaryExpression>(fileName, line);
        unaryExpression->unaryOp = unaryOp;
        if (!ParseTerminalExpression(unaryExpression->expression, needsEndParen))
        {
            return false;
        }
        if (unaryOp == HLSLUnaryOp_BitNot)
        {
            if (unaryExpression->expression->expressionType.baseType < HLSLBaseType_FirstInteger || 
                unaryExpression->expression->expressionType.baseType > HLSLBaseType_LastInteger)
            {
                const char * typeName = GetTypeNameHLSL(unaryExpression->expression->expressionType);
                m_tokenizer.Error("unary '~' : no global operator found which takes type '%s' (or there is no acceptable conversion)", typeName);
                return false;
            }
        }
        if (unaryOp == HLSLUnaryOp_Not)
        {
            unaryExpression->expressionType = HLSLType(HLSLBaseType_Bool);
            
            // Propagate constness.
            unaryExpression->expressionType.flags = unaryExpression->expression->expressionType.flags & HLSLTypeFlag_Const;
        }
        else
        {
            unaryExpression->expressionType = unaryExpression->expression->expressionType;
        }
        expression = unaryExpression;
        return true;
    }
    
    // Expressions inside parenthesis or casts.
    if (Accept('('))
    {
        // Check for a casting operator.
        HLSLType type;
        if (AcceptType(false, type))
        {
            // This is actually a type constructor like (float2(...
            if (Accept('('))
            {
                needsEndParen = true;
                return ParsePartialConstructor(expression, type.baseType, type.typeName);
            }
            HLSLCastingExpression* castingExpression = m_tree->AddNode<HLSLCastingExpression>(fileName, line);
            castingExpression->type = type;
            expression = castingExpression;
            castingExpression->expressionType = type;
            return Expect(')') && ParseExpression(castingExpression->expression);
        }
        
        if (!ParseExpression(expression) || !Expect(')'))
        {
            return false;
        }
    }
    else
    {
        // Terminal values.
        float fValue = 0.0f;
        int   iValue = 0;
        
        // literals
        if (AcceptFloat(fValue))
        {
            HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
            literalExpression->type   = HLSLBaseType_Float;
            literalExpression->fValue = fValue;
            literalExpression->expressionType.baseType = literalExpression->type;
            literalExpression->expressionType.flags = HLSLTypeFlag_Const;
            expression = literalExpression;
            return true;
        }
		if( AcceptHalf( fValue ) )
		{
			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>( fileName, line );
			literalExpression->type = HLSLBaseType_Half;
			literalExpression->fValue = fValue;
			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;
			return true;
		}
        if (AcceptInt(iValue))
        {
            HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
            literalExpression->type   = HLSLBaseType_Int;
            literalExpression->iValue = iValue;
            literalExpression->expressionType.baseType = literalExpression->type;
            literalExpression->expressionType.flags = HLSLTypeFlag_Const;
            expression = literalExpression;
            return true;
        }
        // TODO: need uint, u/short
        
        // boolean
        if (Accept(HLSLToken_True))
        {
            HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
            literalExpression->type   = HLSLBaseType_Bool;
            literalExpression->bValue = true;
            literalExpression->expressionType.baseType = literalExpression->type;
            literalExpression->expressionType.flags = HLSLTypeFlag_Const;
            expression = literalExpression;
            return true;
        }
        if (Accept(HLSLToken_False))
        {
            HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
            literalExpression->type   = HLSLBaseType_Bool;
            literalExpression->bValue = false;
            literalExpression->expressionType.baseType = literalExpression->type;
            literalExpression->expressionType.flags = HLSLTypeFlag_Const;
            expression = literalExpression;
            return true;
        }

        // Type constructor.
        HLSLType type;
        if (AcceptType(/*allowVoid=*/false, type))
        {
            Expect('(');
            if (!ParsePartialConstructor(expression, type.baseType, type.typeName))
            {
                return false;
            }
        }
        else
        {
            HLSLIdentifierExpression* identifierExpression = m_tree->AddNode<HLSLIdentifierExpression>(fileName, line);
            if (!ExpectIdentifier(identifierExpression->name))
            {
                return false;
            }

            bool undeclaredIdentifier = false;
 
            const HLSLType* identifierType = FindVariable(identifierExpression->name, identifierExpression->global);
            if (identifierType != NULL)
            {
                identifierExpression->expressionType = *identifierType;
            }
            else
            {
                if (GetIsFunction(identifierExpression->name))
                {
                    // Functions are always global scope.
                    // TODO: what about member functions?
                    identifierExpression->global = true;
                }
                else
                {
                    undeclaredIdentifier = true;
                }
            }

            if (undeclaredIdentifier)
            {
                if (m_allowUndeclaredIdentifiers)
                {
                    HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
                    literalExpression->bValue = false;
                    literalExpression->type = HLSLBaseType_Bool;
                    literalExpression->expressionType.baseType = literalExpression->type;
                    literalExpression->expressionType.flags = HLSLTypeFlag_Const;
                    expression = literalExpression;
                }
                else
                {
                    m_tokenizer.Error("Undeclared identifier '%s'", identifierExpression->name);
                    return false;
                }
            }
            else {
                expression = identifierExpression;
            }
        }
    }

    bool done = false;
    while (!done)
    {
        done = true;

        // Post fix unary operator
        HLSLUnaryOp unaryOp2;
        while (AcceptUnaryOperator(false, unaryOp2))
        {
            HLSLUnaryExpression* unaryExpression = m_tree->AddNode<HLSLUnaryExpression>(fileName, line);
            unaryExpression->unaryOp = unaryOp2;
            unaryExpression->expression = expression;
            unaryExpression->expressionType = unaryExpression->expression->expressionType;
            expression = unaryExpression;
            done = false;
        }

        // Member access operator.
        while (Accept('.'))
        {
            HLSLMemberAccess* memberAccess = m_tree->AddNode<HLSLMemberAccess>(fileName, line);
            memberAccess->object = expression;
            if (!ExpectIdentifier(memberAccess->field))
            {
                return false;
            }
            
            if (!GetMemberType(expression->expressionType, memberAccess))
            {
                m_tokenizer.Error("Couldn't access '%s'", memberAccess->field);
                return false;
            }
            expression = memberAccess;
            done = false;
        }

        // Handle array access.
        while (Accept('['))
        {
            HLSLArrayAccess* arrayAccess = m_tree->AddNode<HLSLArrayAccess>(fileName, line);
            arrayAccess->array = expression;
            if (!ParseExpression(arrayAccess->index) || !Expect(']'))
            {
                return false;
            }

            if (expression->expressionType.baseType == HLSLBaseType_UserDefined)
            {
                // some buffer types (!IsGlobalFields) have array notation
                arrayAccess->expressionType.baseType = HLSLBaseType_UserDefined;
                arrayAccess->expressionType.typeName = expression->expressionType.typeName;
                arrayAccess->expressionType.array     = true;
                arrayAccess->expressionType.arraySize = NULL;
                
            }
            else if (expression->expressionType.array)
            {
                arrayAccess->expressionType = expression->expressionType;
                arrayAccess->expressionType.array     = false;
                arrayAccess->expressionType.arraySize = NULL;
            }
            else
            {
                switch (expression->expressionType.baseType)
                {
                case HLSLBaseType_Float2:
                case HLSLBaseType_Float3:
                case HLSLBaseType_Float4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Float;
                    break;
				case HLSLBaseType_Float2x2:
					arrayAccess->expressionType.baseType = HLSLBaseType_Float2;
					break;
                case HLSLBaseType_Float3x3:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Float3;
                    break;
                case HLSLBaseType_Float4x4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Float4;
                    break;

                case HLSLBaseType_Half2:
                case HLSLBaseType_Half3:
                case HLSLBaseType_Half4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Half;
                    break;
				case HLSLBaseType_Half2x2:
					arrayAccess->expressionType.baseType = HLSLBaseType_Half2;
					break;
                case HLSLBaseType_Half3x3:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Half3;
                    break;
                case HLSLBaseType_Half4x4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Half4;
                    break;

                case HLSLBaseType_Double2:
                case HLSLBaseType_Double3:
                case HLSLBaseType_Double4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Double;
                    break;
                case HLSLBaseType_Double2x2:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Double2;
                    break;
                case HLSLBaseType_Double3x3:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Double3;
                    break;
                case HLSLBaseType_Double4x4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Double4;
                    break;

                        
                case HLSLBaseType_Int2:
                case HLSLBaseType_Int3:
                case HLSLBaseType_Int4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Int;
                    break;
                case HLSLBaseType_Uint2:
                case HLSLBaseType_Uint3:
                case HLSLBaseType_Uint4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Uint;
                    break;
                case HLSLBaseType_Bool2:
                case HLSLBaseType_Bool3:
                case HLSLBaseType_Bool4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Bool;
                    break;
                case HLSLBaseType_Ushort2:
                case HLSLBaseType_Ushort3:
                case HLSLBaseType_Ushort4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Ushort;
                    break;
                case HLSLBaseType_Short2:
                case HLSLBaseType_Short3:
                case HLSLBaseType_Short4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Short;
                    break;
                case HLSLBaseType_Ulong2:
                case HLSLBaseType_Ulong3:
                case HLSLBaseType_Ulong4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Ulong;
                    break;
                case HLSLBaseType_Long2:
                case HLSLBaseType_Long3:
                case HLSLBaseType_Long4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Long;
                    break;
                        
                // TODO: u/char
                default:
                    m_tokenizer.Error("array, matrix, vector, or indexable object type expected in index expression");
                    return false;
                }
            }

            expression = arrayAccess;
            done = false;
        }

        // Handle function calls. Note, HLSL functions aren't like C function
        // pointers -- we can only directly call on an identifier, not on an
        // expression.
        if (Accept('('))
        {
            HLSLFunctionCall* functionCall = m_tree->AddNode<HLSLFunctionCall>(fileName, line);
            done = false;
            if (!ParseExpressionList(')', false, functionCall->argument, functionCall->numArguments))
            {
                return false;
            }
            
            if (expression->nodeType != HLSLNodeType_IdentifierExpression)
            {
                m_tokenizer.Error("Expected function identifier");
                return false;
            }
            
            const HLSLIdentifierExpression* identifierExpression = static_cast<const HLSLIdentifierExpression*>(expression);
            const HLSLFunction* function = MatchFunctionCall( functionCall, identifierExpression->name );
            if (function == NULL)
            {
                return false;
            }
            
            functionCall->function = function;
            functionCall->expressionType = function->returnType;
            expression = functionCall;
        }

    }
    return true;

}

bool HLSLParser::ParseExpressionList(int endToken, bool allowEmptyEnd, HLSLExpression*& firstExpression, int& numExpressions)
{
    numExpressions = 0;
    HLSLExpression* lastExpression = NULL;
    while (!Accept(endToken))
    {
        if (CheckForUnexpectedEndOfStream(endToken))
        {
            return false;
        }
        if (numExpressions > 0 && !Expect(','))
        {
            return false;
        }
        // It is acceptable for the final element in the initialization list to
        // have a trailing comma in some cases, like array initialization such as {1, 2, 3,}
        if (allowEmptyEnd && Accept(endToken))
        {
            break;
        }
        HLSLExpression* expression = NULL;
        if (!ParseExpression(expression))
        {
            return false;
        }
        if (firstExpression == NULL)
        {
            firstExpression = expression;
        }
        else
        {
            lastExpression->nextExpression = expression;
        }
        lastExpression = expression;
        ++numExpressions;
    }
    return true;
}

bool HLSLParser::ParseArgumentList(HLSLArgument*& firstArgument, int& numArguments, int& numOutputArguments)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();
        
    HLSLArgument* lastArgument = NULL;
    numArguments = 0;

    while (!Accept(')'))
    {
        if (CheckForUnexpectedEndOfStream(')'))
        {
            return false;
        }
        if (numArguments > 0 && !Expect(','))
        {
            return false;
        }

        HLSLArgument* argument = m_tree->AddNode<HLSLArgument>(fileName, line);

        // what is unifor modifier ?
        if (Accept(HLSLToken_Uniform))     { argument->modifier = HLSLArgumentModifier_Uniform; }
        
        else if (Accept(HLSLToken_In))     { argument->modifier = HLSLArgumentModifier_In;      }
        else if (Accept(HLSLToken_Out))    { argument->modifier = HLSLArgumentModifier_Out;     }
        else if (Accept(HLSLToken_InOut))  { argument->modifier = HLSLArgumentModifier_Inout;   }
        else if (Accept(HLSLToken_Const))  { argument->modifier = HLSLArgumentModifier_Const;   }

        if (!ExpectDeclaration(/*allowUnsizedArray=*/true, argument->type, argument->name))
        {
            return false;
        }

        DeclareVariable( argument->name, argument->type );

        // Optional semantic.
        if (Accept(':') && !ExpectIdentifier(argument->semantic))
        {
            return false;
        }

        if (Accept('=') && !ParseExpression(argument->defaultValue))
        {
            // @@ Print error!
            return false;
        }

        if (lastArgument != NULL)
        {
            lastArgument->nextArgument = argument;
        }
        else
        {
            firstArgument = argument;
        }
        lastArgument = argument;

        ++numArguments;
        if (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout)
        {
            ++numOutputArguments;
        }
    }
    return true;
}

/*
bool HLSLParser::ParseSamplerState(HLSLExpression*& expression)
{
    if (!Expect(HLSLToken_SamplerState))
    {
        return false;
    }

    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    HLSLSamplerState* samplerState = m_tree->AddNode<HLSLSamplerState>(fileName, line);

    if (!Expect('{'))
    {
        return false;
    }

    HLSLStateAssignment* lastStateAssignment = NULL;

    // Parse state assignments.
    while (!Accept('}'))
    {
        if (CheckForUnexpectedEndOfStream('}'))
        {
            return false;
        }

        HLSLStateAssignment* stateAssignment = NULL;
        if (!ParseStateAssignment(stateAssignment, true, false))
        {
            return false;
        }
        ASSERT(stateAssignment != NULL);
        if (lastStateAssignment == NULL)
        {
            samplerState->stateAssignments = stateAssignment;
        }
        else
        {
            lastStateAssignment->nextStateAssignment = stateAssignment;
        }
        lastStateAssignment = stateAssignment;
        samplerState->numStateAssignments++;
    }

    expression = samplerState;
    return true;
}

bool HLSLParser::ParseTechnique(HLSLStatement*& statement)
{
    if (!Accept(HLSLToken_Technique)) {
        return false;
    }

    const char* techniqueName = NULL;
    if (!ExpectIdentifier(techniqueName))
    {
        return false;
    }

    if (!Expect('{'))
    {
        return false;
    }

    HLSLTechnique* technique = m_tree->AddNode<HLSLTechnique>(GetFileName(), GetLineNumber());
    technique->name = techniqueName;

    //m_techniques.PushBack(technique);

    HLSLPass* lastPass = NULL;

    // Parse state assignments.
    while (!Accept('}'))
    {
        if (CheckForUnexpectedEndOfStream('}'))
        {
            return false;
        }

        HLSLPass* pass = NULL;
        if (!ParsePass(pass))
        {
            return false;
        }
        ASSERT(pass != NULL);
        if (lastPass == NULL)
        {
            technique->passes = pass;
        }
        else
        {
            lastPass->nextPass = pass;
        }
        lastPass = pass;
        technique->numPasses++;
    }

    statement = technique;
    return true;
}

bool HLSLParser::ParsePass(HLSLPass*& pass)
{
    if (!Accept(HLSLToken_Pass)) {
        return false;
    }

    // Optional pass name.
    const char* passName = NULL;
    AcceptIdentifier(passName);

    if (!Expect('{'))
    {
        return false;
    }

    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    pass = m_tree->AddNode<HLSLPass>(fileName, line);
    pass->name = passName;

    HLSLStateAssignment* lastStateAssignment = NULL;

    // Parse state assignments.
    while (!Accept('}'))
    {
        if (CheckForUnexpectedEndOfStream('}'))
        {
            return false;
        }

        HLSLStateAssignment* stateAssignment = NULL;
        if (!ParseStateAssignment(stateAssignment, false, false))
        {
            return false;
        }
        ASSERT(stateAssignment != NULL);
        if (lastStateAssignment == NULL)
        {
            pass->stateAssignments = stateAssignment;
        }
        else
        {
            lastStateAssignment->nextStateAssignment = stateAssignment;
        }
        lastStateAssignment = stateAssignment;
        pass->numStateAssignments++;
    }
    return true;
}


bool HLSLParser::ParsePipeline(HLSLStatement*& statement)
{
    if (!Accept("pipeline")) {
        return false;
    }

    // Optional pipeline name.
    const char* pipelineName = NULL;
    AcceptIdentifier(pipelineName);

    if (!Expect('{'))
    {
        return false;
    }

    HLSLPipeline* pipeline = m_tree->AddNode<HLSLPipeline>(GetFileName(), GetLineNumber());
    pipeline->name = pipelineName;

    HLSLStateAssignment* lastStateAssignment = NULL;

    // Parse state assignments.
    while (!Accept('}'))
    {
        if (CheckForUnexpectedEndOfStream('}'))
        {
            return false;
        }

        HLSLStateAssignment* stateAssignment = NULL;
        if (!ParseStateAssignment(stateAssignment, false, true))
        {
            return false;
        }
        ASSERT(stateAssignment != NULL);
        if (lastStateAssignment == NULL)
        {
            pipeline->stateAssignments = stateAssignment;
        }
        else
        {
            lastStateAssignment->nextStateAssignment = stateAssignment;
        }
        lastStateAssignment = stateAssignment;
        pipeline->numStateAssignments++;
    }

    statement = pipeline;
    return true;
}


const EffectState* GetEffectState(const char* name, bool isSamplerState, bool isPipeline)
{
    const EffectState* validStates = effectStates;
    int count = sizeof(effectStates)/sizeof(effectStates[0]);
    
    if (isPipeline)
    {
        validStates = pipelineStates;
        count = sizeof(pipelineStates) / sizeof(pipelineStates[0]);
    }

    if (isSamplerState)
    {
        validStates = samplerStates;
        count = sizeof(samplerStates)/sizeof(samplerStates[0]);
    }

    // Case insensitive comparison.
    for (int i = 0; i < count; i++)
    {
        if (String_EqualNoCase(name, validStates[i].name)) 
        {
            return &validStates[i];
        }
    }

    return NULL;
}

static const EffectStateValue* GetStateValue(const char* name, const EffectState* state)
{
    // Case insensitive comparison.
    for (int i = 0; ; i++) 
    {
        const EffectStateValue & value = state->values[i];
        if (value.name == NULL) break;

        if (String_EqualNoCase(name, value.name)) 
        {
            return &value;
        }
    }

    return NULL;
}


bool HLSLParser::ParseStateName(bool isSamplerState, bool isPipelineState, const char*& name, const EffectState *& state)
{
    if (m_tokenizer.GetToken() != HLSLToken_Identifier)
    {
        char near[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(near);
        m_tokenizer.Error("Syntax error: expected identifier near '%s'", near);
        return false;
    }

    state = GetEffectState(m_tokenizer.GetIdentifier(), isSamplerState, isPipelineState);
    if (state == NULL)
    {
        m_tokenizer.Error("Syntax error: unexpected identifier '%s'", m_tokenizer.GetIdentifier());
        return false;
    }

    m_tokenizer.Next();
    return true;
}

bool HLSLParser::ParseColorMask(int& mask)
{
    mask = 0;

    do {
        if (m_tokenizer.GetToken() == HLSLToken_IntLiteral) {
            mask |= m_tokenizer.GetInt();
        }
        else if (m_tokenizer.GetToken() == HLSLToken_Identifier) {
            const char * ident = m_tokenizer.GetIdentifier();
            const EffectStateValue * stateValue = colorMaskValues;
            while (stateValue->name != NULL) {
                if (String_EqualNoCase(stateValue->name, ident)) {
                    mask |= stateValue->value;
                    break;
                }
                ++stateValue;
            }
        }
        else {
            return false;
        }
        m_tokenizer.Next();
    } while (Accept('|'));

    return true;
}

bool HLSLParser::ParseStateValue(const EffectState * state, HLSLStateAssignment* stateAssignment)
{
    const bool expectsExpression = state->values == colorMaskValues;
    const bool expectsInteger = state->values == integerValues;
    const bool expectsFloat = state->values == floatValues;
    const bool expectsBoolean = state->values == booleanValues;

    if (!expectsExpression && !expectsInteger && !expectsFloat && !expectsBoolean) 
    {
        if (m_tokenizer.GetToken() != HLSLToken_Identifier)
        {
            char near[HLSLTokenizer::s_maxIdentifier];
            m_tokenizer.GetTokenName(near);
            m_tokenizer.Error("Syntax error: expected identifier near '%s'", near);
            stateAssignment->iValue = 0;
            return false;
        }
    }

    if (state->values == NULL)
    {
        if (strcmp(m_tokenizer.GetIdentifier(), "compile") != 0)
        {
            m_tokenizer.Error("Syntax error: unexpected identifier '%s' expected compile statement", m_tokenizer.GetIdentifier());
            stateAssignment->iValue = 0;
            return false;
        }

        // @@ Parse profile name, function name, argument expressions.

        // Skip the rest of the compile statement.
        while(m_tokenizer.GetToken() != ';')
        {
            m_tokenizer.Next();
        }
    }
    else {
        if (expectsInteger)
        {
            if (!AcceptInt(stateAssignment->iValue))
            {
                m_tokenizer.Error("Syntax error: expected integer near '%s'", m_tokenizer.GetIdentifier());
                stateAssignment->iValue = 0;
                return false;
            }
        }
        else if (expectsFloat)
        {
            if (!AcceptFloat(stateAssignment->fValue))
            {
                m_tokenizer.Error("Syntax error: expected float near '%s'", m_tokenizer.GetIdentifier());
                stateAssignment->iValue = 0;
                return false;
            }
        }
        else if (expectsBoolean)
        {
            const EffectStateValue * stateValue = GetStateValue(m_tokenizer.GetIdentifier(), state);

            if (stateValue != NULL)
            {
                stateAssignment->iValue = stateValue->value;

                m_tokenizer.Next();
            }
            else if (AcceptInt(stateAssignment->iValue))
            {
                stateAssignment->iValue = (stateAssignment->iValue != 0);
            }
            else {
                m_tokenizer.Error("Syntax error: expected bool near '%s'", m_tokenizer.GetIdentifier());
                stateAssignment->iValue = 0;
                return false;
            }
        }
        else if (expectsExpression)
        {
            if (!ParseColorMask(stateAssignment->iValue))
            {
                m_tokenizer.Error("Syntax error: expected color mask near '%s'", m_tokenizer.GetIdentifier());
                stateAssignment->iValue = 0;
                return false;
            }
        }
        else 
        {
            // Expect one of the allowed values.
            const EffectStateValue * stateValue = GetStateValue(m_tokenizer.GetIdentifier(), state);

            if (stateValue == NULL)
            {
                m_tokenizer.Error("Syntax error: unexpected value '%s' for state '%s'", m_tokenizer.GetIdentifier(), state->name);
                stateAssignment->iValue = 0;
                return false;
            }

            stateAssignment->iValue = stateValue->value;

            m_tokenizer.Next();
        }
    }

    return true;
}

bool HLSLParser::ParseStateAssignment(HLSLStateAssignment*& stateAssignment, bool isSamplerState, bool isPipelineState)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    stateAssignment = m_tree->AddNode<HLSLStateAssignment>(fileName, line);

    const EffectState * state;
    if (!ParseStateName(isSamplerState, isPipelineState, stateAssignment->stateName, state)) {
        return false;
    }

    //stateAssignment->name = m_tree->AddString(m_tokenizer.GetIdentifier());
    stateAssignment->stateName = state->name;
    stateAssignment->d3dRenderState = state->d3drs;

    if (!Expect('=')) {
        return false;
    }

    if (!ParseStateValue(state, stateAssignment)) {
        return false;
    }

    if (!Expect(';')) {
        return false;
    }

    return true;
}
*/

bool HLSLParser::ParseAttributeList(HLSLAttribute*& firstAttribute)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();
    
    HLSLAttribute * lastAttribute = firstAttribute;
    do {
        const char * identifier = NULL;
        if (!ExpectIdentifier(identifier)) {
            return false;
        }

        HLSLAttribute * attribute = m_tree->AddNode<HLSLAttribute>(fileName, line);
        
        if (String_Equal(identifier, "unroll"))
            attribute->attributeType = HLSLAttributeType_Unroll;
        else if (String_Equal(identifier, "flatten"))
            attribute->attributeType = HLSLAttributeType_Flatten;
        else if (String_Equal(identifier, "branch"))
            attribute->attributeType = HLSLAttributeType_Branch;
        else if (String_Equal(identifier, "nofastmath"))
            attribute->attributeType = HLSLAttributeType_NoFastMath;
        
        // @@ parse arguments, () not required if attribute constructor has no arguments.

        if (firstAttribute == NULL)
        {
            firstAttribute = attribute;
        }
        else
        {
            lastAttribute->nextAttribute = attribute;
        }
        lastAttribute = attribute;
        
    } while(Accept(','));

    return true;
}

// Attributes can have all these forms:
//   [A] statement;
//   [A,B] statement;
//   [A][B] statement;
// These are not supported yet:
//   [A] statement [B];
//   [A()] statement;
//   [A(a)] statement;
bool HLSLParser::ParseAttributeBlock(HLSLAttribute*& attribute)
{
    HLSLAttribute ** lastAttribute = &attribute;
    while (*lastAttribute != NULL) { lastAttribute = &(*lastAttribute)->nextAttribute; }

    if (!Accept('['))
    {
        return false;
    }

    // Parse list of attribute constructors.
    ParseAttributeList(*lastAttribute);

    if (!Expect(']'))
    {
        return false;
    }

    // Parse additional [] blocks.
    ParseAttributeBlock(*lastAttribute);

    return true;
}

/* never completed
bool HLSLParser::ParseStage(HLSLStatement*& statement)
{
    if (!Accept("stage"))
    {
        return false;
    }

    // Required stage name.
    const char* stageName = NULL;
    if (!ExpectIdentifier(stageName))
    {
        return false;
    }

    if (!Expect('{'))
    {
        return false;
    }

    HLSLStage* stage = m_tree->AddNode<HLSLStage>(GetFileName(), GetLineNumber());
    stage->name = stageName;

    BeginScope();

    HLSLType voidType(HLSLBaseType_Void);
    if (!Expect('{') || !ParseBlock(stage->statement, voidType))
    {
        return false;
    }

    EndScope();

    // @@ To finish the stage definition we should traverse the statements recursively (including function calls) and find all the input/output declarations.

    statement = stage;
    return true;
}
*/



bool HLSLParser::Parse(HLSLTree* tree)
{
    m_tree = tree;
    
    HLSLRoot* root = m_tree->GetRoot();
    HLSLStatement* lastStatement = NULL;

    while (!Accept(HLSLToken_EndOfStream))
    {
        HLSLStatement* statement = NULL;
        if (!ParseTopLevel(statement))
        {
            return false;
        }
        if (statement != NULL)
        {   
            if (lastStatement == NULL)
            {
                root->statement = statement;
            }
            else
            {
                lastStatement->nextStatement = statement;
            }
            lastStatement = statement;
            while (lastStatement->nextStatement) lastStatement = lastStatement->nextStatement;
        }
    }
    return true;
}

bool HLSLParser::AcceptTypeModifier(int& flags)
{
    if (Accept(HLSLToken_Const))
    {
        flags |= HLSLTypeFlag_Const;
        return true;
    }
    else if (Accept(HLSLToken_Static))
    {
        flags |= HLSLTypeFlag_Static;
        return true;
    }
    else if (Accept(HLSLToken_Uniform))
    {
        //flags |= HLSLTypeFlag_Uniform;      // @@ Ignored.
        return true;
    }
    else if (Accept(HLSLToken_Inline))
    {
        //flags |= HLSLTypeFlag_Uniform;      // @@ Ignored. In HLSL all functions are inline.
        return true;
    }
    /*else if (Accept("in"))
    {
        flags |= HLSLTypeFlag_Input;
        return true;
    }
    else if (Accept("out"))
    {
        flags |= HLSLTypeFlag_Output;
        return true;
    }*/

    // Not an usage keyword.
    return false;
}

bool HLSLParser::AcceptInterpolationModifier(int& flags)
{
    if (Accept("linear"))
    { 
        flags |= HLSLTypeFlag_Linear; 
        return true;
    }
    else if (Accept("centroid"))
    { 
        flags |= HLSLTypeFlag_Centroid;
        return true;
    }
    else if (Accept("nointerpolation"))
    {
        flags |= HLSLTypeFlag_NoInterpolation;
        return true;
    }
    else if (Accept("noperspective"))
    {
        flags |= HLSLTypeFlag_NoPerspective;
        return true;
    }
    else if (Accept("sample"))
    {
        flags |= HLSLTypeFlag_Sample;
        return true;
    }

    return false;
}


bool HLSLParser::AcceptType(bool allowVoid, HLSLType& type/*, bool acceptFlags*/)
{
    //if (type.flags != NULL)
    {
        type.flags = 0;
        while(AcceptTypeModifier(type.flags) || AcceptInterpolationModifier(type.flags)) {}
    }

    int token = m_tokenizer.GetToken();

    if (token == HLSLToken_Comment)
    {
        // TODO: should this advance the tokenizer?
        // m_tokenizer.Next();
        
        type.baseType = HLSLBaseType_Comment;
        return true;
    }
    
    // Check built-in types.
    type.baseType = HLSLBaseType_Void;
    switch (token)
    {
    case HLSLToken_Float:
        type.baseType = HLSLBaseType_Float;
        break;
    case HLSLToken_Float2:      
        type.baseType = HLSLBaseType_Float2;
        break;
    case HLSLToken_Float3:
        type.baseType = HLSLBaseType_Float3;
        break;
    case HLSLToken_Float4:
        type.baseType = HLSLBaseType_Float4;
        break;
            
	case HLSLToken_Float2x2:
		type.baseType = HLSLBaseType_Float2x2;
		break;
    case HLSLToken_Float3x3:
        type.baseType = HLSLBaseType_Float3x3;
        break;
    case HLSLToken_Float4x4:
        type.baseType = HLSLBaseType_Float4x4;
        break;
            
    case HLSLToken_Half:
        type.baseType = HLSLBaseType_Half;
        break;
    case HLSLToken_Half2:      
        type.baseType = HLSLBaseType_Half2;
        break;
    case HLSLToken_Half3:
        type.baseType = HLSLBaseType_Half3;
        break;
    case HLSLToken_Half4:
        type.baseType = HLSLBaseType_Half4;
        break;
            
	case HLSLToken_Half2x2:
		type.baseType = HLSLBaseType_Half2x2;
		break;
    case HLSLToken_Half3x3:
        type.baseType = HLSLBaseType_Half3x3;
        break;
    case HLSLToken_Half4x4:
        type.baseType = HLSLBaseType_Half4x4;
        break;

    case HLSLToken_Bool:
        type.baseType = HLSLBaseType_Bool;
        break;
	case HLSLToken_Bool2:
		type.baseType = HLSLBaseType_Bool2;
		break;
	case HLSLToken_Bool3:
		type.baseType = HLSLBaseType_Bool3;
		break;
	case HLSLToken_Bool4:
		type.baseType = HLSLBaseType_Bool4;
		break;
            
    case HLSLToken_Int:
        type.baseType = HLSLBaseType_Int;
        break;
    case HLSLToken_Int2:
        type.baseType = HLSLBaseType_Int2;
        break;
    case HLSLToken_Int3:
        type.baseType = HLSLBaseType_Int3;
        break;
    case HLSLToken_Int4:
        type.baseType = HLSLBaseType_Int4;
        break;
            
    case HLSLToken_Uint:
        type.baseType = HLSLBaseType_Uint;
        break;
    case HLSLToken_Uint2:
        type.baseType = HLSLBaseType_Uint2;
        break;
    case HLSLToken_Uint3:
        type.baseType = HLSLBaseType_Uint3;
        break;
    case HLSLToken_Uint4:
        type.baseType = HLSLBaseType_Uint4;
        break;
         
    case HLSLToken_Ushort:
        type.baseType = HLSLBaseType_Ushort;
        break;
    case HLSLToken_Ushort2:
        type.baseType = HLSLBaseType_Ushort2;
        break;
    case HLSLToken_Ushort3:
        type.baseType = HLSLBaseType_Ushort3;
        break;
    case HLSLToken_Ushort4:
        type.baseType = HLSLBaseType_Ushort4;
        break;
        
    case HLSLToken_Short:
        type.baseType = HLSLBaseType_Short;
        break;
    case HLSLToken_Short2:
        type.baseType = HLSLBaseType_Short2;
        break;
    case HLSLToken_Short3:
        type.baseType = HLSLBaseType_Short3;
        break;
    case HLSLToken_Short4:
        type.baseType = HLSLBaseType_Short4;
        break;
            
    // Textures (TODO: could have baseType be texture, with subtype like buffer)
    case HLSLToken_Texture2D:
        type.baseType = HLSLBaseType_Texture2D;
        break;
    case HLSLToken_Texture2DArray:
        type.baseType = HLSLBaseType_Texture2DArray;
        break;
    case HLSLToken_Texture3D:
        type.baseType = HLSLBaseType_Texture3D;
        break;
    case HLSLToken_TextureCube:
        type.baseType = HLSLBaseType_TextureCube;
        break;
    case HLSLToken_Texture2DMS:
        type.baseType = HLSLBaseType_Texture2DMS;
        break;
    case HLSLToken_TextureCubeArray:
        type.baseType = HLSLBaseType_TextureCubeArray;
        break;
       
    case HLSLToken_Depth2D:
        type.baseType = HLSLBaseType_Depth2D;
        break;
    case HLSLToken_Depth2DArray:
        type.baseType = HLSLBaseType_Depth2DArray;
        break;
    case HLSLToken_DepthCube:
        type.baseType = HLSLBaseType_DepthCube;
        break;
            
    case HLSLToken_RWTexture2D:
        type.baseType = HLSLBaseType_RWTexture2D;
        break;
            
    // samplers
    case HLSLToken_SamplerState:
        type.baseType = HLSLBaseType_SamplerState;
        break;
    case HLSLToken_SamplerComparisonState:
        type.baseType = HLSLBaseType_SamplerComparisonState;
        break;
            
    // older constants
    case HLSLToken_CBuffer:
    case HLSLToken_TBuffer:
        // might make these BufferGlobals?
        type.baseType = HLSLBaseType_Buffer;
        break;
            
    // SSBO
    case HLSLToken_StructuredBuffer:
    case HLSLToken_RWStructuredBuffer:
    case HLSLToken_ByteAddressBuffer:
    case HLSLToken_RWByteAddressBuffer:
    case HLSLToken_ConstantBuffer:
        type.baseType = HLSLBaseType_Buffer;
        break;
    }
    if (type.baseType != HLSLBaseType_Void)
    {
        m_tokenizer.Next();
        
        if (IsTextureType(type.baseType))
        {
            // Parse optional sampler type.
            if (Accept('<'))
            {
                token = m_tokenizer.GetToken();
                
                // TODO: need more format types
                // TODO: double, u/long, and other types
                if (token >= HLSLToken_Float && token <= HLSLToken_Float4)
                {
                    // TODO: code only tests if texture formatType exactly matches
                    // when looking for Intrinsics, need to fix that before changing
                    // this.
                    
                    type.formatType = HLSLBaseType_Float;
                    // (HLSLBaseType)(HLSLBaseType_Float + (token - HLSLToken_Float));
                }
                else if (token >= HLSLToken_Half && token <= HLSLToken_Half4)
                {
                    type.formatType =  HLSLBaseType_Half;
                    // (HLSLBaseType)(HLSLBaseType_Half + (token - HLSLToken_Half));
                }
                else
                {
                    m_tokenizer.Error("Expected half or float format type on texture.");
                    return false;
                }
                m_tokenizer.Next();
                
                if (!Expect('>'))
                {
                    return false;
                }
            }
        }
        return true;
    }

    if (allowVoid && Accept(HLSLToken_Void))
    {
        type.baseType = HLSLBaseType_Void;
        return true;
    }
    if (token == HLSLToken_Identifier)
    {
        const char* identifier = m_tree->AddString( m_tokenizer.GetIdentifier() );
        if (FindUserDefinedType(identifier) != NULL)
        {
            m_tokenizer.Next();
            
            type.baseType = HLSLBaseType_UserDefined;
            type.typeName = identifier;
            return true;
        }
    }
    return false;
}

bool HLSLParser::ExpectType(bool allowVoid, HLSLType& type)
{
    if (!AcceptType(allowVoid, type))
    {
        m_tokenizer.Error("Expected type");
        return false;
    }
    return true;
}

bool HLSLParser::AcceptDeclaration(bool allowUnsizedArray, HLSLType& type, const char*& name)
{
    if (!AcceptType(/*allowVoid=*/false, type))
    {
        return false;
    }

    if (!ExpectIdentifier(name))
    {
        // TODO: false means we didn't accept a declaration and we had an error!
        return false;
    }
    // Handle array syntax.
    if (Accept('['))
    {
        type.array = true;
        // Optionally allow no size to the specified for the array.
        if (Accept(']') && allowUnsizedArray)
        {
            return true;
        }
        if (!ParseExpression(type.arraySize) || !Expect(']'))
        {
            return false;
        }
    }
    return true;
}

bool HLSLParser::ExpectDeclaration(bool allowUnsizedArray, HLSLType& type, const char*& name)
{
    if (!AcceptDeclaration(allowUnsizedArray, type, name))
    {
        m_tokenizer.Error("Expected declaration");
        return false;
    }
    return true;
}

const HLSLStruct* HLSLParser::FindUserDefinedType(const char* name) const
{
    // Pointer comparison is sufficient for strings since they exist in the
    // string pool.
    for (int i = 0; i < m_userTypes.GetSize(); ++i)
    {
        if (m_userTypes[i]->name == name)
        {
            return m_userTypes[i];
        }
    }
    return NULL;
}

bool HLSLParser::CheckForUnexpectedEndOfStream(int endToken)
{
    if (Accept(HLSLToken_EndOfStream))
    {
        char what[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(endToken, what);
        m_tokenizer.Error("Unexpected end of file while looking for '%s'", what);
        return true;
    }
    return false;
}

int HLSLParser::GetLineNumber() const
{
    return m_tokenizer.GetLineNumber();
}

const char* HLSLParser::GetFileName()
{
    return m_tree->AddString( m_tokenizer.GetFileName() );
}

void HLSLParser::BeginScope()
{
    // Use NULL as a sentinel that indices a new scope level.
    Variable& variable = m_variables.PushBackNew();
    variable.name = NULL;
}

void HLSLParser::EndScope()
{
    int numVariables = m_variables.GetSize() - 1;
    while (m_variables[numVariables].name != NULL)
    {
        --numVariables;
        ASSERT(numVariables >= 0);
    }
    m_variables.Resize(numVariables);
}

const HLSLType* HLSLParser::FindVariable(const char* name, bool& global) const
{
    for (int i = m_variables.GetSize() - 1; i >= 0; --i)
    {
        if (m_variables[i].name == name)
        {
            global = (i < m_numGlobals);
            return &m_variables[i].type;
        }
    }
    return NULL;
}

const HLSLFunction* HLSLParser::FindFunction(const char* name) const
{
    for (int i = 0; i < m_functions.GetSize(); ++i)
    {
        if (m_functions[i]->name == name)
        {
            return m_functions[i];
        }
    }
    return NULL;
}

static bool AreTypesEqual(HLSLTree* tree, const HLSLType& lhs, const HLSLType& rhs)
{
    return GetTypeCastRank(tree, lhs, rhs) == 0;
}

static bool AreArgumentListsEqual(HLSLTree* tree, HLSLArgument* lhs, HLSLArgument* rhs)
{
    while (lhs && rhs)
    {
        if (!AreTypesEqual(tree, lhs->type, rhs->type))
            return false;

        if (lhs->modifier != rhs->modifier)
            return false;

        if (lhs->semantic != rhs->semantic || lhs->sv_semantic != rhs->sv_semantic)
            return false;

        lhs = lhs->nextArgument;
        rhs = rhs->nextArgument;
    }

    return lhs == NULL && rhs == NULL;
}

const HLSLFunction* HLSLParser::FindFunction(const HLSLFunction* fun) const
{
    for (int i = 0; i < m_functions.GetSize(); ++i)
    {
        if (m_functions[i]->name == fun->name &&
            AreTypesEqual(m_tree, m_functions[i]->returnType, fun->returnType) &&
            AreArgumentListsEqual(m_tree, m_functions[i]->argument, fun->argument))
        {
            return m_functions[i];
        }
    }
    return NULL;
}

void HLSLParser::DeclareVariable(const char* name, const HLSLType& type)
{
    if (m_variables.GetSize() == m_numGlobals)
    {
        ++m_numGlobals;
    }
    Variable& variable = m_variables.PushBackNew();
    variable.name = name;
    variable.type = type;
}

bool HLSLParser::GetIsFunction(const char* name) const
{
    // check user defined functions
    for (int i = 0; i < m_functions.GetSize(); ++i)
    {
        // == is ok here because we're passed the strings through the string pool.
        if (m_functions[i]->name == name)
        {
            return true;
        }
    }
    
    // see if it's an intrinsic
    const auto& it = _intrinsicRangeMap.find(name);
    return it != _intrinsicRangeMap.end();
}

const HLSLFunction* HLSLParser::MatchFunctionCall(const HLSLFunctionCall* functionCall, const char* name)
{
    const HLSLFunction* matchedFunction     = NULL;

    //int  numArguments           = functionCall->numArguments;
    int  numMatchedOverloads    = 0;
    bool nameMatches            = false;

    // Get the user defined functions with the specified name.
    for (int i = 0; i < m_functions.GetSize(); ++i)
    {
        const HLSLFunction* function = m_functions[i];
        if (function->name == name)
        {
            nameMatches = true;
            
            CompareFunctionsResult result = CompareFunctions( m_tree, functionCall, function, matchedFunction );
            if (result == Function1Better)
            {
                matchedFunction = function;
                numMatchedOverloads = 1;
            }
            else if (result == FunctionsEqual)
            {
                ++numMatchedOverloads;
            }
        }
    }

    // Get the intrinsic functions with the specified name.
    const auto& iter = _intrinsicRangeMap.find(name);
    if (iter != _intrinsicRangeMap.end())
    {
        Range range = iter->second;
        for (int i = 0; i < range.count; ++i)
        {
            uint32_t idx = range.start + i;
            const HLSLFunction* function = &_intrinsics[idx].function;
            
            ASSERT(String_Equal(function->name, name));
                   
            nameMatches = true;
            
            CompareFunctionsResult result = CompareFunctions( m_tree, functionCall, function, matchedFunction );
            if (result == Function1Better)
            {
                matchedFunction = function;
                numMatchedOverloads = 1;
            }
            else if (result == FunctionsEqual)
            {
                ++numMatchedOverloads;
            }
        }
    }
    
    if (matchedFunction != NULL && numMatchedOverloads > 1)
    {
        // Multiple overloads match.
        m_tokenizer.Error("'%s' %d overloads have similar conversions", name, numMatchedOverloads);
        return NULL;
    }
    else if (matchedFunction == NULL)
    {
        if (nameMatches)
        {
            m_tokenizer.Error("'%s' no overloaded function matched all of the arguments", name);
        }
        else
        {
            m_tokenizer.Error("Undeclared identifier '%s'", name);
        }
    }

    return matchedFunction;
}

inline bool IsSwizzle(char c)
{
    return c == 'x' || c == 'y' || c == 'z' || c ==  'w' ||
           c == 'r' || c == 'g' || c == 'b' || c ==  'a';
}

bool HLSLParser::GetMemberType(const HLSLType& objectType, HLSLMemberAccess * memberAccess)
{
    const char* fieldName = memberAccess->field;

    HLSLBaseType baseType = objectType.baseType;

    // pull field from struct
    if (baseType == HLSLBaseType_UserDefined)
    {
        const HLSLStruct* structure = FindUserDefinedType( objectType.typeName );
        ASSERT(structure != NULL);
        if (structure == NULL)
            return false;
        
        const HLSLStructField* field = structure->field;
        while (field != NULL)
        {
            if (field->name == fieldName)
            {
                memberAccess->expressionType = field->type;
                return true;
            }
            field = field->nextField;
        }

        return false;
    }

    if (baseTypeDescriptions[objectType.baseType].numericType == NumericType_NaN)
    {
        // Currently we don't have an non-numeric types that allow member access.
        return false;
    }

    int swizzleLength = 0;

    if (IsScalarType(baseType) || IsVectorType(baseType))
    {
        // Check for a swizzle on the scalar/vector types.
        for (int i = 0; fieldName[i] != 0; ++i)
        {
            if (!IsSwizzle(fieldName[i]))
            {
                m_tokenizer.Error("Invalid swizzle '%s'", fieldName);
                return false;
            }
            ++swizzleLength;
        }
        ASSERT(swizzleLength > 0);
        if (swizzleLength == 0)
            return false;
    }
    else if (IsMatrixType(baseType))
    {

        // Check for a matrix element access (e.g. _m00 or _11)

        const char* n = fieldName;
        while (n[0] == '_')
        {
            ++n;
            int base = 1;
            if (n[0] == 'm')
            {
                base = 0;
                ++n;
            }
            if (!isdigit(n[0]) || !isdigit(n[1]))
            {
                m_tokenizer.Error("Invalid matrix digit");
                return false;
            }

            int r = (n[0] - '0') - base;
            int c = (n[1] - '0') - base;
            if (r >= baseTypeDescriptions[objectType.baseType].height)
            {
                m_tokenizer.Error("Invalid matrix dimension %d", r);
                return false;
            }
            if (c >= baseTypeDescriptions[objectType.baseType].numComponents)
            {
                m_tokenizer.Error("Invalid matrix dimension %d", c);
                return false;
            }
            ++swizzleLength;
            n += 2;

        }

        if (n[0] != 0)
        {
            return false;
        }

    }
    else
    {
        return false;
    }

    if (swizzleLength > 4)
    {
        m_tokenizer.Error("Invalid swizzle '%s'", fieldName);
        return false;
    }
 
    switch (baseTypeDescriptions[objectType.baseType].numericType)
    {
    case NumericType_Float:
        memberAccess->expressionType.baseType = (HLSLBaseType)(HLSLBaseType_Float + swizzleLength - 1);
        break;
    case NumericType_Half:
        memberAccess->expressionType.baseType = (HLSLBaseType)(HLSLBaseType_Half + swizzleLength - 1);
        break;
    case NumericType_Double:
        memberAccess->expressionType.baseType = (HLSLBaseType)(HLSLBaseType_Double + swizzleLength - 1);
        break;
        
    case NumericType_Int:
        memberAccess->expressionType.baseType = (HLSLBaseType)(HLSLBaseType_Int + swizzleLength - 1);
        break;
    case NumericType_Uint:
        memberAccess->expressionType.baseType = (HLSLBaseType)(HLSLBaseType_Uint + swizzleLength - 1);
            break;
    case NumericType_Bool:
        memberAccess->expressionType.baseType = (HLSLBaseType)(HLSLBaseType_Bool + swizzleLength - 1);
            break;
    case NumericType_Short:
        memberAccess->expressionType.baseType = (HLSLBaseType)(HLSLBaseType_Short + swizzleLength - 1);
            break;
    case NumericType_Ushort:
        memberAccess->expressionType.baseType = (HLSLBaseType)(HLSLBaseType_Ushort + swizzleLength - 1);
            break;
    case NumericType_Long:
        memberAccess->expressionType.baseType = (HLSLBaseType)(HLSLBaseType_Long + swizzleLength - 1);
            break;
    case NumericType_Ulong:
        memberAccess->expressionType.baseType = (HLSLBaseType)(HLSLBaseType_Ulong + swizzleLength - 1);
            break;
    // TODO: u/char
    default:
        ASSERT(false);
    }

    memberAccess->swizzle = true;
    
    return true;
}

}
