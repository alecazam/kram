// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import "KramShaders.h"

using namespace metal;

//---------------------------------
// helpers

//constant float PI = 3.1415927;

float toUnorm8(float c)
{
    return (127.0 / 255.0) * c + (128.0 / 255.0);
}
float2 toUnorm8(float2 c)
{
    return (127.0 / 255.0) * c + (128.0 / 255.0);
}
float3 toUnorm8(float3 c)
{
    return (127.0 / 255.0) * c + (128.0 / 255.0);
}
float4 toUnorm8(float4 c)
{
    return (127.0 / 255.0) * c + (128.0 / 255.0);
}

float toUnorm(float c)
{
    return 0.5 * c + 0.5;
}
float2 toUnorm(float2 c)
{
    return 0.5 * c + 0.5;
}
float3 toUnorm(float3 c)
{
    return 0.5 * c + 0.5;
}
float4 toUnorm(float4 c)
{
    return 0.5 * c + 0.5;
}

float toSnorm8(float c)
{
    return (255.0 / 127.0) * c - (128.0 / 127.0);
}
float2 toSnorm8(float2 c)
{
    return (255.0 / 127.0) * c - (128.0 / 127.0);
}
float3 toSnorm8(float3 c)
{
    return (255.0 / 127.0) * c - (128.0 / 127.0);
}
float4 toSnorm8(float4 c)
{
    return (255.0 / 127.0) * c - (128.0 / 127.0);
}

half2 toSnorm8(half2 c)
{
    return (255.0h / 127.0h) * c - (128.0h / 127.0h);
}

float toSnorm(float c)
{
    return 2.0 * c - 1.0;
}
float2 toSnorm(float2 c)
{
    return 2.0 * c - 1.0;
}
float3 toSnorm(float3 c)
{
    return 2.0 * c - 1.0;
}
float4 toSnorm(float4 c)
{
    return 2.0 * c - 1.0;
}

float recip(float c)
{
    return 1.0 / c;
}
float2 recip(float2 c)
{
    return 1.0 / c;
}
float3 recip(float3 c)
{
    return 1.0 / c;
}
float4 recip(float4 c)
{
    return 1.0 / c;
}

half toHalf(float c)
{
    return half(c);
}
half2 toHalf(float2 c)
{
    return half2(c);
}
half3 toHalf(float3 c)
{
    return half3(c);
}
half4 toHalf(float4 c)
{
    return half4(c);
}

float toFloat(half c)
{
    return float(c);
}
float2 toFloat(half2 c)
{
    return float2(c);
}
float3 toFloat(half3 c)
{
    return float3(c);
}
float4 toFloat(half4 c)
{
    return float4(c);
}

float4 toPremul(float4 c) {
    c.rgb *= c.a;
    return c;
}
half4 toPremul(half4 c) {
    c.rgb *= c.a;
    return c;
}

// TODO: note that Metal must pass the same half3 from vertex to fragment shader
// so can't mix a float vs with half fs.

//-------------------------------------------
// functions

// https://bgolus.medium.com/anti-aliased-alpha-test-the-esoteric-alpha-to-coverage-8b177335ae4f
float toMipLevel(float2 uv)
{
    float2 dx = dfdx(uv);
    float2 dy = dfdy(uv);
    
    // a better approximation than fwidth
    float deltaSquared = max(length_squared(dx), length_squared(dy));
    
    // 0.5 because squared, find mip level
    return max(0.0, 0.5 * log2(deltaSquared));
}

// Also see here:
// https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-28-mipmap-level-measurement
//  100 percent, 25 percent, 6.3 percent, and 1.6 percent)

float4 toMipLevelColor(float2 uv)
{
    // yellow, blue, green, red, black/transparent
    // 1, 0.75, 0.5, 0.25, 0
    // point sample from a texture with unique mip level colors
    float lev = toMipLevel(uv);
    float clev = saturate(lev / 4.0);
    float alpha = saturate(1.0 - clev);
    
    const float3 colors[5] = {
        float3(1,1,0), // yellow
        float3(0,0,1), // blue
        float3(0,1,0), // green
        float3(1,0,0), // red
        float3(0,0,0), // black
    };
    
    float clev4 = clev * 4.0;
    float3 low = colors[int(floor(clev4))];
    float3 hi  = colors[int(round(clev4))];
                  
    // lerp in unmul space
    float3 color = mix(low, hi, fract(clev4));
    
    return toPremul(float4(color, alpha));
}
            
// reconstruct normal from xy, n.z ignored
float3 toNormal(float3 n)
{
    // make sure the normal doesn't exceed the unit circle
    // many reconstructs skip and get a non-unit or n.z=0
    // this can all be done with half math too
    
    float len2 = length_squared(n.xy);
    const float maxLen2 = 0.999 * 0.999;
    
    if (len2 <= maxLen2) {
        // textures should be corrected to always take this path
        n.z = sqrt(1.0 - len2);
    }
    else {
        n.xy *= 0.999 * rsqrt(len2);
        n.z = 0.0447108; // sqrt(1-maxLen2)
    }
    
    return n;
}

// reconstruct normal from xy, n.z ignored
half3 toNormal(half3 n)
{
    // make sure the normal doesn't exceed the unit circle
    // many reconstructs skip and get a non-unit or n.z=0
    // this can all be done with half math too
    
    half len2 = length_squared(n.xy);
    const half maxLen2 = 0.999h * 0.999h;
    
    if (len2 <= maxLen2) {
        // textures should be corrected to always take this path
        n.z = sqrt(1.0h - len2);
    }
    else {
        n.xy *= 0.999h * rsqrt(len2);
        n.z = 0.0447108h; // sqrt(1-maxLen2)
    }
    
    return n;
}

// This will result in comlier failed XPC_ERROR_CONNECTION_INTERRUPTED
// was based on forum suggestion.  assert() does nothing in Metal.
//#define myMetalAssert(x) \
//    if (!(x)) { \
//        device float* f = 0; \
//        *f = 12; \
//    }
//#define myMetalAssert(x) assert(x)

// https://www.gamasutra.com/blogs/RobertBasler/20131122/205462/Three_Normal_Mapping_Techniques_Explained_For_the_Mathematically_Uninclined.php?print=1
// http://www.thetenthplanet.de/archives/1180
// This generates the TBN from vertex normal and p and uv derivatives
// Then transforms the bumpNormal to that space.  No tangent is needed.
// The downside is this must all be fp32, and all done in fragment shader and use derivatives.
// Derivatives are known to be caclulated differently depending on hw and different precision.
float length_squared(float x) {
    return x * x;
}

