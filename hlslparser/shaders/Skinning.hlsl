
// TODO: syntax highlighting as Metal doesn't work
// This isn't including header, but that doesn't seem to fix either.
// Need HLSL plugin for Xcode

struct InputVS
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 blendWeights : BLENDWEIGHT;
    uint4  blendIndices : BLENDINDICES;
};

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
  
    // glslc fix
    // output.diffuse = half( dot(lightDir, normal) );
    // DXC
    output.diffuse = dot(lightDir, normal);

    output.uv = input.uv;

    return output;
}

// Want to pass OutputVS as input, but DXC can't handle the redefinition
// in the same file.  So have to keep OutputVS and InputPS in sync.
// this can include position on MSL, but not on HLSL
struct InputPS
{
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
