// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KTXMipper.h"

#include <cassert>

namespace kram {

using namespace std;
USING_SIMD;

Mipper::Mipper() { initTables(); }

bool isPow2(int num)
{
    // exclude 0
    return (num > 0) && ((num & (num - 1)) == 0);
}

int nextPow2(int num)
{
    if (isPow2(num))
        return num;

    int powerOfTwo = 1;
    while (powerOfTwo < num)
        powerOfTwo <<= 1;

    return powerOfTwo;
}

inline uint8_t floatToUint8(float value)
{
    return (uint8_t)roundf(value * 255.1);
}

inline Color Unormfloat4ToColor(float4 value)
{
    Color c;
    value = round(value * 255.1);
    c.r = (uint8_t)value.x;
    c.g = (uint8_t)value.y;
    c.b = (uint8_t)value.z;
    c.a = (uint8_t)value.w;
    return c;
}

inline Color Snormfloat4ToColor(float4 value)
{
    Color c;
    value = round(127 * value) + float4(127.0);  // or is it 128?
    c.r = (uint8_t)value.x;
    c.g = (uint8_t)value.y;
    c.b = (uint8_t)value.z;
    c.a = (uint8_t)value.w;
    return c;
}

inline float linearToSRGBFunc(float lin)
{
    assert(lin >= 0.0 && lin <= 1.0);
    return (lin < 0.00313066844250063) ? (lin * 12.92)
                                       : (1.055 * powf(lin, 1.0 / 2.4) - 0.055);
}

inline float srgbToLinearFunc(float s)
{
    assert(s >= 0.0 && s <= 1.0);
    return (s < 0.0404482362771082) ? (s / 12.92)
                                    : powf((s + 0.055) / 1.055, 2.4);
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
    // normalied [0,1]

    // https://entropymine.com/imageworsener/srgbformula/
    // https://stackoverflow.com/questions/34472375/linear-to-srgb-conversion
    for (int i = 0; i < 256; ++i) {
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
    //    for (int i = 0; i < 32; ++i) {
    //        float lin = (i + extra) / 31.0;
    //        uint8_t value = (floatToUint8(linearToSRGBFunc(lin)) >> 3) & 31;
    //        linearToSrgb5[i] = value;
    //    }
    //    for (int i = 0; i < 64; ++i) {
    //        float lin = (i + extra) / 63.0;
    //        uint8_t value = (floatToUint8(linearToSRGBFunc(lin)) >> 2) & 63;
    //        linearToSrgb6[i] = value;
    //    }

    // png is straight unmul rgb, a, convert that to linear premul, box filter
    // to build mips, and then go back to srgb (premul) compute area of alpha
    // channels for trees, and don't lerp to vertical

    // srgb -> linear, then premul, then re-encode
    // tables are 64k each, simd math is probably faster to compute

    // for (int a = 0; a < 256; ++a) {
    //        for (int i = 0; i < 256; ++i) {
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
#ifndef NDEBUG
    {
        float lin = 0.5;
        lin = srgbToLinearFunc(linearToSRGBFunc(lin));

        float s = 0.5;
        s = srgbToLinearFunc(s);  // 0.21404
        s = linearToSRGBFunc(s);  // back to 0.5
    }
#endif
}

void Mipper::initPixelsFloatIfNeeded(ImageData& srcImage, bool doPremultiply,
                                     vector<float4>& floatImage) const
{
    int w = srcImage.width;
    int h = srcImage.height;

    // do in-place mips to this if srgb or premul involved
    if (srcImage.isHDR) {
        // don't do all this if data is already float, data should already be in
        // floatImage
        assert(false);
    }
    else if (srcImage.isSRGB) {
        // this does srgb and premul conversion
        for (int y = 0; y < h; y++) {
            int y0 = y * w;
            for (int x = 0; x < w; x++) {
                Color& c0 = srcImage.pixels[y0 + x];
                float4& cFloat = floatImage[y0 + x];

                cFloat =
                    {srgbToLinear[c0.r], srgbToLinear[c0.g],
                     srgbToLinear[c0.b], 1.0f};
                assert(cFloat.x >= 0 && cFloat.x <= 1.0);

                // only have to rewrite src alpha/color if there is alpha
                if (c0.a != 255) {
                    float alpha = alphaToFloat[c0.a];

                    if (!doPremultiply) {
                        cFloat.w = alpha;
                    }
                    else {
                        cFloat *= alpha;
                        assert(cFloat.x >= 0 && cFloat.x <= 1.0);

                        // need to overwrite the color 8-bit color too
                        // but this writes back to srgb for encoding
                        float4 cFloatCopy = cFloat;
                        cFloatCopy.x = linearToSRGBFunc(cFloat.x);
                        cFloatCopy.y = linearToSRGBFunc(cFloat.y);
                        cFloatCopy.z = linearToSRGBFunc(cFloat.z);

                        c0 = Unormfloat4ToColor(cFloatCopy);
                    }
                }
            }
        }
    }
    else if (doPremultiply) {
        // do premul conversion
        for (int y = 0; y < h; y++) {
            int y0 = y * w;
            for (int x = 0; x < w; x++) {
                Color& c0 = srcImage.pixels[y0 + x];
                float4& cFloat = floatImage[y0 + x];

                // premul so mips are built properly, and lookups are correct
                // annoying no ctor
                cFloat = {alphaToFloat[c0.r], alphaToFloat[c0.g],
                          alphaToFloat[c0.b], 1.0f};
                assert(cFloat.x >= 0 && cFloat.x <= 1.0);

                // only have to rewrite color if there is alpha
                if (c0.a != 255) {
                    float alpha = alphaToFloat[c0.a];
                    cFloat *= alpha;
                    assert(cFloat.x >= 0 && cFloat.x <= 1.0);

                    // need to overwrite the color 8-bit color too
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

int8_t signedConvertUint8(uint8_t x)
{
    // split into +/- values
    int xx = (int)x - 128;
    return (int8_t)xx;
}

void Mipper::remapToSignedEndpoint88(uint16_t& endpoint) const
{
    uint8_t e0val = endpoint & 0xFF;
    uint8_t e1val = (endpoint >> 8) & 0xFF;

    int8_t e0 = signedConvertUint8(e0val);
    int8_t e1 = signedConvertUint8(e1val);

    endpoint = (*(const uint8_t*)&e0) | ((*(const uint8_t*)&e1) << 8);
}

void Mipper::remapToSignedEndpoint8(uint16_t& endpoint) const
{
    remapToSignedEndpoint88(endpoint);
}

void Mipper::mipmap(ImageData& srcImage, ImageData& dstImage) const
{
    dstImage.width = (srcImage.width + 1) / 2;
    dstImage.height = (srcImage.height + 1) / 2;

    // this assumes that we can read mip-1 from srcImage
    mipmapLevel(srcImage, dstImage);
}

void Mipper::mipmapLevel(ImageData& srcImage, ImageData& dstImage) const
{
    int width = srcImage.width;
    int height = srcImage.height;

    // this can receive premul, srgb data
    // the mip chain is linear data only
    Color* cDst = dstImage.pixels;

    float4* cDstFloat = srcImage.pixelsFloat;
    const float4* srcFloatImage = cDstFloat;

    for (int y = 0; y < height; y += 2) {
        int y0 = y;
        int y1 = y + 1;
        if (y1 == height) {
            y1 = y;
        }
        y0 *= width;
        y1 *= width;

        for (int x = 0; x < width; x += 2) {
            int x1 = x + 1;
            if (x1 == width) {
                x1 = x;
            }

            if (srcFloatImage) {
                const float4& c0 = srcFloatImage[y0 + x];
                const float4& c1 = srcFloatImage[y0 + x1];

                const float4& c2 = srcFloatImage[y1 + x];
                const float4& c3 = srcFloatImage[y1 + x1];

                // mip filter is simple box filter
                // assumes alpha premultiplied already
                float4 cFloat = (c0 + c1 + c2 + c3) * 0.25;

                // overwrite float4 image
                *cDstFloat = cFloat;
                cDstFloat++;

                if (!srcImage.isHDR) {
                    // it's premul linear now, but convert rgb to srgb before
                    // encode if needed ideally this should be done after
                    // endpoints picked by encoder but then must find endpoints
                    // for each compressed block type
                    if (srcImage.isSRGB) {
                        cFloat.x = linearToSRGBFunc(cFloat.x);
                        cFloat.y = linearToSRGBFunc(cFloat.y);
                        cFloat.z = linearToSRGBFunc(cFloat.z);
                    }

                    // Overwrite the RGBA8u image too (this will go out to
                    // encoder) that means BC/ASTC are linearly fit to
                    // non-linear srgb colors - ick
                    // TODO: ideally send linear color to endcoder, fit
                    // endpoints, then remap endpoints to srgb after But it's
                    // hard to find endpoints, and BC1-3 uses 565.  ASTC uses
                    // 888 but often stored base+offset.
                    Color c = Unormfloat4ToColor(cFloat);
                    *cDst = c;
                    cDst++;
                }
            }
            else {
                // faster 8-bit only path for LDR and unmultiplied
                const Color& c0 = srcImage.pixels[y0 + x];
                const Color& c1 = srcImage.pixels[y0 + x1];

                const Color& c2 = srcImage.pixels[y1 + x];
                const Color& c3 = srcImage.pixels[y1 + x1];

                // 8-bit box filter
                int r = ((int)c0.r + (int)c1.r + (int)c2.r + (int)c3.r + 2) / 4;
                int g = ((int)c0.g + (int)c1.g + (int)c2.g + (int)c3.g + 2) / 4;
                int b = ((int)c0.b + (int)c1.b + (int)c2.b + (int)c3.b + 2) / 4;

                int a = ((int)c0.a + (int)c1.a + (int)c2.a + (int)c3.a + 2) / 4;

                // can overwrite memory on linear image
                Color c = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
                *cDst = c;
                cDst++;
            }
        }
    }
}

}  // namespace kram