// how is this not a built-in?
float cross(float2 lhs, float2 rhs) {
    return lhs.x * rhs.y - rhs.x * lhs.y;
}

bool generateFragmentTangentBasis(half3 vertexNormal, float3 worldPos, float2 uv, thread float3x3& basis)
{
    float3 N = toFloat(vertexNormal);
    
    // normalizing this didn't help the reconstruction
    //N = normalize(N);
    
    // Original code pases viewDir, but that is constant for ortho view and would only work for perspective.
    // Comment was that cameraPos drops out since it's constant, but perspective viewDir is also typically normalized too.
    // Here using worldPos but it has much larger magnitude than uv then.
    
    // get edge vectors of the pixel triangle
    float3 dpx = dfdx(worldPos);
    float3 dpy = dfdy(worldPos);
    
    //N = normalize(cross(dpy, dpx));
    
    //dpx.y = -dpx.y;
    //dpy.y = -dpy.y;
    
    // could also pass isFrontFacing, should this almost always be true
    
    // The math problem here seems related to that we're using the planar dpx/dpy.
    // but the normal is interpolated on the sphere, and plane is likely closer to dNx/dNy.
    
    //float3 faceNormal = cross(dpy, dpx); // because dpy is down on screen
    //bool isFlipped = dot(faceNormal, N) > 0;
    
    // These are much smaller in magnitude than the position derivatives
    float2 duvx = dfdx(uv);
    float2 duvy = dfdy(uv);

    // flip T based on uv direction to handle mirrored UV
    float uvPlaneSign = sign(cross(duvy, duvx));
    
#if 1
    
    // can't really tell this from using N
    float3 useN;
    
    //float3 faceNormal = cross(dpy, dpx);
    //useN = faceNormal;
    
    useN = N;
    
    // solve the linear system
    float3 dp1perp = cross(useN, dpx); // vertical
    float3 dp2perp = cross(dpy, useN); // horizontal
#else
    float3 dp1perp = -dpy;
    float3 dp2perp = dpx;
#endif
    
    // could use B = dp1perp
    //if (Blen == 0.0)
    //    return false;
    
    float3 T, B;

#if 0
    B = normalize(dp1perp);
    T = -normalize(dp2perp);
#elif 1
    B = dp2perp * duvx.y + dp1perp * duvy.y;
    float Blen = length_squared(B);

    // vertical ridges with T.y flipping sign
    B *= rsqrt(Blen);
    T = cross(B, N);
    
    // This switches to lhcs on left side of mirrored sphere
    // May just be that ModelIO has generated bad basis on that left side.
    T *= -uvPlaneSign;
    
#elif 0
    // This calc just doesn't look as good
    
    // trapezoidal pattern wih T.y flipping sign
    T = dp2perp * duvx.x + dp1perp * duvy.x;
    float Tlen = length_squared(T);

    T *= rsqrt(Tlen);
    
    //T = -T;
    
    // Fixes tangent on mirrored sphere but Bitangent is wrong, does this mean uv wrap switches to lhcs instead of rhcs?
    T *= uvPlaneSign;
    
    B = cross(N, T);
#endif
    
    basis = float3x3(T, B, N);
    return true;
}

half3 transformNormalByBasis(half3 bumpNormal, half3 vertexNormal, float3 worldPos, float2 uv)
{
    float3x3 basis;
    bool success = generateFragmentTangentBasis(vertexNormal, worldPos, uv, basis);
    
    if (!success) {
        return vertexNormal;
    }
    
    // construct a scale-invariant frame
    // drop to half to match other call
    bumpNormal = toHalf(basis * toFloat(bumpNormal));
    
    return bumpNormal;
}

// use mikktspace, gen bitan in frag shader with sign, don't normalize vb/vt
// see http://www.mikktspace.com/
half3 transformNormalByBasis(half3 bumpNormal, half4 tangent, half3 vertexNormal)
{
    // Normalize tangent/vertexNormal in vertex shader
    // but don't renormalize interpolated tangent, vertexNormal in fragment shader
    // Reconstruct bitan in frag shader
    // https://bgolus.medium.com/generating-perfect-normal-maps-for-unity-f929e673fc57

    // TODO: there's facing too, could be inside model
    
    half bitangentSign = tangent.w;
    half3 bitangent =  bitangentSign * cross(vertexNormal, tangent.xyz);
    
    // ModelIO not generating correct bitan sign
    // DONE: flip this on srcData, and not here
    //bitangentSign = -bitangentSign;
    
    // now transform by basis and normalize from any shearing, and since interpolated basis vectors
    // are not normalized
    half3x3 tbn = half3x3(tangent.xyz, bitangent, vertexNormal);
    bumpNormal = tbn * bumpNormal;
    return normalize(bumpNormal);
}

half3 transformNormal(half4 nmap, half3 vertexNormal, half4 tangent,
                      float3 worldPos, float2 uv, bool useTangent, // to gen TBN from normal
                      bool isSwizzleAGToRG, bool isSigned, bool isFrontFacing)
{
    // add swizzle for ASTC/BC5nm, other 2 channels format can only store 01 in ba
    // could use hw swizzle for this
    if (isSwizzleAGToRG) {
        nmap = half4(nmap.ag, 0, 1);
    }

    // to signed, also for ASTC/BC5nm
    if (!isSigned) {
        // convert to signed normal to compute z
        nmap.rg = toSnorm8(nmap.rg);
    }
    
    half3 bumpNormal = nmap.xyz;
    
    bumpNormal = toNormal(bumpNormal);

    // handle the basis here (need worldPos and uv for other path)
    if (useTangent) {
        // flip the normal if facing is flipped
        // TODO: needed for tangent too?
        if (!isFrontFacing) {
            bumpNormal = -bumpNormal;
            tangent.w = -tangent.w;
        }
    
        bumpNormal = transformNormalByBasis(bumpNormal, tangent, vertexNormal);
    }
    else {
        bumpNormal = transformNormalByBasis(bumpNormal, vertexNormal, worldPos, uv);
    }
    
    return bumpNormal;
}

