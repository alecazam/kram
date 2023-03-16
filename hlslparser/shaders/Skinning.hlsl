

// TODO: syntax highlighting as Metal doesn't work
// This isn't including header, but that doesn't seem to fix either.
// Need HLSL plugin for Xcode.  files have to be a part of project
// to get syntax highlight but even that doesn't work.  And ref folders
// also don't work since the files aren't part of the project.
//
// https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst

// setup specialization
// HLSL: at beginning
// [[vk::constant_id(0)]] const int   specConstInt  = 1;
// [[vk::constant_id(1)]] const bool  specConstBool  = true;
//
// MSL: at end
// constant bool a [[function_constant(0)]];
// constant int  a [[function_constant(1)]]; // 0.. 64K-1

// subpass input, and SubpassLoad() calls
// [[vk::input_attachment_index(i)]] SubpassInput input;
// class SubpassInput<T> { T SubpassLoad(); };
// class SubpassInputMS<T> { T SubpassLoad(int sampleIndex); };

// push constants
// [[vk::push_constant]]

// descriptors and arg buffers
// [[vk::binding(X[, Y])]] and [[vk::counter_binding(X)]]

// [[vk::image_format("rgba8")]]
// RWBuffer<float4> Buf;


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

struct InputVS
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 blendWeights : BLENDWEIGHT;
    uint4  blendIndices : BLENDINDICES;
    
    short4 testShort : TANGENT;
    ushort4 testUShort : BITANGENT;
};

// half below won't work on many Adreno/Mali without inputOutput
struct OutputVS
{
    float4  position : SV_Position;
    half    diffuse : COLOR;
    float2  uv : TEXCOORD0;
};

// try to mondernize to ConstantBuffer
struct UniformsStruct
{
    float4x4 skinTfms[256];
    half3    lightDir;
    float4x4 worldToClipTfm;
};
ConstantBuffer<UniformsStruct> uniforms : register(b0);

// can have 14x 64K limit to each cbuffer, 128 tbuffers,
 
// Structured buffers
struct StructuredStruct
{
    half3    lightDir;
    float4x4 worldToClipTfm;
};

StructuredBuffer<StructuredStruct> bufferTest0 : register(t2);


Texture2D<half4> tex : register(t1);
SamplerState samplerClamp : register(s0);

float4x4 DoSkinTfm(UniformsStruct uniforms, float4 blendWeights, uint4 blendIndices)
{
    // Can use mul or * here
    //float4x4 skinTfm = blendWeights[0] * uniforms.skinTfms[blendIndices[0]];
    float4x4 skinTfm = mul(blendWeights[0], uniforms.skinTfms[blendIndices[0]]);
    
    for (uint i = 1; i < 4; ++i)
    {
        //skinTfm += blendWeights[i] * uniforms.skinTfms[blendIndices[i]];
        skinTfm += mul(blendWeights[i], uniforms.skinTfms[blendIndices[i]]);
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
    
    float4x4 skinTfm = uniforms.skinTfms[ instanceNum ];
    
    // MSL doesn't like *=
    skinTfm = skinTfm * DoSkinTfm(uniforms, input.blendWeights, input.blendIndices);

    // Skin to world space
    float3 position = mul(input.position, skinTfm).xyz;
    half3 normal = half3(mul(float4(input.normal,0.0), skinTfm).xyz);
        
    // Output stuff
    output.position = mul(float4(position, 1.0), uniforms.worldToClipTfm);
  
    // glslc fix
    // output.diffuse = half( dot(lightDir, normal) );
    // DXC
    output.diffuse = dot(uniforms.lightDir, normal);

    // TODO: test structured buffer
    StructuredStruct item = bufferTest0[0];
    output.diffuse *= item.lightDir.x;
   
    // test the operators
    output.diffuse *= output.diffuse;
    output.diffuse += output.diffuse;
    output.diffuse -= output.diffuse;
    output.diffuse /= output.diffuse;
    
    output.uv = input.uv;
    
    // only for Vulkan/MSL, DX12 can't control per vertex shader
    //pointSize = 1;
    
    return output;
}

// Want to pass OutputVS as input, but DXC can't handle the redefinition
// in the same file.  So have to keep OutputVS and InputPS in sync.
// this can include position on MSL, but not on HLSL.
// Also for mobile the type should be higher precision to avoid banding.
// So half from VS, but float in PS.
//struct InputPS
//{
//    float4  position : SV_Position;
//    half    diffuse : COLOR;
//    float2  uv : TEXCOORD0;
//};

struct OutputPS
{
    half4 color : SV_Target0;
};

// Note: don't write as void SkinningPS(VS_OUTPUT input, out PS_OUTPUT output)
// this is worse MSL codegen.

// TODO: SV_Position differs from Vulkan/MSL in that pos.w = w and not 1/w like gl_FragCoord and [[position]].
// DXC has a setting to invert w.

OutputPS SkinningPS(OutputVS input,
     bool isFrontFace: SV_IsFrontFace
    )
{
    OutputPS output;

    // This is hard to reflect with combined tex/sampler
    // have way more textures than samplers on mobile.
    //float4 color = tex2D(tex, input.uv);
    //half4 color = half4(1.0h);
    half4 color = SampleH(tex, samplerClamp, input.uv);
    
    // TODO: move to DX10 style, but MSL codegen is trickier then
    // since it wraps the vars
    // half4 color = tex.Sample(pointSampler, input.uv);
    
    // just to test min3 support
    // glslc fix
    //half c = half( min3(color.r, color.g, color.b) );
    //color.rgb = half3(c,c,c); // can't use half3(c)!
    // DXC
    color.rgb = min3(color.r, color.g, color.b);

    color.rgb *= input.diffuse;
    output.color = color;

    return output;
}
