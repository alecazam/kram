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

// TODO: also have this form, where can index into
// ConstantBuffer<SceneConstantBuffer> scene[10] : register(b0);

struct InputVS
{
    float3 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangent : TANGENT;
};

struct OutputVS
{
    float4 position : SV_Position;
    float4 worldpos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

// Don't need this
//struct InputPS
//{
//    float4 position : SV_Position;
//    float4 worldpos : TEXCOORD0;
//    float2 uv : TEXCOORD1;
//    float3 normal : NORMAL;
//    float3 tangent : TANGENT;
//};


//--------------------------------------------------------------------------------------
// Sample normal map, convert to signed, apply tangent-to-world space transform.
//--------------------------------------------------------------------------------------
half3 CalcPerPixelNormal(float2 texcoord, half3 vertNormal, half3 vertTangent)
{
    // Compute tangent frame.
    vertNormal = normalize(vertNormal);
    vertTangent = normalize(vertTangent);

    half3 vertBinormal = normalize(cross(vertTangent, vertNormal));
    half3x3 tangentSpaceToWorldSpace = half3x3(vertTangent, vertBinormal, vertNormal);

    // Compute per-pixel normal.
    half3 bumpNormal = SampleH(normalMap, sampleWrap, texcoord).xyz;
    bumpNormal = 2.0h * bumpNormal - 1.0h;

    return mul(bumpNormal, tangentSpaceToWorldSpace);
}

//--------------------------------------------------------------------------------------
// Diffuse lighting calculation, with angle and distance falloff.
//--------------------------------------------------------------------------------------
half4 CalcLightingColor(float3 lightPos, float3 lightDir, half4 lightColor, float4 falloffs, float3 posWorld, half3 perPixelNormal)
{
    float3 lightToPixelUnNormalized = posWorld - lightPos;

    // Dist falloff = 0 at falloffs.x, 1 at falloffs.x - falloffs.y
    float dist = length(lightToPixelUnNormalized);

    half distFalloff = (half)saturate((falloffs.x - dist) / falloffs.y);

    // Normalize from here on.
    half3 lightToPixelNormalized = (half3)normalize(lightToPixelUnNormalized);

    // Angle falloff = 0 at falloffs.z, 1 at falloffs.z - falloffs.w
    half3 lightDirHalf = (half3)normalize(lightDir);
    half cosAngle = dot(lightToPixelNormalized, lightDirHalf);
    half angleFalloff = saturate((cosAngle - (half)falloffs.z) / (half)falloffs.w);

    // Diffuse contribution.
    half dotNL = saturate(-dot(lightToPixelNormalized, perPixelNormal));

    return lightColor * (dotNL * distFalloff * angleFalloff);
}

//--------------------------------------------------------------------------------------
// Test how much pixel is in shadow, using 2x2 percentage-closer filtering.
//--------------------------------------------------------------------------------------
half CalcUnshadowedAmountPCF2x2(int lightIndex, float4 posWorld, float4x4 viewProj)
{
    // Compute pixel position in light space.
    float4 lightSpacePos = posWorld;
    lightSpacePos = mul(lightSpacePos, viewProj);
    
    // need to reject before division (assuming revZ, infZ)
    if (lightSpacePos.z > lightSpacePos.w)
        return 1.0h;
    
    // near/w for persp, z/1 for ortho
    lightSpacePos.xyz /= lightSpacePos.w;

/*
    // TODO: do all the flip and scaling and bias in the proj, not in shader
    lightSpacePos.xy *= 0.5;
    lightSpacePos.xy += 0.5;
    
    // Translate from homogeneous coords to texture coords.
    lightSpacePos.y = 1.0 - lightSpacePos.y;

    // Depth bias to avoid pixel self-shadowing.
    lightSpacePos.z -= SHADOW_DEPTH_BIAS;
*/
    
    // Use HW filtering
    return (half)SampleCmp(shadowMap, shadowMapSampler, lightSpacePos);
}

OutputVS SampleVS(InputVS input)
{
    OutputVS output;

    float4 newPosition = float4(input.position, 1.0);

    newPosition = mul(newPosition, scene.model);

    output.worldpos = newPosition;

    newPosition = mul(newPosition, scene.viewProj);
   
    output.position = newPosition;
    output.uv = input.uv;
    
    // need transformed to world space too?
    // this only works if only uniform scale and invT on normal
    //input.normal.z *= -1.0; // why negated?
    output.normal = mul(float4(input.normal, 0.0), scene.model).xyz;
    output.tangent = mul(float4(input.tangent, 0.0), scene.model).xyz;

    return output;
}

float4 SamplePS(OutputVS input) : SV_Target0
{
    half4 diffuseColor = SampleH(diffuseMap, sampleWrap, input.uv);
    half3 pixelNormal = CalcPerPixelNormal(input.uv, (half3)input.normal, (half3)input.tangent);
    half4 totalLight = (half4)scene.ambientColor;

    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        LightState light = scene.lights[i];
        half4 lightPass = CalcLightingColor(light.position, light.direction, (half4)light.color, light.falloff, input.worldpos.xyz, pixelNormal);
        
        // only single light shadow map
        if (scene.sampleShadowMap && i == 0)
        {
            lightPass *= CalcUnshadowedAmountPCF2x2(i, input.worldpos, light.viewProj);
        }
        totalLight += lightPass;
    }

    return (float4)(diffuseColor * saturate(totalLight));
}


