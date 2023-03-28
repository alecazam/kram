
#include "ShaderHLSL.h"

// TODO: syntax highlighting as Metal doesn't work
// This isn't including header, but that doesn't seem to fix either.
// Need HLSL plugin for Xcode.  files have to be a part of project
// to get syntax highlight but even that doesn't work.  And ref folders
// also don't work since the files aren't part of the project.
//
// https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst

// setup variants
// HLSL: specialization constants marked at beginning
// [[vk::constant_id(0)]] const int   specConstInt  = 1;
// [[vk::constant_id(1)]] const bool  specConstBool  = true;
//
// MSL: function constants marked at end
// constant bool a [[function_constant(0)]];
// constant int  a [[function_constant(1)]]; // 0.. 64K-1

// This is for tile shaders
// subpass input, and SubpassLoad() calls
// [[vk::input_attachment_index(i)]] SubpassInput input;
// class SubpassInput<T> { T SubpassLoad(); };
// class SubpassInputMS<T> { T SubpassLoad(int sampleIndex); };

// push constants (DONE)
// [[vk::push_constant]]

// descriptors and arg buffers
// [[vk::binding(X[, Y])]] and [[vk::counter_binding(X)]]

// tagging the format of buffers/textures, since HLSL can't represent
// [[vk::image_format("rgba8")]]
// RWBuffer<float4> Buf;
//
// [[vk::image_format("rg16f")]]
// RWTexture2D<float2> Tex;

// structure buffer only supports 2/4B access, ByteAddressBuffer only 4B increments
// #ifdef __spirv__
// [[vk::binding(X, Y), vk::counter_binding(Z)]]
// #endif
// StructuredBuffer<Struct> ssbo;

// No u/int8_t or u/char in HLSL.
// There is int64_t/uint46_t in MSL.
// D3DCOLORtoUBYTE4: Decodes a D3DCOLOR packed DWORD to a float4.
// Note the swizzle, and I don't want an int4.  I need to encode.
// This is achieved by performing int4(input.zyxw * 255.002) using SPIR-V OpVectorShuffle, OpVectorTimesScalar, and OpConvertFToS, respectively.

// Have uint16_t/int16_t support in 6.2.
//
// cbuffer are std140, and ssbo are std430 arrangment.  Affects arrays.
// or -fvk-use-dx-layout vs. -fvk-use-gl-layout vs. -fvk-use-scalar-layout.
// Scalar layout rules introduced via VK_EXT_scalar_block_layout, which basically aligns
// all aggregrate types according to their elements' natural alignment.
// They can be enabled by -fvk-use-scalar-layout.
// see table.  Vulkan can't use DX layout yet.
//
// This is 6.1 change so constants can be array indexed
// And it also reduces the quantity of globals throughout and ties to MSL better.
// cbuffer vs. ConstantBuffer<T> myCBuffer[10];

// struct VSInput {
//   [[vk::location(0)]] float4 pos  : POSITION;
//   [[vk::location(1)]] float3 norm : NORMAL;
// };

// 6.2 adds templated load, so can
//ByteAddressBuffer buffer;
//
//float f1 = buffer.Load<float>(idx);
//half2 h2 = buffer.Load<half2>(idx);
//uint16_t4 i4 = buffer.Load<uint16_t4>(idx);

// MSL rule;
// If a vertex function writes to one or more buffers or textures, its return type must be void.

// no preprocessor to do this yet, so have to add functions
// can't seem to have overloads like this with same name
inline float4 mulr(float4 v, float4x4 m) { return mul(m,v); }
inline float3 mulr(float3 v, float3x3 m) { return mul(m,v); }
inline half3  mulr(half3  v, half3x3  m) { return mul(m,v); }

struct InputVS
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 blendWeights : BLENDWEIGHT;
    uint4  blendIndices : BLENDINDICES;
};

// these were just tests
//short4 testShort : TANGENT;
//ushort4 testUShort : BITANGENT;

struct OutputVS
{
    float4  position : SV_Position;
    halfio  diffuse : COLOR;
    float2  uv : TEXCOORD0;
};

struct OutputPS
{
    half4io color : SV_Target0;
};

