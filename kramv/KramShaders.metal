// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import "KramShaders.h"

using namespace metal;

struct Vertex
{
    float4 position [[attribute(VertexAttributePosition)]];
    float2 texCoord [[attribute(VertexAttributeTexcoord)]];
};

struct ColorInOut
{
    float4 position [[position]];
    float3 texCoordXYZ;
    float2 texCoord;
    float3 worldPos;
};

ColorInOut DrawImageFunc(Vertex in [[stage_in]],
                        constant Uniforms & uniforms)
{
    ColorInOut out;

    float4 worldPos = uniforms.modelMatrix * in.position;
    out.position = uniforms.projectionViewMatrix * worldPos;
    
    // this is a 2d coord always which is 0 to 1, or 0 to 2
    out.texCoord.xy = in.texCoord;
    if (uniforms.isWrap) {
        out.texCoord.xy *= 2.0; // can make this a repeat value uniform
    }
   
    // potentially 3d coord, and may be -1 to 1
    out.texCoordXYZ.xy = out.texCoord;
    out.texCoordXYZ.z = 0.0;
   
    out.worldPos = worldPos.xyz;
    return out;
}

vertex ColorInOut DrawImageVS(Vertex in [[stage_in]],
                               constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]])
{
    return DrawImageFunc(in, uniforms);
}

vertex ColorInOut DrawCubeVS(Vertex in [[stage_in]],
                               constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]])
{
    ColorInOut out = DrawImageFunc(in, uniforms);
    
    // convert to -1 to 1
    float3 uvw = out.texCoordXYZ;
    uvw.xy = uvw.xy * 2.0 - 1.0;
    uvw.z = 1.0;
    
    // switch to the face
    switch(uniforms.face) {
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
    constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]]
)
{
    ColorInOut out = DrawImageFunc(in, uniforms);
    
    // this is normalized in ps by dividing by depth-1
    out.texCoordXYZ.z = uniforms.arrayOrSlice;
    return out;
}

float4 DrawPixels(
    ColorInOut in [[stage_in]],
    constant Uniforms & uniforms,
    float4 c,
    float2 textureSize
)
{
    bool isPreview = uniforms.isPreview;
    if (isPreview) {
        
        if (uniforms.isSDF) {
            if (!uniforms.isSigned) {
                // convert to signed normal to compute z
                c.r = 2.0 * c.r - 256.0 / 255.0; // 0 = 128 on unorm data on 8u
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
            float onePixel = 1.0 / max(0.0001, length(float2(dfdx(dist), dfdy(dist))));

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
                c.rg = 2.0 * c.rg - float2(256.0 / 255.0); // 0 = 128 on unorm data on 8u
            }
            
            c.z = sqrt(1 - saturate(dot(c.xy, c.xy))); // z always positive
            
            float3 lightDir = normalize(float3(1,1,1));
            float3 lightColor = float3(1,1,1);
            
            float3 n = c.xyz;
            
            // diffuse
            float dotNL = saturate(dot(n, lightDir));
            float3 diffuse = lightColor.xyz * dotNL;
            
            float3 specular = float3(0.0);
            
            // this renders bright in one quadrant of wrap preview
            // specular
            //float3 v = normalize(in.worldPos); //  - worldCameraPos); // or worldCameraDir
            //float3 r = normalize(reflect(lightDir, n));
            //float dotRV = saturate(dot(r, v));
            //dotRV = pow(dotRV, 4.0); // * saturate(dotNL * 8.0);  // no spec without diffuse
            //specular = saturate(dotRV * lightColor.rgb);

            float3 ambient = float3(0.1);
            c.xyz = ambient + diffuse + specular;
            
            c.a = 1;
            
            // TODO: add some specular, can this be combined with albedo texture in same folder?
            // may want to change perspective for that, and give light controls
        }
        else {
            // to unorm
            if (uniforms.isSigned) {
                c.xyz = c.xyz * 0.5 + 0.5;
            }
            
            // to premul, but also need to see without premul
            if (uniforms.isPremul) {
                c.xyz *= c.a;
            }
        }
    }
    else {
        // handle single channel and SDF content
        if (uniforms.numChannels == 1) {
            // toUnorm
            if (uniforms.isSigned) {
                c.x = c.x * 0.5 + 0.5;
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
                c.rg = 2.0 * c.rg - float2(256.0 / 255.0); // 0 = 128 on unorm data
            }
            
            c.z = sqrt(1 - saturate(dot(c.xy, c.xy))); // z always positive
            
            // from signed, to match other editors that don't display signed data
            c.xyz = c.xyz * 0.5 + 0.5; // can sample from this
            
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
                c.xyz = c.xyz * 0.5 + 0.5;
            }
            
            // to premul, but also need to see without premul
            if (uniforms.isPremul) {
                c.xyz *= c.a;
            }
        }
    }
    
    // mask to see one channel in isolation, this is really 0'ing out other channels
    // would be nice to be able to keep this set on each channel independently.
    switch(uniforms.channels)
    {
        case ModeRGBA: break;
        case ModeR001: c = float4(c.r,0,0,1); break;
        case Mode0G01: c = float4(0,c.g,0,1); break;
        case Mode00B1: c = float4(0,0,c.b,1); break;
            
        case ModeRRR1: c = float4(c.rrr,1); break;
        case ModeGGG1: c = float4(c.ggg,1); break;
        case ModeBBB1: c = float4(c.bbb,1); break;
            
        case ModeAAA1: c = float4(c.aaa,1); break;
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

    
    
    if (uniforms.debugMode != DebugModeNone && c.a != 0.0f) {
        
        bool isHighlighted = false;
        if (uniforms.debugMode == DebugModeTransparent) {
            if (c.a < 1.0) {
                isHighlighted = true;
            }
        }
        else if (uniforms.debugMode == DebugModeColor) {
            // with 565 formats, all pixels with light up
            if (c.r != c.g || c.r != c.b) {
                isHighlighted = true;
            }
        }
        else if (uniforms.debugMode == DebugModeGray) {
            // with 565 formats, all pixels with light up
            if (c.r == c.g && c.r == c.b) {
                isHighlighted = true;
            }
        }
        else if (uniforms.debugMode == DebugModeHDR) {
            if (any(c.rgb < float3(0.0)) || any(c.rgb < float3(0.0)) ) {
                isHighlighted = true;
            }
        }
        
        // signed data already convrted to unorm above, so compare to 0.5
        else if (uniforms.debugMode == DebugModePosX) {
            // two channels here, would need to color each channel
            if (c.r >= 0.5) {
                isHighlighted = true;
            }
        }
        else if (uniforms.debugMode == DebugModePosY) {
            if (c.g > 0.5) {
                isHighlighted = true;
            }
        }
        
        // TODO: is it best to highlight the interest pixels in red
        // or the negation of that to see which ones aren't.
        if (isHighlighted) {
            float3 highlightColor = float3(1.0f, 0.0f, 0.0f);
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
        float2 lineWidth = 1.0 / fwidth(pixels);
        
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
    constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texture1d_array<float> colorMap [[ texture(TextureIndexColor) ]])
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.x, uniforms.arrayOrSlice);
    
    // here are the pixel dimensions of the lod
    // uint lod = uniforms.mipLOD;
    // mips not supported on 1D textures according to Metal spec.
    
    float2 textureSize = float2(colorMap.get_width(0), 1);
    // colorMap.get_num_mip_levels();

    return DrawPixels(in, uniforms, c, textureSize);
}

