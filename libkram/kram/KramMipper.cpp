// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramMipper.h"

#include <algorithm>
#include <cassert>

namespace kram {

using namespace std;
using namespace simd;

Mipper::Mipper() { initTables(); }

bool isPow2(int32_t num)
{
    // exclude 0
    return (num > 0) && ((num & (num - 1)) == 0);
}

int32_t nextPow2(int32_t num)
{
    if (isPow2(num))
        return num;

    int32_t powerOfTwo = 1;
    while (powerOfTwo < num)
        powerOfTwo <<= 1;

    return powerOfTwo;
}

inline uint8_t floatToUint8(float value)
{
    return (uint8_t)roundf(value * 255.0f);
}

inline Color Unormfloat4ToColor(float4 value)
{
    Color c;
    value = round(value * 255.0f);
    c.r = (uint8_t)value.x;
    c.g = (uint8_t)value.y;
    c.b = (uint8_t)value.z;
    c.a = (uint8_t)value.w;
    return c;
}

inline Color Snormfloat4ToColor(float4 value)
{
    Color c;
    value = round(127.0f * value) + float4(128.0f); 
    c.r = (uint8_t)value.x;
    c.g = (uint8_t)value.y;
    c.b = (uint8_t)value.z;
    c.a = (uint8_t)value.w;
    return c;
}

float linearToSRGBFunc(float lin)
{
    assert(lin >= 0.0f && lin <= 1.0f);
    return (lin < 0.00313066844250063f) ? (lin * 12.92f)
                                        : (1.055f * powf(lin, 1.0f / 2.4f) - 0.055f);
}

inline float srgbToLinearFunc(float s)
{
    assert(s >= 0.0f && s <= 1.0f);
    return (s < 0.0404482362771082f) ? (s / 12.92f)
                                     : powf((s + 0.055f) / 1.055f, 2.4f);
}

float4 linearToSRGB(float4 lin) {
    lin = saturate(lin);
    
    return float4m(
        linearToSRGBFunc(lin.x),
        linearToSRGBFunc(lin.y),
        linearToSRGBFunc(lin.z),
        lin.w
    );
}

//
// inline void color565To888(uint16_t endpoint, Color &c) {
//    uint16_t red = (endpoint >> 11)  & 31;
//    uint16_t green = (endpoint >> 5) & 63;
//    uint16_t blue = (endpoint >> 0)  & 31;
//
//    c.r = ( red << 3 ) | ( red >> 2 );
//    c.g = ( green << 2 ) | ( green >> 4 ); // why 4?
//    c.b = ( blue << 3 ) | ( blue >> 2 );
//}

void Mipper::initTables()
{
    // normalized [0,1]

    // https://entropymine.com/imageworsener/srgbformula/
    // https://stackoverflow.com/questions/34472375/linear-to-srgb-conversion
    for (int32_t i = 0; i < 256; ++i) {
        float s = i / 255.0;
        float lin = s;
        alphaToFloat[i] = lin;
        srgbToLinear[i] = srgbToLinearFunc(s);
        // linearTosrgb[i] = linearToSRGBFunc(lin);
    }

    // srgb to linear lowers values (0.5 -> 0.3)
    // linear to srgb raises values (0.3 -> 0.5)

    // line -> srgb 0.516 -> 23 / 31

    // what about grayscales?

    // on 5-bit table, not enough precision
    //   values are repeating as snapping occurs, so only a few unique values
    //   out of the 32
    //    float extra = 0.0; // 0.5/255.0;
    //
    //    for (int32_t i = 0; i < 32; ++i) {
    //        float lin = (i + extra) / 31.0;
    //        uint8_t value = (floatToUint8(linearToSRGBFunc(lin)) >> 3) & 31;
    //        linearToSrgb5[i] = value;
    //    }
    //    for (int32_t i = 0; i < 64; ++i) {
    //        float lin = (i + extra) / 63.0;
    //        uint8_t value = (floatToUint8(linearToSRGBFunc(lin)) >> 2) & 63;
    //        linearToSrgb6[i] = value;
    //    }

    // png is straight unmul rgb, a, convert that to linear premul, box filter
    // to build mips, and then go back to srgb (premul) compute area of alpha
    // channels for trees, and don't lerp to vertical

    // srgb -> linear, then premul, then re-encode
    // tables are 64k each, simd math is probably faster to compute

    // for (int32_t a = 0; a < 256; ++a) {
    //        for (int32_t i = 0; i < 256; ++i) {
    //            float s = srgbToLinear[i];
    //            srgbToLinearPremul[i] = s;
    //
    //            //float lin = alphaToFloat[i];
    //            //rgbToLinearPremul[i]  = lin;
    //            //linearPremulTosrgbPremul[a][i] =
    //            floatToUint8(linearTosrgb[floatToUint8(lin * alpha[a])]);
    //        }
    //}

    // just test code
#if KRAM_DEBUG
    {
        float lin = 0.5;
        lin = srgbToLinearFunc(linearToSRGBFunc(lin));

        float s = 0.5;
        s = srgbToLinearFunc(s);  // 0.21404
        s = linearToSRGBFunc(s);  // back to 0.5
    }
#endif
}

void Mipper::initPixelsHalfIfNeeded(ImageData& srcImage, bool doPremultiply, bool doPrezero,
                                    vector<half4>& halfImage) const
{
    Color zeroColor = { 0, 0, 0, 0 };
    float4 zeroColorf = float4m(0.0, 0.0f, 0.0f, 0.f); // need a constant for this
    half4 zeroColorh = toHalf4(zeroColorf);
    
    int32_t w = srcImage.width;
    int32_t h = srcImage.height;

    // do in-place mips to this if srgb or premul involved
    if (srcImage.isHDR) {
        // don't do all this if data is already float, data should already be in
        // floatImage.  But may need to do premultiply?
        assert(false);
    }
    else if (srcImage.isSRGB) {
        
        // this does srgb and premul conversion
        for (int32_t y = 0; y < h; y++) {
            int32_t y0 = y * w;
            for (int32_t x = 0; x < w; x++) {
                Color& c0 = srcImage.pixels[y0 + x];
                float4 cFloat =
                    {srgbToLinear[c0.r], srgbToLinear[c0.g],
                     srgbToLinear[c0.b], 1.0f};

                if (c0.a != 255) {
                    float alpha = alphaToFloat[c0.a];

                    if (doPremultiply) {
                        // premul and sets alpha
                        cFloat *= alpha;
                    }
                    else if (doPrezero) {
                        if (c0.a == 0) {
                            cFloat = zeroColorf;
                            c0 = zeroColor;
                        }
                        else {
                            cFloat.w = alpha;
                        }
                    }
                    else {
                        cFloat.w = alpha;
                    }
                }

                // TODO: 32F path
                // if (!floatImage.empty()) {
                //    floatImage[y0 + x] = cFloat;
                // }
                // else
                {
                    halfImage[y0 + x] = toHalf4(cFloat);
                }

                // only have to rewrite src alpha/color if there is alpha and it's premul
                if (doPremultiply && c0.a != 255) {
                    // need to overwrite the color 8-bit color too
                    // but this writes back to srgb for encoding
                    cFloat.x = linearToSRGBFunc(cFloat.x);
                    cFloat.y = linearToSRGBFunc(cFloat.y);
                    cFloat.z = linearToSRGBFunc(cFloat.z);

                    c0 = Unormfloat4ToColor(cFloat);
                }
            }
        }
    }
    else if (doPrezero) {
        // do premul conversion
        for (int32_t y = 0; y < h; y++) {
            int32_t y0 = y * w;
            for (int32_t x = 0; x < w; x++) {
                Color& c0 = srcImage.pixels[y0 + x];
                
                // TODO: assumes 16, need 32f path too
                if (c0.a == 0) {
                    c0 = zeroColor;
                    halfImage[y0 + x] = zeroColorh;
                }
                else {
                    float4 cFloat = {alphaToFloat[c0.r], alphaToFloat[c0.g],
                                    alphaToFloat[c0.b], alphaToFloat[c0.a]};
                    halfImage[y0 + x] = toHalf4(cFloat);
                }
            }
        }
    }
    else if (doPremultiply) {
        // do premul conversion
        for (int32_t y = 0; y < h; y++) {
            int32_t y0 = y * w;
            for (int32_t x = 0; x < w; x++) {
                Color& c0 = srcImage.pixels[y0 + x];
                float4 cFloat = {alphaToFloat[c0.r], alphaToFloat[c0.g],
                                 alphaToFloat[c0.b], 1.0f};

                // premul and sets alpha
                if (c0.a != 255) {
                    float alpha = alphaToFloat[c0.a];
                    cFloat *= alpha;
                }

                // TODO: 32F path
                // if (!floatImage.empty()) {
                //    floatImage[y0 + x] = cFloat;
                // }
                // else
                {
                    halfImage[y0 + x] = toHalf4(cFloat);
                }

                // only have to rewrite color if there is alpha
                if (c0.a != 255) {
                    // need to overwrite the color 8-bit color w/premul
                    c0 = Unormfloat4ToColor(cFloat);
                }
            }
        }
    }
}

// 128/255 in Rectangle128 is mapped to, but it came in as 0.5 in
// 20, 41, 20   in srgbToLinear conversion
// 26, 38, 26   in linearToSRGB conversion in remapEndpoint565

// void Mipper::remapToSrgbEndpoint565(uint16_t& endpoint) const {
//    // TODO: might need endian swap of bytes in endpoint, and then before
//    writing too
//    //uint8_t *endpoint8 = (uint8_t*)&endpoint;
//    //std::swap(endpoint8[0], endpoint8[1]);
//
//    // unpack from 565, and repack with srgb applied
//    uint16_t r = (endpoint >> 11) & 31;
//    uint16_t g = (endpoint >> 5)  & 63;
//    uint16_t b = (endpoint >> 0)  & 31;
//
//    r = linearToSrgb6[r];
//    g = linearToSrgb5[g];
//    b = linearToSrgb5[b];
//
//    endpoint = b | (g << 5) | (r << 11);
//    //std::swap(endpoint8[0], endpoint8[1]);
//}

inline int8_t signedConvertUint8(uint8_t x)
{
    // split into +/- values
    int32_t xx = (int32_t)x - 128;
    return (int8_t)xx;
}

void remapToSignedBCEndpoint88(uint16_t& endpoint)
{
    uint8_t e0val = endpoint & 0xFF;
    uint8_t e1val = (endpoint >> 8) & 0xFF;

    int8_t e0 = signedConvertUint8(e0val);
    int8_t e1 = signedConvertUint8(e1val);

    endpoint = (*(const uint8_t*)&e0) | ((*(const uint8_t*)&e1) << 8);
}

void Mipper::mipmap(const ImageData& srcImage, ImageData& dstImage) const
{
    dstImage.width = srcImage.width;
    dstImage.height = srcImage.height;
    dstImage.depth = srcImage.depth;
    
    mipDown(dstImage.width, dstImage.height, dstImage.depth);

    // this assumes that we can read mip-1 from srcImage
    mipmapLevel(srcImage, dstImage);
}

void Mipper::mipmapLevelOdd(const ImageData& srcImage, ImageData& dstImage) const
{
    int32_t width = srcImage.width;
    int32_t height = srcImage.height;
    
    // this can receive premul, srgb data
    // the mip chain is linear data only
    Color* cDstColor = dstImage.pixels;
    const Color* srcColor = srcImage.pixels;

    float4* cDstFloat = dstImage.pixelsFloat;
    const float4* srcFloat = srcImage.pixelsFloat;

    half4* cDstHalf = dstImage.pixelsHalf;
    const half4* srcHalf = srcImage.pixelsHalf;
    
    // Note the ptrs above may point to same memory
    
    int32_t dstIndex = 0;

    bool isOddX = width & 1;
    bool isOddY = height & 1;

    // advance always by 2, but sample from neighbors
    int32_t mipWidth = std::max(1, width / 2);
    int32_t mipHeight = std::max(1, height / 2);
    
    float invWidth = 1.0f/width;
    float invHeight = 1.0f/height;
    
    for (int32_t y = isOddY ? 1 : 0; y < height; y += 2) {
        int32_t ym = y - 1;
        int32_t y0 = y;
        int32_t y1 = y + 1;

        // weights
        int32_t mipY = y/2;
        float ymw = (mipHeight - mipY - 1) * invHeight;
        float y0w = mipHeight * invHeight;
        float y1w = mipY * invHeight;
        
        if (!isOddY) {
            ym = y; // weight is 0
            
            ymw = 0.0f;
            y0w = 0.5f;
            y1w = 0.5f;
        }
        
        ym *= width;
        y0 *= width;
        y1 *= width;

        for (int32_t x = isOddX ? 1 : 0; x < width; x += 2) {
 
            int32_t xm = x - 1;
            int32_t x0 = x;
            int32_t x1 = x + 1;
 
            // weights
            int32_t mipX = x/2;
            float xmw = (mipWidth - mipX - 1) * invWidth;
            float x0w = mipWidth * invWidth;
            float x1w = mipX * invWidth;
            
            if (!isOddX) {
                xm = x; // weight is 0
                
                xmw = 0.0f;
                x0w = 0.5f;
                x1w = 0.5f;
            }
            
            // we have 3x2, 2x3 or 3x3 pattern to weight
            // now lookup the 9 values from the buffer
            
            float4 c[9];
            
            if (srcHalf) {
                c[0] = toFloat4(srcHalf[ym + xm]);
                c[1] = toFloat4(srcHalf[ym + x0]);
                c[2] = toFloat4(srcHalf[ym + x1]);
                
                c[3] = toFloat4(srcHalf[y0 + xm]);
                c[4] = toFloat4(srcHalf[y0 + x0]);
                c[5] = toFloat4(srcHalf[y0 + x1]);
                
                c[6] = toFloat4(srcHalf[y1 + xm]);
                c[7] = toFloat4(srcHalf[y1 + x0]);
                c[8] = toFloat4(srcHalf[y1 + x1]);
            }
            else if (srcFloat) {
                c[0] = srcFloat[ym + xm];
                c[1] = srcFloat[ym + x0];
                c[2] = srcFloat[ym + x1];
                
                c[3] = srcFloat[y0 + xm];
                c[4] = srcFloat[y0 + x0];
                c[5] = srcFloat[y0 + x1];
                
                c[6] = srcFloat[y1 + xm];
                c[7] = srcFloat[y1 + x0];
                c[8] = srcFloat[y1 + x1];
            }
            else {
                c[0] = ColorToUnormFloat4(srcColor[ym + xm]);
                c[1] = ColorToUnormFloat4(srcColor[ym + x0]);
                c[2] = ColorToUnormFloat4(srcColor[ym + x1]);
                
                c[3] = ColorToUnormFloat4(srcColor[y0 + xm]);
                c[4] = ColorToUnormFloat4(srcColor[y0 + x0]);
                c[5] = ColorToUnormFloat4(srcColor[y0 + x1]);
                
                c[6] = ColorToUnormFloat4(srcColor[y1 + xm]);
                c[7] = ColorToUnormFloat4(srcColor[y1 + x0]);
                c[8] = ColorToUnormFloat4(srcColor[y1 + x1]);
            }
                
            // apply weights to columns/rows
            for (int32_t i = 0; i < 3; i++) {
                c[3*i+0] *= xmw;
                c[3*i+1] *= x0w;
                c[3*i+2] *= x1w;
            }
             
            for (int32_t i = 0; i < 3; i++) {
                c[0+i] *= ymw;
                c[3+i] *= y0w;
                c[6+i] *= y1w;
            }
                
            // add them all up
            float4 cFloat = c[0];
            for (int32_t i = 1; i < 9; ++i) {
                cFloat += c[i];
            }
                
            if (srcHalf) {

                // overwrite float4 image
                cDstHalf[dstIndex] = toHalf4(cFloat);

                // assume hdr pulls from half/float data
                if (!srcImage.isHDR) {
                    // convert back to srgb for encode
                    if (srcImage.isSRGB) {
                        cFloat.x = linearToSRGBFunc(cFloat.x);
                        cFloat.y = linearToSRGBFunc(cFloat.y);
                        cFloat.z = linearToSRGBFunc(cFloat.z);
                    }

                    // override rgba8u version, since this is what is encoded
                    Color c = Unormfloat4ToColor(cFloat);

                    // can only skip this if cSrc = cDst
                    cDstColor[dstIndex] = c;
                }
            }
            else if (srcFloat) {

                // overwrite float4 image
                cDstFloat[dstIndex] = cFloat;

                // assume hdr pulls from half/float data
                if (!srcImage.isHDR) {
                    // convert back to srgb for encode
                    if (srcImage.isSRGB) {
                        cFloat.x = linearToSRGBFunc(cFloat.x);
                        cFloat.y = linearToSRGBFunc(cFloat.y);
                        cFloat.z = linearToSRGBFunc(cFloat.z);
                    }

                    // Overwrite the RGBA8u image too (this will go out to
                    // encoder) that means BC/ASTC are linearly fit to
                    // non-linear srgb colors - ick
                    Color c = Unormfloat4ToColor(cFloat);
                    cDstColor[dstIndex] = c;
                }
            }
            else {

                // can overwrite memory on linear image, some precision loss, but fast
                Color c = Unormfloat4ToColor(cFloat);
                cDstColor[dstIndex] = c;
            }

            dstIndex++;
        }
    }
}

    
void Mipper::mipmapLevel(const ImageData& srcImage, ImageData& dstImage) const
{
    int32_t width = srcImage.width;
    int32_t height = srcImage.height;

    bool isOddX = width & 1;
    bool isOddY = height & 1;
    
    if (isOddX || isOddY) {
        mipmapLevelOdd(srcImage, dstImage);
        return;
    }
    
    // fast path for 2x2 downsample below, can do in 4 taps
    
    // this can receive premul, srgb data
    // the mip chain is linear data only
    Color* cDstColor = dstImage.pixels;
    const Color* srcColor = srcImage.pixels;

    float4* cDstFloat = dstImage.pixelsFloat;
    const float4* srcFloat = srcImage.pixelsFloat;

    half4* cDstHalf = dstImage.pixelsHalf;
    const half4* srcHalf = srcImage.pixelsHalf;
    
    // Note the ptrs above may point to same memory
    
    int32_t dstIndex = 0;

    for (int32_t y = 0; y < height; y += 2) {
        int32_t y0 = y;
        int32_t y1 = y + 1;
        y0 *= width;
        y1 *= width;

        for (int32_t x = 0; x < width; x += 2) {
            int32_t x0 = x;
            int32_t x1 = x + 1;

            if (srcHalf) {
                float4 c0, c1, c2, c3;
                c0 = toFloat4(srcHalf[y0 + x0]);
                c1 = toFloat4(srcHalf[y0 + x1]);
                c2 = toFloat4(srcHalf[y1 + x0]);
                c3 = toFloat4(srcHalf[y1 + x1]);

                // mip filter is simple box filter
                // assumes alpha premultiplied already
                float4 cFloat = (c0 + c1 + c2 + c3) * 0.25;

                // overwrite float4 image
                cDstHalf[dstIndex] = toHalf4(cFloat);

                // assume hdr pulls from half/float data
                if (!srcImage.isHDR) {
                    // convert back to srgb for encode
                    if (srcImage.isSRGB) {
                        cFloat.x = linearToSRGBFunc(cFloat.x);
                        cFloat.y = linearToSRGBFunc(cFloat.y);
                        cFloat.z = linearToSRGBFunc(cFloat.z);
                    }

                    // override rgba8u version, since this is what is encoded
                    Color c = Unormfloat4ToColor(cFloat);

                    // can only skip this if cSrc = cDst
                    cDstColor[dstIndex] = c;
                }
            }
            else if (srcFloat) {
                const float4& c0 = srcFloat[y0 + x0];
                const float4& c1 = srcFloat[y0 + x1];

                const float4& c2 = srcFloat[y1 + x0];
                const float4& c3 = srcFloat[y1 + x1];

                // mip filter is simple box filter
                // assumes alpha premultiplied already
                float4 cFloat = (c0 + c1 + c2 + c3) * 0.25;

                // overwrite float4 image
                cDstFloat[dstIndex] = cFloat;

                // assume hdr pulls from half/float data
                if (!srcImage.isHDR) {
                    // convert back to srgb for encode
                    if (srcImage.isSRGB) {
                        cFloat.x = linearToSRGBFunc(cFloat.x);
                        cFloat.y = linearToSRGBFunc(cFloat.y);
                        cFloat.z = linearToSRGBFunc(cFloat.z);
                    }

                    // Overwrite the RGBA8u image too (this will go out to
                    // encoder) that means BC/ASTC are linearly fit to
                    // non-linear srgb colors - ick
                    Color c = Unormfloat4ToColor(cFloat);
                    cDstColor[dstIndex] = c;
                }
            }
            else {
                // faster 8-bit only path for LDR and unmultiplied
                const Color& c0 = srcColor[y0 + x0];
                const Color& c1 = srcColor[y0 + x1];

                const Color& c2 = srcColor[y1 + x0];
                const Color& c3 = srcColor[y1 + x1];

                // 8-bit box filter, with +2/4 for rounding
                int32_t r = ((int32_t)c0.r + (int32_t)c1.r + (int32_t)c2.r + (int32_t)c3.r + 2) / 4;
                int32_t g = ((int32_t)c0.g + (int32_t)c1.g + (int32_t)c2.g + (int32_t)c3.g + 2) / 4;
                int32_t b = ((int32_t)c0.b + (int32_t)c1.b + (int32_t)c2.b + (int32_t)c3.b + 2) / 4;
                int32_t a = ((int32_t)c0.a + (int32_t)c1.a + (int32_t)c2.a + (int32_t)c3.a + 2) / 4;

                // can overwrite memory on linear image, some precision loss, but fast
                Color c = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
                cDstColor[dstIndex] = c;
            }

            dstIndex++;
        }
    }
}

}  // namespace kram
