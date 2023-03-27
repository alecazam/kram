//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// from here https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Multithreading/src/shaders.hlsl

// TODO: also want to be able to share functions in a multientry point file
// so could move these out as static functions from the namespace in Metal.
// Right now if used, they get replicated into each class, but that handles
// any global use of texture+sampler/buffer.

// Textures/samplers/buffers are globals to all shader within the file.
// But have to pair texture + sampler.  In MSL, can combine
// into an argument buffer which holds all that.
// Vulkan has descriptor sets.

Depth2D<float4> shadowMap : register(t0);
Texture2D<half4> diffuseMap : register(t1);
Texture2D<half4> normalMap : register(t2);

SamplerState sampleWrap : register(s0);
SamplerComparisonState shadowMapSampler : register(s1);

// #define didn't compile due to lack of preprocesor
static const int NUM_LIGHTS = 3;
//static const float SHADOW_DEPTH_BIAS = 0.00005;

struct LightState
{
    float3 position;
    float3 direction;
    float4 color;
    float4 falloff;
    float4x4 viewProj;
};

struct SceneConstantBuffer
{
    float4x4 model;
    float4x4 viewProj;
    float4 ambientColor;
    bool sampleShadowMap;
    LightState lights[NUM_LIGHTS];
};

// SM 6.1
ConstantBuffer<SceneConstantBuffer> scene : register(b0);

// no preprocessor to do this yet, so have to add functions
// can't seem to have overloads like this with same name
inline float4 mulr(float4 v, float4x4 m) { return mul(m,v); }
inline float3 mulr(float3 v, float3x3 m) { return mul(m,v); }
inline half3  mulr(half3  v, half3x3  m) { return mul(m,v); }


// TODO: also have this form, where can index into
// ConstantBuffer<SceneConstantBuffer> scene[10] : register(b0);

// TODO: normal/tangent should be half3/4, but use 101010A2 in buffer
struct InputVS
{
    float3 position : SV_Position;
    float2 uv : TEXCOORD0;
    
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
};

// TODO: normal/tangent should be half3/4 to cut parameter buffer
// but that will break Nvidia/Adreno.
struct OutputVS
{
    float4 position : SV_Position;
    float4 worldPos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
};

// TODO: should output half4
struct OutputPS
{
    float4 target0 : SV_Target0;
};

// Sample normal map, convert to signed, apply tangent-to-world space transform.
half3 CalcPerPixelNormal(float2 texcoord, half3 vertNormal, half3 vertTangent, half bitanSign)
{
    half3 vertBinormal = normalize(cross(vertTangent, vertNormal)) * bitanSign;
    half3x3 tangentSpaceToWorldSpace = half3x3(vertTangent, vertBinormal, vertNormal);

    // Compute per-pixel normal.
    half3 bumpNormal = SampleH(normalMap, sampleWrap, texcoord).xyz;
    
    // TODO: let snorm format handle, and do z reconstruct
    bumpNormal = 2.0h * bumpNormal - 1.0h;

    return mul(bumpNormal, tangentSpaceToWorldSpace);
}

// Diffuse lighting calculation, with angle and distance falloff.
half4 CalcLightingColor(float3 lightPos, half3 lightDir, half4 lightColor, float4 falloffs, float3 posWorld, half3 perPixelNormal)
{
    float3 lightToPixelUnNormalized = posWorld - lightPos;

    // Dist falloff = 0 at falloffs.x, 1 at falloffs.x - falloffs.y
    float dist = length(lightToPixelUnNormalized);

    half distFalloff = (half)saturate((falloffs.x - dist) / falloffs.y);

    // Normalize from here on.
    half3 lightToPixelNormalized = (half3)normalize(lightToPixelUnNormalized);

    // Angle falloff = 0 at falloffs.z, 1 at falloffs.z - falloffs.w
    //half3 lightDirHalf = (half3)normalize(lightDir);
    half cosAngle = dot(lightToPixelNormalized, lightDir);
    half angleFalloff = saturate((cosAngle - (half)falloffs.z) / (half)falloffs.w);

    // Diffuse contribution.
    half dotNL = saturate(-dot(lightToPixelNormalized, perPixelNormal));

    return lightColor * (dotNL * distFalloff * angleFalloff);
}

// Test how much pixel is in shadow, using 2x2 percentage-closer filtering.
half CalcUnshadowedAmountPCF2x2(float4 posWorld, float4x4 viewProj)
{
    // Compute pixel position in light space.
    float4 lightSpacePos = posWorld;
    lightSpacePos = mulr(lightSpacePos, viewProj);
    
    // need to reject before division (assuming revZ, infZ)
    if (lightSpacePos.z > lightSpacePos.w)
        return 1.0h;
    
    // near/w for persp, z/1 for ortho
    lightSpacePos.xyz /= lightSpacePos.w;

    // Use HW filtering
    return (half)SampleCmp(shadowMap, shadowMapSampler, lightSpacePos);
}

OutputVS SampleVS(InputVS input)
{
    OutputVS output;

    float4 newPosition = float4(input.position, 1.0);

    // TODO: need to flip these muls for column matrix
    newPosition = mulr(newPosition, scene.model);

    output.worldPos = newPosition;

    newPosition = mulr(newPosition, scene.viewProj);
   
    output.position = newPosition;
    output.uv = input.uv;
    
    // TODO: preserve bitan sign in tangent.w down to PS
    // need transformed to world space too?
    // this only works if only uniform scale and invT on normal
    output.normal = mulr(float4(input.normal, 0.0), scene.model).xyz;
    output.tangent.xyz = mulr(float4(input.tangent.xyz, 0.0), scene.model).xyz;
    output.tangent.w = input.tangent.w;
    
    return output;
}

OutputPS SamplePS(OutputVS input)
{
    // Compute tangent frame.
    half3 normal = (half3)normalize(input.normal);
    half3 tangent = (half3)normalize(input.tangent.xyz);
    half  bitanSign = (half)input.tangent.w;
    
    half4 diffuseColor = SampleH(diffuseMap, sampleWrap, input.uv);
    half3 pixelNormal = CalcPerPixelNormal(input.uv, normal, tangent, bitanSign);
    half4 totalLight = (half4)scene.ambientColor;

    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        LightState light = scene.lights[i];
        half4 lightPass = CalcLightingColor(light.position, (half3)normalize(light.direction), (half4)light.color, light.falloff, input.worldPos.xyz, pixelNormal);
        
        // only single light shadow map
        if (i == 0 && scene.sampleShadowMap)
        {
            lightPass *= CalcUnshadowedAmountPCF2x2(input.worldPos, light.viewProj);
        }
        totalLight += lightPass;
    }

    OutputPS output;
    output.target0 = (float4)(diffuseColor * saturate(totalLight));
    return output;
}