fragment float4 DrawImagePS(
    ColorInOut in [[stage_in]],
    constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texture2d<float> colorMap [[ texture(TextureIndexColor) ]])
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.xy);
    
    // here are the pixel dimensions of the lod
    uint lod = uniforms.mipLOD;
    float2 textureSize = float2(colorMap.get_width(lod), colorMap.get_height(lod));
    // colorMap.get_num_mip_levels();

    return DrawPixels(in, uniforms, c, textureSize);
}

fragment float4 DrawImageArrayPS(
    ColorInOut in [[stage_in]],
    constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texture2d_array<float> colorMap [[ texture(TextureIndexColor) ]])
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.xy, uniforms.arrayOrSlice);
    
    // here are the pixel dimensions of the lod
    uint lod = uniforms.mipLOD;
    float2 textureSize = float2(colorMap.get_width(lod), colorMap.get_height(lod));
    // colorMap.get_num_mip_levels();

    return DrawPixels(in, uniforms, c, textureSize);
}


fragment float4 DrawCubePS(
    ColorInOut in [[stage_in]],
    constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texturecube<float> colorMap [[ texture(TextureIndexColor) ]])
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.xyz);
    
    // here are the pixel dimensions of the lod
    uint lod = uniforms.mipLOD;
    float w = colorMap.get_width(lod);
    float2 textureSize = float2(w, w);
    // colorMap.get_num_mip_levels();

    return DrawPixels(in, uniforms, c, textureSize);
}

fragment float4 DrawCubeArrayPS(
    ColorInOut in [[stage_in]],
    constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texturecube_array<float> colorMap [[ texture(TextureIndexColor) ]])
{
    float4 c = colorMap.sample(colorSampler, in.texCoordXYZ.xyz, uniforms.arrayOrSlice);
    
    // here are the pixel dimensions of the lod
    uint lod = uniforms.mipLOD;
    float w = colorMap.get_width(lod);
    float2 textureSize = float2(w, w);
    // colorMap.get_num_mip_levels();

    return DrawPixels(in, uniforms, c, textureSize);
}


fragment float4 DrawVolumePS(
    ColorInOut in [[stage_in]],
    constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
    sampler colorSampler [[ sampler(SamplerIndexColor) ]],
    texture3d<float> colorMap [[ texture(TextureIndexColor) ]])
{
    // fix up the tex coordinate here to be normalized w = [0,1]
    uint lod = uniforms.mipLOD;
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

    return DrawPixels(in, uniforms, c, textureSize);
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
    uv = max(uint2(1), uv >> uniforms.mipLOD);
    
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
    uv = max(uint2(1), uv >> uniforms.mipLOD);
    
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
    uv = max(uint2(1), uv >> uniforms.mipLOD);
    
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
    uv = max(uint2(1), uv >> uniforms.mipLOD);
    
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
    uv = max(uint3(1), uv >> uniforms.mipLOD);
    
    // the color returned is linear
    float4 color = colorMap.read(uv, uniforms.mipLOD);
    result.write(color, index);
}