half3 transformNormal(half4 tangent, half3 vertexNormal, float3 worldPos,
                      bool useTangent,
                      texture2d<half> texture, sampler s, float2 uv,
                      bool isSigned, bool isSwizzleAGToRG, bool isFrontFacing)
{
    half4 nmap = texture.sample(s, uv);
    
    return transformNormal(nmap, vertexNormal, tangent,
                           worldPos, uv, useTangent,
                           isSwizzleAGToRG, isSigned, isFrontFacing);
}

// TODO: have more bones, or read from texture instead of uniforms
// can then do instanced skining, but vfetch lookup slower
#define maxBones 128

// this is for vertex shader
void skinPosAndBasis(thread float4& position, thread float3& tangent, thread float3& normal,
                     uint4 indices, float4 weights, float3x4 bones[maxBones])
{
    // TODO: might do this as up to 3x vtex lookup per bone, fetch from buffer texture
    // but uniforms after setup would be faster if many bones.  Could support 1-n bones with vtex.
    // instances use same bones, but different indices/weights already
    // but could draw skinned variants with vtex lookup and not have so much upload prep
    
    float3x4 bindPoseToBoneTransform = bones[indices.x];
    
    if (weights[0] != 1.0)
    {
        // weight the bone transforms
        bindPoseToBoneTransform *= weights[0];
        
        // with RGB10A2U have 2 bits in weights.w to store the boneCount
        // or could count non-zero weights, make sure to set w > 0 if 4 bones
        // the latter is more compatible with more conent
        
        //int numBones = 1 + int(weights.w * 3.0);
        
        int numBones = int(dot(float4(weights > 0.0), float4(1.0)));
        
        // reconstruct so can store weights in RGB10A2U
        if (numBones == 4)
            weights.w = 1.0 - saturate(dot(weights.xyz, float3(1.0)));
        
        for (int i = 1; i < numBones; ++i)
        {
            bindPoseToBoneTransform += bones[indices[i]] * weights[i];
        }
    }
    
    // 3x4 is a transpose of 4x4 transform
    position.xyz = position * bindPoseToBoneTransform;
    
    // not dealing with non-uniform scale correction
    // see scale2 handling in transformBasis, a little different with transpose of 3x4
    
    normal  = (float4(normal, 0.0)  * bindPoseToBoneTransform);
    
    // compiler will deadstrip if tangent unused by caller
    //if (useTangent)
        tangent = (float4(tangent, 0.0) * bindPoseToBoneTransform);
}

inline float3x3 toFloat3x3(float4x4 m)
{
    return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}

// this is for vertex shader if tangent supplied
void transformBasis(thread float3& normal, thread float3& tangent,
                    float4x4 modelToWorldTfm, float3 invScale2, bool useTangent)
{
    
    float3x3 m = toFloat3x3(modelToWorldTfm);
    
    // note this is RinvT * n = (Rt)t = R, this is for simple inverse, inv scale handled below
    // but uniform scale already handled by normalize
    normal = m * normal;
    normal *= invScale2;
    normal = normalize(normal);
   
    // question here of whether tangent is transformed by m or mInvT
    // most apps assume m, but after averaging it can be just as off the surface as the normal
    if (useTangent) {
        tangent = m * tangent;
        tangent *= invScale2;
        tangent = normalize(tangent);
    }
    
    // make sure to preserve bitan sign in tangent.w
}

//-------------------------------------------

struct Vertex
{
    float4 position [[attribute(VertexAttributePosition)]];
    float2 texCoord [[attribute(VertexAttributeTexcoord)]];
    
    // basis
    float3 normal [[attribute(VertexAttributeNormal)]]; // consider half
    float4 tangent [[attribute(VertexAttributeTangent)]]; // tan + bitanSign
};

struct ColorInOut
{
    float4 position [[position]];
    float3 texCoordXYZ;
    float2 texCoord;
    float3 worldPos;
    
    // basis
    half3 normal;
    half4 tangent;
};

ColorInOut DrawImageFunc(
    Vertex in [[stage_in]],
    constant Uniforms& uniforms,
    constant UniformsLevel& uniformsLevel
)
{
    ColorInOut out;

    float4 position = in.position;
    //position.xy += uniformsLevel.drawOffset;
    
    float4 worldPos = uniforms.modelMatrix * position;
    
    // deal with full basis
    
    bool needsWorldBasis =
        uniforms.isPreview ||
        //uniforms.isNormalMapPreview ||
        // these need normal transformed to world space
        uniforms.shapeChannel == ShaderShapeChannel::ShShapeChannelTangent ||
        uniforms.shapeChannel == ShaderShapeChannel::ShShapeChannelNormal ||
        uniforms.shapeChannel == ShaderShapeChannel::ShShapeChannelBitangent;

    if (needsWorldBasis) {
        float3 normal = in.normal;
        float3 tangent = in.tangent.xyz;
        transformBasis(normal, tangent, uniforms.modelMatrix, uniforms.modelMatrixInvScale2.xyz, uniforms.useTangent);
        
        out.normal = toHalf(normal);
        
        // may be invalid if useTangent is false
        out.tangent.xyz = toHalf(tangent);
        out.tangent.w = toHalf(in.tangent.w);
    }
    else {
        out.normal = toHalf(in.normal);
        out.tangent = toHalf(in.tangent);
    }
    // try adding pixel offset to pixel values
    worldPos.xy += uniformsLevel.drawOffset;
    
    out.position = uniforms.projectionViewMatrix * worldPos;
    
    // this is a 2d coord always which is 0 to 1, or 0 to 2
    if (uniforms.isWrap) {
        // can make this a repeat value uniform
        float wrapAmount = 2.0;
        out.texCoord.xy = in.texCoord;
        out.texCoord.xy *= wrapAmount;
    }
    else if (uniforms.is3DView && uniforms.isInsetByHalfPixel) {
        // inset from edge by a fraction of a pixel, to avoid clamp boundary error
        // does this have to adjust for mipLOD too?
        float2 onePixel = uniformsLevel.textureSize.zw;
        float2 halfPixel = 0.5 * onePixel;
        
        out.texCoord.xy = clamp(in.texCoord, halfPixel, float2(1.0) - halfPixel);
    }
    else {
        out.texCoord.xy = in.texCoord;
    }
   
    // potentially 3d coord, and may be -1 to 1
    out.texCoordXYZ.xy = out.texCoord;
    out.texCoordXYZ.z = 0.0;
   
    out.worldPos = worldPos.xyz;
    return out;
}

vertex ColorInOut DrawImageVS(
    Vertex in [[stage_in]],
    constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
    constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]]
)
{
    return DrawImageFunc(in, uniforms, uniformsLevel);
}

