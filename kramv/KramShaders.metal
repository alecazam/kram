// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import "KramShaders.h"

using namespace metal;

//---------------------------------
// helpers

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

// TODO: note that Metal must pass the same half3 from vertex to fragment shader
// so can't mix a float vs with half fs.

//-------------------------------------------
// functions

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

// use mikktspace, gen bitan in frag shader with sign, don't normalize vb/vt
// see http://www.mikktspace.com/
half3 transformNormal(half3 bumpNormal, half4 tangent, half3 vertexNormal)
{
    // Normalize tangent/vertexNormal in vertex shader
    // but don't renormalize interpolated tangent, vertexNormal in fragment shader
    // Reconstruct bitan in frag shader
    // https://bgolus.medium.com/generating-perfect-normal-maps-for-unity-f929e673fc57

    half bitangentSign = tangent.w;
    
    // ModelIO not generating correct bitan sign
    // DONE: flip this on srcData, and not here
    //bitangentSign = -bitangentSign;
    
    // now transform by basis and normalize from any shearing, and since interpolated basis vectors
    // are not normalized
    half3 bitangent =  bitangentSign * cross(vertexNormal, tangent.xyz);
    half3x3 tbn = half3x3(tangent.xyz, bitangent, vertexNormal);
    bumpNormal = tbn * bumpNormal;
    return normalize(bumpNormal);
}

half3 transformNormal(half4 tangent, half3 vertexNormal,
                      texture2d<half> texture, sampler s, float2 uv, bool isSigned = true)
{
    half4 nmap = texture.sample(s, uv);
    
    // unorm-only formats like ASTC need to convert
    if (!isSigned) {
        nmap.xy = toSnorm8(nmap.xy);
    }
    
    // rebuild the z term
    half3 bumpNormal = toNormal(nmap.xyz);
   
    return transformNormal(bumpNormal,
                           tangent, vertexNormal);
}



// TODO: have more bones, or read from texture instead of uniforms
// can then do instanced skining, but vfetch lookup slower
#define maxBones 128

// this is for vertex shader
void skinPosAndBasis(thread float4& position, thread float3& tangent, thread float3& normal,
                     uint4 indices, float4 weights, float3x4 bones[maxBones])
{
    // TODO: might do this as up to 12x vtex lookup, fetch from buffer texture
    // but uniforms after setup would be faster if many bones
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
    tangent = (float4(tangent, 0.0) * bindPoseToBoneTransform);
}

float3x3 toFloat3x3(float4x4 m)
{
    return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}

// this is for vertex shader if tangent supplied
void transformBasis(thread float3& normal, thread float3& tangent,
                    float4x4 modelToWorldTfm, float3 invScale2)
{
    
    float3x3 m = toFloat3x3(modelToWorldTfm);
    
    // note this is RinvT * n = (Rt)t = R, this is for simple inverse, inv scale handled below
    // but uniform scale already handled by normalize
    normal = m * normal;
       
    // question here of whether tangent is transformed by m or mInvT
    // most apps assume m, but after averaging it can be just as off the surface as the normal
    tangent = m * tangent;
    
    // have to apply invSquare of scale here to approximate invT
    // also make sure to identify inversion off determinant before instancing so that backfacing is correct
    // this is only needed if non-uniform scale present in modelToWorldTfm, could precompute scale2
//    if (isScaled)
//    {
//        // compute scale squared from rows
//        float3 scale2 = float3(
//            length_squared(m[0].xyz),
//            length_squared(m[1].xyz),
//            length_squared(m[2].xyz));
//
//        // do a max(1e4), but really don't have scale be super small
//        scale2 = recip(max(0.0001 * 0.0001, scale2));
        
        // apply inverse
        normal  *= invScale2;
        tangent *= invScale2;
//    }
    
    // vertex shader normalize, but the fragment shader should not
    normal  = normalize(normal);
    tangent = normalize(tangent);
    
    // make sure to preserve bitan sign in tangent.w
}

//-------------------------------------------

struct Vertex
{
    float4 position [[attribute(VertexAttributePosition)]];
    float2 texCoord [[attribute(VertexAttributeTexcoord)]];
    