static const uint kMaxSkinTfms = 256;

// Note: SkinTfms makes MSL air shader 60K bigger at 256,
// so may want to limit large hardcoded arrays.  It's 61K @256, and 7.8K @1.
// Doesn't seem to affect spriv at 4K.
// GameShaders.metallib is 33K, and Zip of Spirv is 6K.
struct UniformsStruct
{
    float4x4 skinTfms[kMaxSkinTfms];
    
    half3st  lightDir;
    float4x4 worldToClipTfm;
};
ConstantBuffer<UniformsStruct> uniforms : register(b0);

// can have 14x 64K limit to each cbuffer, 128 tbuffers,
 
// Structured buffers
//struct StructuredStruct
//{
//    half3st  lightDir;
//    float4x4 worldToClipTfm;
//};
//
//StructuredBuffer<StructuredStruct> bufferTest0 : register(t2);


Texture2D<half4> tex : register(t1);
SamplerState samplerClamp : register(s0);

float4x4 DoSkinTfm(/*UniformsStruct uniforms,*/ float4 blendWeights, uint4 blendIndices)
{
    // weight the transforms, could use half4x4 and 101010A2 for weights
    float4x4 skinTfm = blendWeights[0] * uniforms.skinTfms[blendIndices[0]];
    
    for (uint i = 1; i < 4; ++i)
    {
        skinTfm += blendWeights[i] * uniforms.skinTfms[blendIndices[i]];
    }
    
    return skinTfm;
}

// TODO: this isn't working, wanted to share OutputVS, so left out of that
// and moved to SkinningVS as output.  But something thinks out is type.
// , out float pointSize : PSIZE

// TODO: fix ability to comment out inputs
OutputVS SkinningVS(InputVS input,
    uint vertexBase : BASEVERTEX,
    uint instanceBase : BASEINSTANCE,
    uint instanceID : SV_InstanceID,
    uint vertexID : SV_VertexID
)
{
    OutputVS output;

    // TODO: this needs to declare array param as constant for
    // MSL function call.  Pointers can't be missing working space.

    // this is just to use these
    //uint vertexNum = vertexID;
    //uint instanceNum = instanceID;

    uint vertexNum = vertexBase + vertexID;
    uint instanceNum = instanceBase + instanceID;

    instanceNum += vertexNum;
    
    // not using above
    
    // float4x4 skinTfm = uniforms.skinTfms[ instanceNum ];
    
    float4x4 skinTfm = DoSkinTfm(input.blendWeights, input.blendIndices);

    // Skin to world space
    float3 normal = mulr(input.normal, (float3x3)skinTfm);
    normal = mulr(normal, (float3x3)uniforms.worldToClipTfm);
    
    // Output stuff
    float4 worldPos = mulr(input.position, skinTfm);
    output.position = mulr(worldPos, uniforms.worldToClipTfm);
  
    output.diffuse = (halfio)dot((half3)uniforms.lightDir, (half3)normal);

    // test structured buffer
    // StructuredStruct item = bufferTest0[0];
    //output.diffuse *= item.lightDir.x;
   
    // test the operators
//    output.diffuse *= output.diffuse;
//    output.diffuse += output.diffuse;
//    output.diffuse -= output.diffuse;
//    output.diffuse /= output.diffuse;
    
    output.uv = input.uv;
    
    // only for Vulkan/MSL, DX12 can't control per vertex shader
    //pointSize = 1;
    
    return output;
}



// Note: don't write as void SkinningPS(VS_OUTPUT input, out PS_OUTPUT output)
// this is worse MSL codegen.

// TODO: SV_Position differs from Vulkan/MSL in that pos.w = w and not 1/w like gl_FragCoord and [[position]].
// DXC has a setting to invert w.

OutputPS SkinningPS(OutputVS input,
     bool isFrontFace: SV_IsFrontFace
    )
{
    half4 color = tex.Sample(samplerClamp, input.uv);
    color.rgb = min3(color.r, color.g, color.b);
    color.rgb *= (half)input.diffuse;
    
    OutputPS output;
    output.color = (half4io)color;
    return output;
}