vertex ColorInOut DrawCubeVS(
     Vertex in [[stage_in]],
     constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
     constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]]
)
{
    ColorInOut out = DrawImageFunc(in, uniforms, uniformsLevel);
    
    // convert to -1 to 1
    float3 uvw;

    // if preview, then actually sample from cube map
    // and don't override to the face
    if (uniforms.isPreview) {
        uvw = 2 * in.position.xyz; // use model-space pos
    }
    else {
        uvw = out.texCoordXYZ;
        uvw.xy = toSnorm(uvw.xy);
        uvw.z = 1.0;
        
        // switch to the face
        switch(uniformsLevel.face) {
            case 0: // x
                uvw = uvw.zyx;
                uvw.zy *= -1; // to match original cube image
                break;
                
            case 1: // -x
                uvw = uvw.zyx;
                uvw.yz *= -1;
                uvw.z *= -1; // to match PVR
                uvw.x = -1;
                break;
                
            case 2: // y
                uvw = uvw.xzy;
                break;
                
            case 3: // -y
                uvw = uvw.xzy;
                uvw.xz *= -1;
                uvw.x *= -1; // to match PVR
                uvw.y = -1;
                break;
                
            case 4: // z
                uvw = uvw.xyz; // noop
                uvw.y *= -1;
                break;
                
            case 5: // -z
                uvw = uvw.xyz;
                uvw.xy *= -1;
                uvw.z = -1;
                break;
        }
    }
    
    out.texCoordXYZ = uvw;
    return out;
}

vertex ColorInOut DrawVolumeVS(
    Vertex in [[stage_in]],
    constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
    constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]]
)
{
    ColorInOut out = DrawImageFunc(in, uniforms, uniformsLevel);
    
    // this is normalized in ps by dividing by depth-1
    out.texCoordXYZ.z = uniformsLevel.arrayOrSlice;
    return out;
}


float3 reflectIQ(float3 v, float3 n)
{
#if 0
    // traditional refect
    // v - 2 * n * dot(v n)
    float3 r = reflect(v, n);
    
    return r;
#else
    // Not sure why IQ uses the r notation
    float3 r = n;
    
    // https://iquilezles.org/www/articles/dontflip/dontflip.htm
    // works for any dimension
    // also article has a clamp forumulation
    
    float k = dot(v, r);
    
    // reflect v if it's in the negative half plane defined by r
    return (k > 0.0) ? v : (v - 2.0 * r * k);
#endif
}

float4 doLighting(float4 albedo, float3 viewDir, float3 bumpNormal, float3 vertexNormal, ShaderLightingMode lightingMode) {
    if (albedo.a == 0.0)
        return albedo;
    
    float3 lightDir = normalize(float3(1,1,1)); // looking down -Z axis
    float3 lightColor = float3(1,1,1);

    float3 specular = float3(0.0);
    float3 diffuse = float3(0.0);
    float3 ambient = float3(0.0);
    
    // Need lighting control in UI, otherwise specular just adds a big bright
    // circle to all texture previews since it's additive.
    bool doBlinnPhongSpecular = false;
    
    bool doSpecular = true; // can confuse lighting review, make option to enable or everything has bright white spot
    bool doDiffuse = true;
    bool doAmbient = true;
    
    if (lightingMode == ShLightingModeDiffuse)
    {
        doSpecular = false;
    }
    
    // see here about energy normalization, not going to GGX just yet
    // http://www.thetenthplanet.de/archives/255
    
    // Note: this isn't the same as the faceNormal, the vertexNormal is interpolated
    // see iq's trick for flipping lighting in reflectIQ.
    
    // Use reflectIQ to flip specular, 
    //float dotVertexNL = dot(vertexNormal, lightDir);
    
    float dotNL = dot(bumpNormal, lightDir);
    
    if (doSpecular) {
        //if (dotVertexNL > 0.0) {
            float specularAmount;
            
            // in lieu of a roughness map, do this
            // fake energy conservation by multiply with gloss
            // https://www.youtube.com/watch?v=E4PHFnvMzFc&t=946s
            float gloss = 0.3;
            float specularExp = exp2(gloss * 11.0) + 2.0;
            float energyNormalization = gloss;
            
            if (doBlinnPhongSpecular) {
                // this doesn't look so good as a highlight in ortho at least
                float3 E = -viewDir;
                float3 H = normalize(lightDir + E);
                float dotHN = saturate(dot(H, bumpNormal));
                specularAmount = dotHN;
                
                // to make dotHN look like dotRL
                // https://en.wikipedia.org/wiki/Blinn%E2%80%93Phong_reflection_model
                specularExp *= 4.0;
                
                //energyNormalization = (specularExp + 1.0) / (2.0 * PI);
            }
            else {
                // phong
                // and seem to recall a conversion to above but H = (L+V)/2, the normalize knocks out the 1/2
                float3 ref = normalize(reflectIQ(viewDir, bumpNormal));
                float dotRL = saturate(dot(ref, lightDir));
                specularAmount = dotRL;
                
                //energyNormalization = (specularExp + 1.0) / (2.0 * PI);
            }
            
            // above can be interpolated
            specularAmount = pow(specularAmount, specularExp) * energyNormalization;
            specular = specularAmount * lightColor.rgb;
       // }
    }

    if (doDiffuse) {
        
        float dotNLSat = saturate(dotNL);
        
        // soften the terminator off the vertNormal
        // this is so no diffuse if normal completely off from vertex normal
        // also limiting diffuse lighting bump to lighting by vertex normal
        float dotVertex = saturate(dot(vertexNormal, bumpNormal));
        dotNL *= saturate(9.0 * dotVertex);
        
        diffuse = dotNLSat * lightColor.rgb;
    }
    
    if (doAmbient) {
        // can misconstrue as diffuse with this, but make dark side not look flat
        float dotNLUnsat = dotNL;
        ambient = mix(0.1, 0.2, saturate(dotNLUnsat * 0.5 + 0.5));
    }
    
    // attenuate, and not saturate below, so no HDR yet
    specular *= 0.8;
    diffuse *= 0.7;
    //ambient *= 0.2;
    
#if 0
    // attenuating albedo with specular knocks it all out
    albedo.xyz *= saturate(ambient + diffuse + specular);
#else
    albedo.xyz *= saturate(diffuse + ambient);
    albedo.xyz += specular;
    albedo.xyz = saturate(albedo.xyz);
#endif
    
    return albedo;
}

