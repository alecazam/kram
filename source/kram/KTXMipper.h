// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include "KramConfig.h"

#include <cstdint>
#include <vector>

namespace kram {
using namespace std;
using namespace simd;

// return whether num is pow2
bool isPow2(int num);

// find pow2 > num if not already pow2
int nextPow2(int num);

struct Color {
    uint8_t r, g, b, a;
};

inline float4 ColorToUnormFloat4(const Color &value)
{
    // simd lib can't ctor these even in C++, so will make abstracting harder
    float4 c = simd_make_float4((float)value.r, (float)value.g, (float)value.b, (float)value.a);
    return c / 255.0f;
}

inline float4 ColorToSnormFloat4(const Color &value)
{
    float4 c = simd_make_float4((float)value.r, (float)value.g, (float)value.b, (float)value.a);
    return (c - float4(128.0f)) / 255.0f;
}

class ImageData {
public:
    // data can be mipped as 8u, 16f, or 32f.  Prefer smallest size.
    // half is used when srgb/premultiply is used.  32f is really only for r/rg/rgba32f mips.
    Color *pixels = nullptr;
    half4 *pixelsHalf = nullptr;    // optional
    float4 *pixelsFloat = nullptr;  // optional

    int width = 0;
    int height = 0;

    bool isSRGB = false;
    bool isHDR = false;  // only updates pixelsFloat
};

class Mipper {
public:
    float srgbToLinear[256];
    float alphaToFloat[256];

    // uint8_t linearPremulTosrgbPremul[256][256];

    // uint8_t linearToSrgb5[32];
    // uint8_t linearToSrgb6[64];

    Mipper();

    void initTables();

    // drop by 1 mip level by box filter
    void mipmap(ImageData &srcImage, ImageData &dstImage) const;

    // remap endpoints and apply lin -> srgb
    void remapToSrgbEndpoint565(uint16_t &endpoint) const;

    // for signed bc4/5, remap the endpoints after unorm fit
    void remapToSignedEndpoint8(uint16_t &endpoint) const;
    void remapToSignedEndpoint88(uint16_t &endpoint) const;

    void initPixelsHalfIfNeeded(ImageData &srcImage, bool doPremultiply,
                                vector<half4> &halfImage) const;

private:
    void mipmapLevel(ImageData &srcImage, ImageData &dstImage) const;
};

}  // namespace kram
