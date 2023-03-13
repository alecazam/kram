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
Texture2D<float4> diffuseMap : register(t1);
Texture2D<float4> normalMap : register(t2);

SamplerState sampleWrap : register(s0);
SamplerState sampleClamp : register(s1);

// #define didn't compile due to lack of preprocesor
static const int NUM_LIGHTS = 3;
static const float SHADOW_DEPTH_BIAS = 0.00005;

struct LightState
{
    float3 position;
    float3 direction;
    float4 color;
    float4 falloff;
    float4x4 viewProj;
};

/*
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 model;
    float4x4 viewProj;
    float4 ambientColor;
    bool sampleShadowMap;
    LightState lights[NUM_LIGHTS];
};
*/

struct SceneConstantBuffer
{
    float4x4 model;
    float4x4 viewProj;
    float4 ambientColor;
    bool sampleShadowMap;
    LightState lights[3];
};
// TODO: NUM_LIGHTS isn't unhidden when parsing structs
// LightState lights[NUM_LIGHTS];

// SM 6.1
ConstantBuffer<SceneConstantBuffer> scene : register(b0);

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

struct InputPS
{
    float4 position : SV_Position;
    float4 worldpos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};


//--------------------------------------------------------------------------------------
// Sample normal map, convert to signed, apply tangent-to-world space transform.
//--------------------------------------------------------------------------------------
float3 CalcPerPixelNormal(float2 vTexcoord, float3 vVertNormal, float3 vVertTangent)
{
    // Compute tangent frame.
    vVertNormal = normalize(vVertNormal);
    vVertTangent = normalize(vVertTangent);

    float3 vVertBinormal = normalize(cross(vVertTangent, vVertNormal));
    float3x3 mTangentSpaceToWorldSpace = float3x3(vVertTangent, vVertBinormal, vVertNormal);

    // Compute per-pixel normal.
    float3 vBumpNormal = Sample(normalMap, sampleWrap, vTexcoord).xyz;
    vBumpNormal = 2.0 * vBumpNormal - 1.0;

    return mul(vBumpNormal, mTangentSpaceToWorldSpace);
}

//--------------------------------------------------------------------------------------
// Diffuse lighting calculation, with angle and distance falloff.
//--------------------------------------------------------------------------------------
float4 CalcLightingColor(float3 vLightPos, float3 vLightDir, float4 vLightColor, float4 vFalloffs, float3 vPosWorld, float3 vPerPixelNormal)
{
    float3 vLightToPixelUnNormalized = vPosWorld - vLightPos;

    // Dist falloff = 0 at vFalloffs.x, 1 at vFalloffs.x - vFalloffs.y
    float fDist = length(vLightToPixelUnNormalized);

    float fDistFalloff = saturate((vFalloffs.x - fDist) / vFalloffs.y);

    // Normalize from here on.
    float3 vLightToPixelNormalized = vLightToPixelUnNormalized / fDist;

    // Angle falloff = 0 at vFalloffs.z, 1 at vFalloffs.z - vFalloffs.w
    float fCosAngle = dot(vLightToPixelNormalized, vLightDir / length(vLightDir));
    float fAngleFalloff = saturate((fCosAngle - vFalloffs.z) / vFalloffs.w);

    // Diffuse contribution.
    float fNDotL = saturate(-dot(vLightToPixelNormalized, vPerPixelNormal));

    return vLightColor * fNDotL * fDistFalloff * fAngleFalloff;
}

//--------------------------------------------------------------------------------------
// Test how much pixel is in shadow, using 2x2 percentage-closer filtering.
//--------------------------------------------------------------------------------------
float4 CalcUnshadowedAmountPCF2x2(int lightIndex, float4 vPosWorld, float4x4 viewProj)
{
    // Compute pixel position in light space.
    float4 vLightSpacePos = vPosWorld;
    vLightSpacePos = mul(vLightSpacePos, viewProj);
    
    vLightSpacePos.xyz /= vLightSpacePos.w;

    // Translate from homogeneous coords to texture coords.
    float2 vShadowTexCoord = 0.5 * vLightSpacePos.xy + 0.5;
    vShadowTexCoord.y = 1.0 - vShadowTexCoord.y;

    // Depth bias to avoid pixel self-shadowing.
    float vLightSpaceDepth = vLightSpacePos.z - SHADOW_DEPTH_BIAS;

    // Find sub-pixel weights.
    float2 vShadowMapDims = float2(1280.0, 720.0); // need to keep in sync with .cpp file
    float4 vSubPixelCoords = float4(1.0, 1.0, 1.0, 1.0);
    vSubPixelCoords.xy = frac(vShadowMapDims * vShadowTexCoord);
    vSubPixelCoords.zw = 1.0 - vSubPixelCoords.xy;
    float4 vBilinearWeights = vSubPixelCoords.zxzx * vSubPixelCoords.wwyy;

    // 2x2 percentage closer filtering.
    float2 vTexelUnits = 1.0 / vShadowMapDims;
    float4 vShadowDepths;
    vShadowDepths.x = Sample(shadowMap, sampleClamp, vShadowTexCoord).x;
    vShadowDepths.y = Sample(shadowMap, sampleClamp, vShadowTexCoord + float2(vTexelUnits.x, 0.0)).x;
    vShadowDepths.z = Sample(shadowMap, sampleClamp, vShadowTexCoord + float2(0.0, vTexelUnits.y)).x;
    vShadowDepths.w = Sample(shadowMap, sampleClamp, vShadowTexCoord + vTexelUnits).x;

    // What weighted fraction of the 4 samples are nearer to the light than this pixel?
    float4 vShadowTests = (vShadowDepths >= vLightSpaceDepth);
    return dot(vBilinearWeights, vShadowTests);
}

OutputVS SampleVS(InputVS input)
{
    OutputVS output;

    float4 newPosition = float4(input.position, 1.0);

    input.normal.z *= -1.0; // why negated?
    newPosition = mul(newPosition, scene.model);

    output.worldpos = newPosition;

    newPosition = mul(newPosition, scene.viewProj);
   
    output.position = newPosition;
    output.uv = input.uv;
    
    // TODO: need transformed to world space too?
    output.normal = input.normal;
    output.tangent = input.tangent;

    return output;
}

float4 SamplePS(InputPS input) : SV_Target0
{
    float4 diffuseColor = Sample(diffuseMap, sampleWrap, input.uv);
    float3 pixelNormal = CalcPerPixelNormal(input.uv, input.normal, input.tangent);
    float4 totalLight = scene.ambientColor;

    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        LightState light = scene.lights[i];
        float4 lightPass = CalcLightingColor(light.position, light.direction, light.color, light.falloff, input.worldpos.xyz, pixelNormal);
        if (scene.sampleShadowMap && i == 0)
        {
            lightPass *= CalcUnshadowedAmountPCF2x2(i, input.worldpos, light.viewProj);
        }
        totalLight += lightPass;
    }

    return diffuseColor * saturate(totalLight);
}