float3 calculateViewDir(float3 worldPos, float3 cameraPosition) {
    // ortho case
    return float3(0,0,-1);
    
    // TODO: need perspective preview
    //return normalize(worldPos - cameraPosition);
}

// This is writing out to 16F and could write snorm data, but then that couldn't be displayed.
// So code first converts to Unorm.

float4 DrawPixels(
    ColorInOut in [[stage_in]],
    bool facing,
    constant Uniforms& uniforms,
    float4 c,
    float4 nmap,
    float2 textureSize
)
{
    float4 sc = c;
    
    bool isPreview = uniforms.isPreview;
    if (isPreview) {
        
        if (uniforms.isSDF) {
            if (!uniforms.isSigned) {
                // convert to signed normal to compute z
                c.r = toSnorm8(c.r); // 0 = 128 on unorm data on 8u
            }
            
            // 0.0 is the boundary of visible vs. non-visible and not a true alpha
            if (c.r < 0.0) {
                discard_fragment();
            }
            
            // https://drewcassidy.me/2020/06/26/sdf-antialiasing/
            // can AA the edges
            // adapted for signed field above,
            // sdf distance from edge (scalar)
            float dist = c.r;

            // size of one pixel line
            float onePixel = recip(max(0.0001, length(float2(dfdx(dist), dfdy(dist)))));

            // distance to edge in pixels (scalar)
            float pixelDist = dist * onePixel;

            // typically source recommends smoothstep, so that get a soft instead of hard ramp of alpha at edges
            
            // store as preml alpha
            c.rgba = saturate(pixelDist);
        }
        else if (uniforms.isNormal) {
            // light the normal map
            half4 nmapH = toHalf(c);
            
            half3 n = transformNormal(nmapH, in.normal, in.tangent,
                                       in.worldPos, in.texCoord, uniforms.useTangent, // to build TBN
                                       uniforms.isSwizzleAGToRG, uniforms.isSigned, facing);
            
            
            float3 viewDir = calculateViewDir(in.worldPos, uniforms.cameraPosition);
            c = doLighting(float4(1.0), viewDir, toFloat(n), toFloat(in.normal), uniforms.lightingMode);

            c.a = 1;
        }
        else {
            // to unorm
            if (uniforms.isSigned) {
                c.xyz = toUnorm(c.xyz);
            }
            else { // TODO: need an isAlbedo test
                float3 viewDir = calculateViewDir(in.worldPos, uniforms.cameraPosition);
                
                if (uniforms.isNormalMapPreview) {
                    half4 nmapH = toHalf(nmap);
                   
                    half3 n = transformNormal(nmapH, in.normal, in.tangent,
                                               in.worldPos, in.texCoord, uniforms.useTangent, // to build TBN
                                               uniforms.isNormalMapSwizzleAGToRG, uniforms.isNormalMapSigned, facing);
                    
                    c = doLighting(c, viewDir, toFloat(n), toFloat(in.normal), uniforms.lightingMode);
                }
                else {
                    c = doLighting(c, viewDir, toFloat(in.normal), toFloat(in.normal), uniforms.lightingMode);
                }
            }
            
            // to premul, but also need to see without premul
            if (uniforms.isPremul) {
                c.xyz *= c.a;
            }
        }
        
        // this allows viewing wrap
        bool doShowUV = false;
        if (doShowUV) {
            c = float4(fract(in.texCoord), 0.0, 1.0);
        }
    }
    else {
        // handle single channel and SDF content
        if (uniforms.numChannels == 1) {
            // toUnorm
            if (uniforms.isSigned) {
                c.x = toUnorm(c.x);
            }
        }
        else if (uniforms.isNormal) {
            // add swizzle for ASTC/BC5nm, other 2 channels format can only store 01 in ba
            if (uniforms.isSwizzleAGToRG) {
                c = float4(c.ag, 0, 1);
            }
            
            // to signed
            if (!uniforms.isSigned) {
                // convert to signed normal to compute z
                c.rg = toSnorm8(c.rg);
            }
            
            c.rgb = toNormal(c.rgb);
            
            // from signed, to match other editors that don't display signed data
            sc = c;
            c.xyz = toUnorm(c.xyz); // can sample from this
            
            // view data as abs magnitude
            //c.xyz = abs(c.xyz); // bright on extrema, but no indicator of sign (use r,g viz)
            
            // normals can pack up to 5 data into 4 channels, xyz + 2 more channels,
            // this currently clobbers the 2 channels since BC5/ETC2rg can't store more data, but ASTC/BC1/3/7nm can
        }
        else {
            // from signed, to match other editors that don't display signed data
            // signed 1/2 channel formats return sr,0,0, and sr,sg,0 for rgb?
            // May want to display those as 0 not 0.5.
            if (uniforms.isSigned) {
                // Note: premul on signed should occur while still signed, since it's a pull to zoer
                // to premul, but also need to see without premul
                if (uniforms.isPremul) {
                    c.xyz *= c.a;
                }
                
                sc = c;
                c.xyz = toUnorm(c.xyz);
            }
            else {
                if (uniforms.isPremul) {
                    c.xyz *= c.a;
                }
            }
            
        }
    }
    
    if (uniforms.shapeChannel != ShShapeChannelNone) {
        // Hard to interpret direction from color, but have eyedropper to decipher render color.
        // See about using the vector flow fields to see values across render, but needs fsqd pass.
        
        if (uniforms.shapeChannel == ShShapeChannelUV0) {
            // fract so wrap will show repeating uv in 0,1, and never negative or large values
            // don't have mirror address modes yet.
            c.rgb = fract(in.texCoordXYZ);
        }
        else if (uniforms.shapeChannel == ShShapeChannelNormal) {
            c.rgb = toFloat(in.normal);
            
            c.rgb = toUnorm(c.rgb);
        }
        else if (uniforms.shapeChannel == ShShapeChannelTangent) {
            if (uniforms.useTangent) {
                c.rgb = toFloat(in.tangent.xyz);
            }
            else {
                float3x3 basis;
                bool success = generateFragmentTangentBasis(in.normal, in.worldPos, in.texCoord, basis);
                if (!success)
                    c.rgb = 0;
                else
                    c.rgb = basis[0];
            }
            
            c.rgb = toUnorm(c.rgb);
        }
        else if (uniforms.shapeChannel == ShShapeChannelBitangent) {
            if (uniforms.useTangent) {
                half3 bitangent = cross(in.normal, in.tangent.xyz) * in.tangent.w;
                c.rgb = toFloat(bitangent);
            }
            else {
                float3x3 basis;
                bool success = generateFragmentTangentBasis(in.normal, in.worldPos, in.texCoord, basis);
                if (!success)
                    c.rgb = 0;
                else
                    c.rgb = basis[1]; // bitan
            }
            
            c.rgb = toUnorm(c.rgb);
        }
        else if (uniforms.shapeChannel == ShShapeChannelDepth) {
            c.rgb = saturate(in.position.z / in.position.w);
        }
        else if (uniforms.shapeChannel == ShShapeChannelFaceNormal) {
            float3 faceNormal = -cross(dfdx(in.worldPos), dfdy(in.worldPos));
            faceNormal = normalize(faceNormal);
            
            // TODO: incorporate facing?
            
            c.rgb = toUnorm(faceNormal);
        }
        else if (uniforms.shapeChannel == ShShapeChannelMipLevel) {
            c = toMipLevelColor(in.texCoord * textureSize.xy); // only for 2d textures
        }
//        else if (uniforms.shapeChannel == ShShapeChannelBumpNormal) {
//            c.rgb = saturate(bumpNormal);
//        }
        
        if (uniforms.shapeChannel != ShShapeChannelMipLevel) {
            c.a = 1.0;
        }
    }
    
    // mask to see one channel in isolation, this is really 0'ing out other channels
    // would be nice to be able to keep this set on each channel independently.
    switch(uniforms.channels)
    {
        case ShModeRGBA: break;
            
        // with premul formats, already have ra,ga,ba
        case ShModeR001: c = float4(c.r,0,0,1); break;
        case ShMode0G01: c = float4(0,c.g,0,1); break;
        case ShMode00B1: c = float4(0,0,c.b,1); break;
            
//        case ShModeRRR1: c = float4(c.rrr,1); break;
//        case ShModeGGG1: c = float4(c.ggg,1); break;
//        case ShModeBBB1: c = float4(c.bbb,1); break;
//
        case ShModeAAA1: c = float4(c.aaa,1); break;
    }
    
    // be able to pinch-zoom into/back from the image
    // only show the grid when zoomed in past some pixel count
    // also show the outline around the edge of the image
    
    // https://ourmachinery.com/post/borderland-between-rendering-and-editor-part-1/
    
    // blend color onto checkboard
    if (uniforms.isCheckerboardShown) {
        // TODO: when this is on get white artifact at edge of image
        // fix that.  Also make this scale with zoom.
        
        // https://www.geeks3d.com/hacklab/20190225/demo-checkerboard-in-glsl/
        float repeats = 20.0;
        float2 checker = floor(repeats * in.texCoord);
        float selector = sign(fmod(checker.x + checker.y, 2.0));
        float cb = mix(float(1), float(222.0/255.0), selector);
        
        c.rgb = c.rgb + (1.0 - c.a) * cb;
        // nothing for alpha?
    }

    if (uniforms.debugMode != ShDebugModeNone && c.a != 0.0) {
        
        bool isHighlighted = false;
        if (uniforms.debugMode == ShDebugModeTransparent) {
            if (c.a < 1.0) {
                isHighlighted = true;
            }
        }
        else if (uniforms.debugMode == ShDebugModeNonZero) {
            // want to compare so snorm 0 on signed data
            // TODO: unorm formats don't store exact 0, so may need toleranc
            if (uniforms.isSigned) {
                if (any(sc.rgb != 0.0)) {
                    isHighlighted = true;
                }
            }
            else {
                if (any(c.rgb != 0.0)) {
                    isHighlighted = true;
                }
            }
        }
        else if (uniforms.debugMode == ShDebugModeColor) {
            // with 565 formats, all pixels with light up
            if (c.r != c.g || c.r != c.b) {
                isHighlighted = true;
            }
        }
        else if (uniforms.debugMode == ShDebugModeGray) {
            // with 565 formats, all pixels with light up
            if ((c.r > 0.0 && c.r < 1.0) && (c.r == c.g && c.r == c.b)) {
                isHighlighted = true;
            }
        }
        else if (uniforms.debugMode == ShDebugModeHDR) {
            if (any(c.rgb < float3(0.0)) || any(c.rgb > float3(1.0)) ) {
                isHighlighted = true;
            }
        }
        
        // signed data already convrted to unorm above, so compare to 0.5
        // adding some slop here so that flat areas don't flood the visual with red
        else if (uniforms.debugMode == ShDebugModePosX) {
            // two channels here, would need to color each channel
            if (uniforms.isSDF) {
                if (c.r >= 0.5) {
                    isHighlighted = true;
                }
            }
            else {
                if (c.r >= 0.5 + 0.05) {
                    isHighlighted = true;
                }
            }
        }
        else if (uniforms.debugMode == ShDebugModePosY) {
            if (c.g > 0.5 + 0.05) {
                isHighlighted = true;
            }
        }
        else if (uniforms.debugMode == ShDebugModeCircleXY) {
            // flag pixels that would throw off normal reconstruct sqrt(1-dot(n.xy,n.xy))
            // see code above in shader that helps keep that from z = 0
            float len2 = length_squared(toSnorm(c.rg));
            const float maxLen2 = 0.999 * 0.999;
            
            if (len2 > maxLen2) {
                isHighlighted = true;
            }
        }
        // TODO: is it best to highlight the interest pixels in red
        // or the negation of that to see which ones aren't.
        if (isHighlighted) {
            float3 highlightColor = float3(1, 0, 1);
            c.rgb = highlightColor;
        }
        
        // TODO: for normals, may want +xy, and -xy which can convey
        // whether normals are pointing correct directions.
    }
    
    // blend grid onto color - base off dFdX and dFdY
    if (uniforms.gridX > 0) {
        float2 pixels = in.texCoord * textureSize;
        
        // snap to the grid
        pixels = pixels / float2(uniforms.gridX, uniforms.gridY);
        
        // DONE: don't draw grid if too small
        
        // fwidth = abs(ddx(p)) + abs(ddy(p))
        float2 lineWidth = recip(fwidth(pixels));
        
        // only show grid when pixels are 8px or bigger
        if (max(lineWidth.x, lineWidth.y) >= 8.0) {
            // Compute anti-aliased world-space grid lines
            float2 grid = abs(fract(pixels - 0.5) - 0.5) * lineWidth;
            float line = min(grid.x, grid.y);

            // Just visualize the grid lines directly
            float lineIntensity = 1.0 - min(line, 1.0);
            
            // determine proximity of white color to pixel
            // and ensure contrast on this blend
            float cDist = distance(float3(1.0), c.rgb);
            
            float lineColor = 1.0;
            if (cDist < 0.2) {
                lineColor = 0.5;
            }
            
            c.rgb = mix(c.rgb, float3(lineColor), lineIntensity);
            
            // nothing for alpha?
        }
    }
    
    return c;
}

