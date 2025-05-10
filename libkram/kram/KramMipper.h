// kram - Copyright 2020-2025 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <cstdint>
//#include <vector>

//#include "KramConfig.h"

namespace kram {
using namespace STL_NAMESPACE;
using namespace SIMD_NAMESPACE;

// return whether num is pow2
bool isPow2(int32_t num);

// find pow2 > num if not already pow2
int32_t nextPow2(int32_t num);

struct Color {
    uint8_t r, g, b, a;
};

// This doesn't convert to srgb before premul, but is cheap.
inline Color toPremul(Color c)
{
    if (c.a != 255) {
        // these are really all fractional, but try this
        c.r = ((uint32_t)c.r * (uint32_t)c.a) / 255;
        c.g = ((uint32_t)c.g * (uint32_t)c.a) / 255;
        c.b = ((uint32_t)c.b * (uint32_t)c.a) / 255;
    }
    return c;
}

inline float4 ColorToUnormFloat4(const Color& value)
{
    // simd lib can't ctor these even in C++, so will make abstracting harder
    float4 c = float4m((float)value.r, (float)value.g, (float)value.b, (float)value.a);
    return c / 255.0f;
}

inline float4 ColorToSnormFloat4(const Color& value)
{
    float4 c = float4m((float)value.r, (float)value.g, (float)value.b, (float)value.a);
    return (c - float4(128.0f)) / 255.0f;
}

inline Color ColorFromUnormFloat4(const float4& value)
{
    float4 c = round(saturate(value) * 255.0f);
    Color color = {(uint8_t)c.x, (uint8_t)c.y, (uint8_t)c.z, (uint8_t)c.w};
    return color;
}

// for signed bc4/5, remap the endpoints after unorm fit
void remapToSignedBCEndpoint88(uint16_t& endpoint);

// for decoding bc4/5 snorm, convert block to unsigned endpoints before decode
void remapFromSignedBCEndpoint88(uint16_t& endpoint);

float4 linearToSRGB(float4 lin);

// return srgb from a linear intesnity
float linearToSRGBFunc(float lin);

class ImageData {
public:
    // data can be mipped as 8u, 16f, or 32f.  Prefer smallest size.
    // half is used when srgb/premultiply is used.  32f is really only for r/rg/rgba32f mips.
    Color* pixels = nullptr;
    half4* pixelsHalf = nullptr; // optional
    float4* pixelsFloat = nullptr; // optional

    int32_t width = 0;
    int32_t height = 0;
    int32_t depth = 0;

    bool isSRGB = false;
    bool isHDR = false; // only updates pixelsFloat
};

class Mipper {
private:
    float srgbToLinear[256];
    float alphaToFloat[256];

public:
    Mipper();

    // start here, then can mipmap or mipFlood
    void initPixelsHalfIfNeeded(ImageData& srcImage, bool doPremultiply, bool doPrezero,
                                vector<half4>& halfImage) const;

    // drop by 1 mip level by box filter
    void mipmap(const ImageData& srcImage, ImageData& dstImage) const;

    // Build all the mips, and then copy from small to big
    // wherever the alpha is 0.  This is a form of cheap
    // dilation, but will result in invalid premul colors r > a.
    void mipflood(vector<ImageData>& srcImage) const;

    // these use table lookups, so need to be class members
    float toLinear(uint8_t srgb) const { return srgbToLinear[srgb]; }
    float toAlphaFloat(uint8_t alpha) const { return alphaToFloat[alpha]; }

    float4 toLinear(const Color& c) const { return float4m(toLinear(c.r), toLinear(c.g), toLinear(c.b), toAlphaFloat(c.a)); }

    uint8_t toPremul(uint8_t channelIntensity, uint8_t alpha) const { return ((uint32_t)channelIntensity * (uint32_t)alpha) / 255; }

private:
    void initTables();

    void mipmapLevel(const ImageData& srcImage, ImageData& dstImage) const;

    void mipmapLevelOdd(const ImageData& srcImage, ImageData& dstImage) const;
};

} // namespace kram
