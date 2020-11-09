// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramSDFMipper.h"

#include "KTXMipper.h"

namespace kram {
using namespace heman;

void SDFMipper::init(ImageData& srcImage, bool isVerbose_)
{
    // this resets maxD, which is determined off first mip generated
    // all mips are using same source, so distances should be same range to
    // scale
    maxD = 0.0;
    isVerbose = isVerbose_;

    int w = srcImage.width;
    int h = srcImage.height;

    srcBitmap.resize(w * h);

    // store bitmap in 8-bit grayscale
    const Color* pixels = srcImage.pixels;               // 4 bytes
    uint8_t* dstImageData = (uint8_t*)srcBitmap.data();  // 1 byte

    for (int y = 0; y < h; y++) {
        int y0 = y * w;
        for (int x = 0; x < w; x++) {
            const Color& c0 = pixels[y0 + x];
            uint8_t& cBitmap = dstImageData[y0 + x];

            cBitmap = (c0.r >= threshold) ? 1 : 0;
        }
    }

    // sdf mips typically start with a high res source image bitmap
    // wasting 7 bits to store that bitmap above.  All mips built from this.
    srcBitmapImage.width = w;
    srcBitmapImage.height = h;
    srcBitmapImage.numChannels = 1;
    srcBitmapImage.data = dstImageData;

    // TODO: eventually do multichannel SDF into rgb, this into alpha
    // but that's a lot of data, and not sure how mutlichannel handles super
    // large image sources
}

void SDFMipper::mipmap(ImageData& dstImage, int mipLevel)
{
    int w = srcBitmapImage.width;
    int h = srcBitmapImage.height;

    for (int i = 0; i < mipLevel; ++i) {
        w = (w + 1) / 2;
        h = (h + 1) / 2;
    }

    dstImage.width = w;
    dstImage.height = h;

    Color* pixels = dstImage.pixels;  // 4 bytes

    // stuff back into the rgb channel of the dst texture to feed to encoder
    // have to do in reverse, since we're expanding 1 channel to 4

    my_image dst = {w, h, 1, (uint8_t*)pixels};

    heman_distance_create_sdf(&srcBitmapImage, &dst, maxD, isVerbose);

    const uint8_t* srcImageData = (const uint8_t*)pixels;  // 1 byte

    for (int y = h - 1; y >= 0; y--) {
        int y0 = y * w;
        for (int x = w - 1; x >= 0; x--) {
            Color& c0 = pixels[y0 + x];
            uint8_t cSDF = srcImageData[y0 + x];

            // override any other channels, though we don't have to
            // in r channel, can go to etc-r, bc4, and r8.  with rrr1 can go to
            // astc-L.

            c0.r = cSDF;
            c0.g = c0.b = c0.r;
            c0.a = 255;
        }
    }
}

}  // namespace kram