fragment float4 Draw1DArrayPS(
    ColorInOut in [[stage_in]],
    bool facing [[front_facing]],
    constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
    constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texture1d_array<float> colorMap [[ texture(TextureIndexColor) ]])
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.x, uniformsLevel.arrayOrSlice);
    
    // here are the pixel dimensions of the lod
    // uint lod = uniformsLevel.mipLOD;
    // mips not supported on 1D textures according to Metal spec.
    
    float2 textureSize = float2(colorMap.get_width(0), 1);
    // colorMap.get_num_mip_levels();

    float4 n = float4(0,0,1,1);
    return DrawPixels(in, facing, uniforms, c, n, textureSize);
}

fragment float4 DrawImagePS(
    ColorInOut in [[stage_in]],
    bool facing [[front_facing]],
    constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
    constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texture2d<float> colorMap [[ texture(TextureIndexColor) ]],
    texture2d<float> normalMap [[ texture(TextureIndexNormal) ]]
)
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.xy);
    float4 n = normalMap.sample(colorSampler, in.texCoordXYZ.xy);
   
    // here are the pixel dimensions of the lod
    uint lod = uniformsLevel.mipLOD;
    float2 textureSize = float2(colorMap.get_width(lod), colorMap.get_height(lod));
    // colorMap.get_num_mip_levels();

    return DrawPixels(in, facing, uniforms, c, n, textureSize);
}

