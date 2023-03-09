

// TODO: syntax highlighting as Metal doesn't work
// This isn't including header, but that doesn't seem to fix either.
// Need HLSL plugin for Xcode.  files have to be a part of project
// to get syntax highlight but even that doesn't work.  And ref folders
// also don't work since the files aren't part of the project.
//
// https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst

// setup specialization
// HLSL:
// [[vk::constant_id(0)]] const bool  specConstInt  = 1;
// [[vk::constant_id(1)]] const bool  specConstBool  = true;
// MSL:
// constant bool a [[function_constant(0)]];
// constant int a [[function_constant(1)]]; // 0.. 64K

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
// D3DCOLORtoUBYTE4: Decodes a D3DCOLOR packed DWORD to a float4.
// Note the swizzle, and I don't want an int4.  I need to encode.
// This is achieved by performing int4(input.zyxw * 255.002) using SPIR-V OpVectorShuffle, OpVectorTimesScalar, and OpConvertFToS, respectively.
// https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_Pack_Unpack_Intrinsics.html

// These all pack to too large of data structores.
// also they don't handle gamma.
//
// int16_t4 unpack_s8s16(int8_t4_packed packedVal);        // Sign Extended
// uint16_t4 unpack_u8u16(uint8_t4_packed packedVal);      // Non-Sign Extended
// int32_t4 unpack_s8s32(int8_t4_packed packedVal);        // Sign Extended
// uint32_t4 unpack_u8u32(uint8_t4_packed packedVal);      // Non-Sign Extended
// uint8_t4_packed pack_u8(uint32_t4 unpackedVal);         // Pack lower 8 bits, drop unused bits
// int8_t4_packed pack_s8(int32_t4  unpackedVal);          // Pack lower 8 bits, drop unused bits
//
// uint8_t4_packed pack_u8(uint16_t4 unpackedVal);         // Pack lower 8 bits, drop unused bits
// int8_t4_packed pack_s8(int16_t4  unpackedVal);          // Pack lower 8 bits, drop unused bits
//
// uint8_t4_packed pack_clamp_u8(int32_t4  unpackedVal);   // Pack and Clamp [0, 255]
// int8_t4_packed pack_clamp_s8(int32_t4  unpackedVal);    // Pack and Clamp [-128, 127]
//
// uint8_t4_packed pack_clamp_u8(int16_t4  unpackedVal);   // Pack and Clamp [0, 255]
// int8_t4_packed pack_clamp_s8(int16_t4  unpackedVal);    // Pack and Clamp [-128, 127]
//
// have uint16_t/int16_t support in 6.2.  Need to add as type into parser.
//
// cbuffer are std140, and ssbo are std430 arrangment.  Affects arrays.
// or -fvk-use-dx-layout vs. -fvk-use-gl-layout vs. -fvk-use-scalar-layout.
// Scalar layout rules introduced via VK_EXT_scalar_block_layout, which basically aligns
// all aggregrate types according to their elements' natural alignment.
// They can be enabled by -fvk-use-scalar-layout.
// see table.  Vulkan can't use DX layout yet.
//
// cbuffer vs. ConstantBuffer<T> myCBuffer;

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

struct OutputVS
{
    float4  position : SV_Position;
    half    diffuse : COLOR;
    float2  uv : TEXCOORD0;
    float   pointSize : PSIZE;
};

cbuffer Uniforms
{
    // should these be float3x4?
    float4x4 skinTfms[256];
    half3    lightDir;
    float4x4 worldToClipTfm;
};

// TODO: split up tex/sampler, update texture calls to DX10
// defines combined tex_texture/tex_sampler
sampler2D<half> tex;

float4x4 DoSkinTfm(float4x4 skinTfms[256], float4 blendWeights, uint4 blendIndices)
{
    float4x4 skinTfm = blendWeights[0] * skinTfms[blendIndices[0]];
       
    for (uint i = 1; i < 4; ++i)
    {
        skinTfm += blendWeights[i] * skinTfms[blendIndices[i]];
    }

    return skinTfm;
}

// TODO: These don't compile for spv despite setting extension
//  don't know what semantic to set?
// uint vertexBase : BASE_VERTEX,
// uint instanceBase : BASE_INSTANCE,

// TODO: fix ability to comment these out inside SkinningVS inputs
    
// TODO: can't yet have commented out inputs or tokenizer fails
OutputVS SkinningVS(InputVS input,
    uint instanceID : SV_InstanceID,
    uint vertexID : SV_VertexID
)
{
    OutputVS output;

    // TODO: this needs to declare array param as constant for
    // MSL function call.  Pointers can't be missing working space.
    
    //float4x4 skinTfm = 0;
       // DoSkinTfm(skinTfms, input.blendWeights, input.blendIndices);

    // this is just to use these
    uint vertexNum = vertexID;
    uint instanceNum = instanceID;

    // uint vertexNum = vertexBase + vertexID;
    // uint instanceNum = instanceBase + instanceID;

    instanceNum += vertexNum;
    
    float4x4 skinTfm = skinTfms[ instanceNum ];
    

    // Skin to world space
    float3 position = mul(input.position, skinTfm).xyz;
    half3 normal = half3(mul(float4(input.normal,0.0), skinTfm).xyz);
        
    // Output stuff
    output.position = mul(float4(position, 1.0), worldToClipTfm);
  
    // glslc fix
    // output.diffuse = half( dot(lightDir, normal) );
    // DXC
    output.diffuse = dot(lightDir, normal);

    // test the operators
    output.diffuse *= output.diffuse;
    output.diffuse += output.diffuse;
    output.diffuse -= output.diffuse;
    output.diffuse /= output.diffuse;
    
    output.uv = input.uv;
    
    // only for Vulkan/MSL, DX12 can't control per vertex shader
    output.pointSize = 1;
    
    return output;
}

// Want to pass OutputVS as input, but DXC can't handle the redefinition
// in the same file.  So have to keep OutputVS and InputPS in sync.
// this can include position on MSL, but not on HLSL.
// Also for mobile the type should be higher precision to avoid banding.
// So half from VS, but float in PS.
struct InputPS
{
    half    diffuse : COLOR;
    float2  uv : TEXCOORD0;
};

struct OutputPS
{
    half4 color : SV_Target0;
};

// Note: don't write as void SkinningPS(VS_OUTPUT input, out PS_OUTPUT output)
// this is worse MSL codegen.

// TODO: SV_Position differs from Vulkan/MSL in that pos.w = w and not 1/w like gl_FragCoord and [[position]].
// DXC has a setting to invert w.

OutputPS SkinningPS(InputPS input,
     float4  position : SV_Position,
     bool isFrontFace: SV_IsFrontFace
    )
{
    OutputPS output;

    // This is hard to reflect with combined tex/sampler
    // have way more textures than samplers on mobile.
    //float4 color = tex2D(tex, input.uv);
    half4 color = tex2D(tex, input.uv);
    
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
