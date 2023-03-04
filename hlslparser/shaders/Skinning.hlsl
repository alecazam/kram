struct InputVS
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 blendWeights : BLENDWEIGHT;
    uint4  blendIndices : BLENDINDICES;
};

// On half:
// HLSL only added back half support in 6.2
// GLSL has a float16_t type that is barely supported on mobile.
// Half inputOuput not support on Nvidia/Adreno
// Also interpolating half leads to banding
// On TBDR, half inputOutput only way to save parameter buffer
// space.  Can output half from VS, but define as float
// input to PS to avoid banding.
// Note Adreno doesn't suport half stored in blocks either.
// What a mess!

struct OutputVS
{
    float4  position : POSITION;
    half    diffuse : COLOR;
    float2  uv : TEXCOORD0;
};

cbuffer Uniforms 
{
    // should these be float3x4?
    float4x4 skinTfms[256];
    half3    lightDir;
    float4x4 worldToClipTfm;
};

// old DX9 style
// sampler2D tex;

// mod from HLSLParser for half
// can specify half, float, or none (float)
sampler2D<half> tex;

// new style in DX10 to move to non-paired sampler/texture
// Texture2D<half4> tex;
// SamplerState pointSampler;

// and in MSL
// texture2d<half> tex;
// sampler pointSampler;

// TODO: using column matrices for MSL/PSSL/GLSL constency
// so switch from premul to postmul in shader.  And used float3x4
// but need to add support to parsers/generators.

float4x4 DoSkinTfm(float4x4 skinTfms[256], float4 blendWeights, uint4 blendIndices)
{
    float4x4 skinTfm = blendWeights[0] * skinTfms[blendIndices[0]];
       
    for (uint i = 1; i < 4; ++i)
    {
        skinTfm += blendWeights[i] * skinTfms[blendIndices[i]];
    }

    return skinTfm;
}

OutputVS SkinningVS(InputVS input)
{
    OutputVS output;

    // TODO: this needs to declare array param as constant for
    // MSL function call.  Pointers can't be missing working space.
    
    float4x4 skinTfm = 0;
       // DoSkinTfm(skinTfms, input.blendWeights, input.blendIndices);

    // Skin to world space
    float3 position = mul(input.position, skinTfm).xyz;
    half3 normal = half3(mul(float4(input.normal,0.0), skinTfm).xyz);
        
    // Output stuff
    output.position = mul(float4(position, 1.0), worldToClipTfm);
    output.diffuse  = dot(lightDir, normal);
    output.uv = input.uv;

    return output;
}

// Want to pass OutputVS as input, but DXC can't handle the redefinition
// in the same file.  So have to keep OutputVS and InputPS in sync.
struct InputPS
{
    // this can include position on MSL, but not on HLSL
    half    diffuse : COLOR;
    float2  uv : TEXCOORD0;
};

struct OutputPS
{
    half4 color : COLOR0;
};

// Note: don't write as void SkinningPS(VS_OUTPUT input, out PS_OUTPUT output)
// this is worse MSL codegen.

OutputPS SkinningPS(InputPS input)
{
    OutputPS output;
   
    // This is hard to reflect with combined tex/sampler
    // have way more textures than samplers on mobile.
    //float4 color = tex2D(tex, input.uv);
    half4 color = tex2DH(tex, input.uv);
    
    // TODO: move to DX10 style, but MSL codegen is trickier then
    // since it wraps the vars
    // half4 color = tex.Sample(pointSampler, input.uv);
    
    // just to test min3 support
    color.rgb = min3(color.r, color.g, color.b);
    
    color.rgb *= input.diffuse;
    output.color = color;

    return output;
}