fragment float4 DrawImageArrayPS(
    ColorInOut in [[stage_in]],
    bool facing [[front_facing]],
    constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
    constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texture2d_array<float> colorMap [[ texture(TextureIndexColor) ]],
    texture2d_array<float> normalMap [[ texture(TextureIndexNormal) ]]
)
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.xy, uniformsLevel.arrayOrSlice);
    float4 n = normalMap.sample(colorSampler, in.texCoordXYZ.xy, uniformsLevel.arrayOrSlice);
    
    // here are the pixel dimensions of the lod
    uint lod = uniformsLevel.mipLOD;
    float2 textureSize = float2(colorMap.get_width(lod), colorMap.get_height(lod));
    // colorMap.get_num_mip_levels();

    return DrawPixels(in, facing, uniforms, c, n, textureSize);
}


fragment float4 DrawCubePS(
    ColorInOut in [[stage_in]],
    bool facing [[front_facing]],
    constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
    constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texturecube<float> colorMap [[ texture(TextureIndexColor) ]]
)
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.xyz);
    
    // here are the pixel dimensions of the lod
    uint lod = uniformsLevel.mipLOD;
    float w = colorMap.get_width(lod);
    float2 textureSize = float2(w, w);
    // colorMap.get_num_mip_levels();

    float4 n = float4(0,0,1,1);
    return DrawPixels(in, facing, uniforms, c, n, textureSize);
}

fragment float4 DrawCubeArrayPS(
    ColorInOut in [[stage_in]],
    bool facing [[front_facing]],
    constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
    constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texturecube_array<float> colorMap [[ texture(TextureIndexColor) ]]
)
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.xyz, uniformsLevel.arrayOrSlice);
    
    // here are the pixel dimensions of the lod
    uint lod = uniformsLevel.mipLOD;
    float w = colorMap.get_width(lod);
    float2 textureSize = float2(w, w);
    // colorMap.get_num_mip_levels();

    float4 n = float4(0,0,1,1);
    return DrawPixels(in, facing, uniforms, c, n, textureSize);
}


fragment float4 DrawVolumePS(
    ColorInOut in [[stage_in]],
    bool facing [[front_facing]],
    constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
    constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texture3d<float> colorMap [[ texture(TextureIndexColor) ]]
)
{
    // fix up the tex coordinate here to be normalized w = [0,1]
    uint lod = uniformsLevel.mipLOD;
    float3 uvw = in.texCoordXYZ;
    
    uvw.z /= ((float)colorMap.get_depth(lod) - 1.0);
    
    // using clamp to border color so at ~1.0, get all black without the epsilon
    // might be enough to saturate?
    if (uvw.z > 0.999) {
        uvw.z = 0.999;
    }
    
    float4 c = colorMap.sample(colorSampler, uvw);
    
    // here are the pixel dimensions of the lod
    float2 textureSize = float2(colorMap.get_width(lod), colorMap.get_height(lod));
    // colorMap.get_num_mip_levels();

    float4 n = float4(0,0,1,1);
    return DrawPixels(in, facing, uniforms, c, n, textureSize);
}

//--------------------------------------------------