    // basis
    float3 normal [[attribute(VertexAttributeNormal)]];; // consider hallf
    float4 tangent [[attribute(VertexAttributeTangent)]];; // tan + bitanSign
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
    
    if (uniforms.isNormal && uniforms.isPreview) {
        float3 normal = in.normal;
        float3 tangent = in.tangent.xyz;
        transformBasis(normal, tangent, uniforms.modelMatrix, uniforms.modelMatrixInvScale2);
        
        out.normal = toHalf(normal);
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
    out.texCoord.xy = in.texCoord;
    if (uniforms.isWrap) {
        // can make this a repeat value uniform
        float wrapAmount = 2.0;
        out.texCoord.xy *= wrapAmount;
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
    float3 uvw = out.texCoordXYZ;
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


// TODO: do more test shapes, but that affects eyedropper
// generate and pass down tangents + bitanSign in the geometry

// TODO: eliminate the toUnorm() calls below, rendering to rgba16f
// but also need to remove conversion code on cpu side expecting unorm in eyedropper

float4 DrawPixels(
    ColorInOut in [[stage_in]],
    bool facing [[front_facing]],
    constant Uniforms& uniforms,
    float4 c,
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

            // typicaly source recommends smoothstep, so that get a soft instead of hard ramp of alpha at edges
            
            // store as preml alpha
            c.rgba = saturate(pixelDist);
        }
        else if (uniforms.isNormal) {
            // light the normal map
            
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
            
            // flip the normal if facing is flipped
            // TODO: needed for tangent too?
            if (!facing) {
                c.xyz = -c.xyz;
                in.tangent.w = -in.tangent.w;
            }
            
            float3 lightDir = normalize(float3(1,1,1));
            float3 lightColor = float3(1,1,1);
            
            float3 n = c.xyz;
            
            // handle the basis here
            n = toFloat(transformNormal(toHalf(n), in.tangent, in.normal));
            
            // diffuse
            float dotNLUnsat = dot(n, lightDir);
            float dotNL = saturate(dotNLUnsat);
            float3 diffuse = lightColor.xyz * dotNL;
            
            float3 specular = float3(0.0);
            
            // this renders bright in one quadrant of wrap preview, hard in ortho view
            // specular
            bool doSpecular = false;
            if (doSpecular) {
                float3 view = normalize(in.worldPos - uniforms.cameraPosition);
                float3 ref = normalize(reflect(view, n));
                
                // above can be interpolated
                float dotRL = saturate(dot(ref, lightDir));
                dotRL = pow(dotRL, 4.0); // * saturate(dotNL * 8.0);  // no spec without diffuse
                specular = saturate(dotRL * lightColor.rgb);
            }
            
            // Note: don't have any albedo yet, need second texture input
            float3 ambient = mix(0.1, 0.3, saturate(dotNLUnsat * 0.5 + 0.5));
            c.xyz = ambient + diffuse + specular;
            
            c.a = 1;
            
            // TODO: add some specular, can this be combined with albedo texture in same folder?
            // may want to change perspective for that, and give light controls
        }
        else {
            // to unorm
            if (uniforms.isSigned) {
                c.xyz = toUnorm(c.xyz);
            }
            
            // to premul, but also need to see without premul
            if (uniforms.isPremul) {
                c.xyz *= c.a;
            }
        }
        
        bool doShowUV = false;
        if (doShowUV) {
            c = float4(in.texCoord, 0.0, 1.0);
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
        
        c.rgb = c.rgb + (1-c.a) * cb;
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
            
            c.rgb = float3(lineIntensity) + (1.0 - lineIntensity) * c.rgb;
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

    return DrawPixels(in, facing, uniforms, c, textureSize);
}

fragment float4 DrawImagePS(
    ColorInOut in [[stage_in]],
    bool facing [[front_facing]],
    constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
    constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texture2d<float> colorMap [[ texture(TextureIndexColor) ]]
)
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.xy);
    
    // here are the pixel dimensions of the lod
    uint lod = uniformsLevel.mipLOD;
    float2 textureSize = float2(colorMap.get_width(lod), colorMap.get_height(lod));
    // colorMap.get_num_mip_levels();

    return DrawPixels(in, facing, uniforms, c, textureSize);
}

fragment float4 DrawImageArrayPS(
    ColorInOut in [[stage_in]],
    bool facing [[front_facing]],
    constant Uniforms& uniforms [[ buffer(BufferIndexUniforms) ]],
    constant UniformsLevel& uniformsLevel [[ buffer(BufferIndexUniformsLevel) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texture2d_array<float> colorMap [[ texture(TextureIndexColor) ]]
)
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.xy, uniformsLevel.arrayOrSlice);
    
    // here are the pixel dimensions of the lod
    uint lod = uniformsLevel.mipLOD;
    float2 textureSize = float2(colorMap.get_width(lod), colorMap.get_height(lod));
    // colorMap.get_num_mip_levels();

    return DrawPixels(in, facing, uniforms, c, textureSize);
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

    return DrawPixels(in, facing, uniforms, c, textureSize);
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

    return DrawPixels(in, facing, uniforms, c, textureSize);
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

    return DrawPixels(in, facing, uniforms, c, textureSize);
}

//--------------------------------------------------

/* not using this yet, need a fsq and some frag coord to sample the normal map at discrete points
 
// https://www.shadertoy.com/view/4s23DG
// 2D vector field visualization by Morgan McGuire, @morgan3d, http://casual-effects.com

constant float PI = 3.1415927;

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
    texture2d<float, access::write> result
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
    texture2d<float, access::write> result
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint2 uv = uniforms.uv; // tie into texture lookup
    // uv >>= uniforms.mipLOD;
    
    // the color returned is linear
    float4 color = colorMap.read(uv, uniforms.mipLOD);
    result.write(color, index);
}

kernel void SampleImageArrayCS(
    texture2d_array<float, access::read> colorMap [[ texture(TextureIndexColor) ]],
    constant UniformsCS& uniforms [[ buffer(BufferIndexUniformsCS) ]],
    uint2 index [[thread_position_in_grid]],
    texture2d<float, access::write> result
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint2 uv = uniforms.uv; // tie into texture lookup
    //uv >>= uniforms.mipLOD;
    
    uint arrayOrSlice = uniforms.arrayOrSlice;
    
    // the color returned is linear
    float4 color = colorMap.read(uv, arrayOrSlice, uniforms.mipLOD);
    result.write(color, index);
}

kernel void SampleCubeCS(
    texturecube<float, access::read> colorMap [[ texture(TextureIndexColor) ]],
    constant UniformsCS& uniforms [[ buffer(BufferIndexUniformsCS) ]],
    uint2 index [[thread_position_in_grid]],
    texture2d<float, access::write> result
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint2 uv = uint2(uniforms.uv); // tie into texture lookup
    //uv >>= uniforms.mipLOD;
    
    uint face = uniforms.face;
    
    // This writes out linear float32, can do srgb conversion on cpu side
    
    // the color returned is linear
    float4 color = colorMap.read(uv, face, uniforms.mipLOD);
    result.write(color, index);
}


kernel void SampleCubeArrayCS(
    texturecube_array<float, access::read> colorMap [[ texture(TextureIndexColor) ]],
    constant UniformsCS& uniforms [[ buffer(BufferIndexUniformsCS) ]],
    uint2 index [[thread_position_in_grid]],
    texture2d<float, access::write> result
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint2 uv = uint2(uniforms.uv); // tie into texture lookup
    //uv >>= uniforms.mipLOD;
    
    uint face = uniforms.face;
    uint arrayOrSlice = uniforms.arrayOrSlice;
    
    // the color returned is linear
    float4 color = colorMap.read(uv, face, arrayOrSlice, uniforms.mipLOD);
    result.write(color, index);
}

kernel void SampleVolumeCS(
    texture3d<float, access::read> colorMap [[ texture(TextureIndexColor) ]],
    constant UniformsCS& uniforms [[ buffer(BufferIndexUniformsCS) ]],
    uint2 index [[thread_position_in_grid]],
    texture2d<float, access::write> result
)
{
    // the for-loop is replaced with a collection of threads, each of which
    // calls this function.
    uint3 uv = uint3(uniforms.uv, uniforms.arrayOrSlice); // tie into texture lookup
    //uv >>= uniforms.mipLOD);
    
    // the color returned is linear
    float4 color = colorMap.read(uv, uniforms.mipLOD);
    result.write(color, index);
}


