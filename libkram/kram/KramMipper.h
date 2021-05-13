// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <cstdint>
#include <vector>

#include "KramConfig.h"

namespace kram {
using namespace std;
using namespace simd;

// return whether num is pow2
bool isPow2(int32_t num);

// find pow2 > num if not already pow2
int32_t nextPow2(int32_t num);

struct Color {
    uint8_t r, g, b, a;
};

inline float4 ColorToUnormFloat4(const Color &value)
{
    // simd lib can't ctor these even in C++, so will make abstracting harder
    float4 c = float4m((float)value.r, (float)value.g, (float)value.b, (float)value.a);
    return c / 255.0f;
}

inline float4 ColorToSnormFloat4(const Color &value)
{
    float4 c = float4m((float)value.r, (float)value.g, (float)value.b, (float)value.a);
    return (c - float4(128.0f)) / 255.0f;
}

// for signed bc4/5, remap the endpoints after unorm fit
void remapToSignedBCEndpoint88(uint16_t &endpoint);

float4 linearToSRGB(float4 lin);

class ImageData {
public:
    // data can be mipped as 8u, 16f, or 32f.  Prefer smallest size.
    // half is used when srgb/premultiply is used.  32f is really only for r/rg/rgba32f mips.
    Color *pixels = nullptr;
    half4 *pixelsHalf = nullptr;    // optional
    float4 *pixelsFloat = nullptr;  // optional

    int32_t width = 0;
    int32_t height = 0;

    bool isSRGB = false;
    bool isHDR = false;  // only updates pixelsFloat
};

class Mipper {
private:
    float srgbToLinear[256];
    float alphaToFloat[256];

public:
    Mipper();

    // drop by 1 mip level by box filter
    void mipmap(const ImageData &srcImage, ImageData &dstImage) const;

    void initPixelsHalfIfNeeded(ImageData &srcImage, bool doPremultiply, bool doPrezero,
                                vector<half4> &halfImage) const;

    float toLinear(uint8_t srgb) const { return srgbToLinear[srgb]; }
    float toAlphaFloat(uint8_t alpha) const { return alphaToFloat[alpha]; }
    
    uint8_t toPremul(uint8_t channelIntensity, uint8_t alpha) const { return ((uint32_t)channelIntensity * (uint32_t)alpha) / 255; }
    
private:
    void initTables();

    void mipmapLevel(const ImageData &srcImage, ImageData &dstImage) const;
    
    void mipmapLevelOdd(const ImageData& srcImage, ImageData& dstImage) const;
};

}  // namespace kram