/* not using this yet, need a fsq and some frag coord to sample the normal map at discrete points
 
// https://www.shadertoy.com/view/4s23DG
// 2D vector field visualization by Morgan McGuire, @morgan3d, http://casual-effects.com


constant int   ARROW_V_STYLE = 1;
constant int   ARROW_LINE_STYLE = 2;

// Choose your arrow head style
constant int   ARROW_STYLE = ARROW_LINE_STYLE;
constant float ARROW_TILE_SIZE = 64.0;

// How sharp should the arrow head be? Used
constant float ARROW_HEAD_ANGLE = 45.0 * PI / 180.0;

// Used for ARROW_LINE_STYLE
constant float ARROW_HEAD_LENGTH = ARROW_TILE_SIZE / 6.0;
constant float ARROW_SHAFT_THICKNESS = 3.0;
    
// Computes the center pixel of the tile containing pixel pos
float2 arrowTileCenterCoord(float2 pos) {
    return (floor(pos / ARROW_TILE_SIZE) + 0.5) * ARROW_TILE_SIZE;
}

// v = field sampled at tileCenterCoord(p), scaled by the length
// desired in pixels for arrows
// Returns 1.0 where there is an arrow pixel.
float arrow(float2 p, float2 v) {
    // Make everything relative to the center, which may be fractional
    p -= arrowTileCenterCoord(p);
        
    float mag_v = length(v);
    float mag_p = length(p);
    
    if (mag_v > 0.0) {
        // Non-zero velocity case
        //float2 dir_p = p / mag_p;
        float2 dir_v = v / mag_v;
        
        // We can't draw arrows larger than the tile radius, so clamp magnitude.
        // Enforce a minimum length to help see direction
        mag_v = clamp(mag_v, 5.0, ARROW_TILE_SIZE / 2.0);

        // Arrow tip location
        v = dir_v * mag_v;
        
        // Define a 2D implicit surface so that the arrow is antialiased.
        // In each line, the left expression defines a shape and the right controls
        // how quickly it fades in or out.

        float dist;
        if (ARROW_STYLE == ARROW_LINE_STYLE) {
            // Signed distance from a line segment based on https://www.shadertoy.com/view/ls2GWG by
            // Matthias Reitinger, @mreitinger
            
            // Line arrow style
            dist =
                max(
                    // Shaft
                    ARROW_SHAFT_THICKNESS / 4.0 -
                        max(abs(dot(p, float2(dir_v.y, -dir_v.x))), // Width
                            abs(dot(p, dir_v)) - mag_v + ARROW_HEAD_LENGTH / 2.0), // Length
                        
                        // Arrow head
                     min(0.0, dot(v - p, dir_v) - cos(ARROW_HEAD_ANGLE / 2.0) * length(v - p)) * 2.0 + // Front sides
                     min(0.0, dot(p, dir_v) + ARROW_HEAD_LENGTH - mag_v)); // Back
        }
        else if (ARROW_STYLE == ARROW_V_STYLE) {
            // V arrow style
            dist = min(0.0, mag_v - mag_p) * 2.0 + // length
                   min(0.0, dot(normalize(v - p), dir_v) - cos(ARROW_HEAD_ANGLE / 2.0)) * 2.0 * length(v - p) + // head sides
                   min(0.0, dot(p, dir_v) + 1.0) + // head back
                   min(0.0, cos(ARROW_HEAD_ANGLE / 2.0) - dot(normalize(v * 0.33 - p), dir_v)) * mag_v * 0.8; // cutout
        }
        
        return clamp(1.0 + dist, 0.0, 1.0);
    }
    else {
        // Center of the pixel is always on the arrow
        return max(0.0, 1.2 - mag_p);
    }
}

*/

//--------------------------------------------------

// All this just to convert and readback one pixel

kernel void SampleImage1DArrayCS(
    texture1d_array<float, access::read> colorMap [[ texture(TextureIndexColor) ]],
    constant UniformsCS& uniforms [[ buffer(BufferIndexUniformsCS) ]],
    uint2 index [[thread_position_in_grid]],
    texture2d<float, access::write> result [[ texture(TextureIndexSamples) ]]
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint uv = uniforms.uv.x; // tie into texture lookup
    // no mips on 1dArray are possible
    
    uint arrayOrSlice = uniforms.arrayOrSlice;
    
    // the color returned is linear
    float4 color = colorMap.read(uv, arrayOrSlice, 0);
    result.write(color, index);
}

kernel void SampleImageCS(
    texture2d<float, access::read> colorMap [[ texture(TextureIndexColor) ]],
    constant UniformsCS& uniforms [[ buffer(BufferIndexUniformsCS) ]],
    uint2 index [[thread_position_in_grid]],
    texture2d<float, access::write> result [[ texture(TextureIndexSamples) ]]
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint2 uv = uniforms.uv; // tie into texture lookup
    // uv >>= uniforms.mipLOD;
    
    // the color is returned to linear rgba32f
    float4 color = colorMap.read(uv, uniforms.mipLOD);
    result.write(color, index);
}

kernel void SampleImageArrayCS(
    texture2d_array<float, access::read> colorMap [[ texture(TextureIndexColor) ]],
    constant UniformsCS& uniforms [[ buffer(BufferIndexUniformsCS) ]],
    uint2 index [[thread_position_in_grid]],
    texture2d<float, access::write> result [[ texture(TextureIndexSamples) ]]
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint2 uv = uniforms.uv; // tie into texture lookup
    //uv >>= uniforms.mipLOD;
    
    uint arrayOrSlice = uniforms.arrayOrSlice;
    
    // the color is returned to linear rgba32f
    float4 color = colorMap.read(uv, arrayOrSlice, uniforms.mipLOD);
    result.write(color, index);
}

kernel void SampleCubeCS(
    texturecube<float, access::read> colorMap [[ texture(TextureIndexColor) ]],
    constant UniformsCS& uniforms [[ buffer(BufferIndexUniformsCS) ]],
    uint2 index [[thread_position_in_grid]],
    texture2d<float, access::write> result [[ texture(TextureIndexSamples) ]]
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint2 uv = uint2(uniforms.uv); // tie into texture lookup
    //uv >>= uniforms.mipLOD;
    
    uint face = uniforms.face;
    
    // the color is returned to linear rgba32f
    float4 color = colorMap.read(uv, face, uniforms.mipLOD);
    result.write(color, index);
}


kernel void SampleCubeArrayCS(
    texturecube_array<float, access::read> colorMap [[ texture(TextureIndexColor) ]],
    constant UniformsCS& uniforms [[ buffer(BufferIndexUniformsCS) ]],
    uint2 index [[thread_position_in_grid]],
    texture2d<float, access::write> result [[ texture(TextureIndexSamples) ]]
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint2 uv = uint2(uniforms.uv); // tie into texture lookup
    //uv >>= uniforms.mipLOD;
    
    uint face = uniforms.face;
    uint arrayOrSlice = uniforms.arrayOrSlice;
    
    // the color is returned to linear rgba32f
    float4 color = colorMap.read(uv, face, arrayOrSlice, uniforms.mipLOD);
    result.write(color, index);
}

kernel void SampleVolumeCS(
    texture3d<float, access::read> colorMap [[ texture(TextureIndexColor) ]],
    constant UniformsCS& uniforms [[ buffer(BufferIndexUniformsCS) ]],
    uint2 index [[thread_position_in_grid]],
    texture2d<float, access::write> result [[ texture(TextureIndexSamples) ]]
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint3 uv = uint3(uniforms.uv, uniforms.arrayOrSlice); // tie into texture lookup
    //uv >>= uniforms.mipLOD);
    
    // the color is returned to linear rgba32f
    float4 color = colorMap.read(uv, uniforms.mipLOD);
    result.write(color, index);
}


