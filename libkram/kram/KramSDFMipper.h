// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

//#include <vector>

//#include "KramConfig.h"

namespace kram {
using namespace NAMESPACE_STL;

class ImageData;

struct my_image {
    int32_t width;
    int32_t height;
    int32_t numChannels;
    uint8_t* data;
};

class SDFMipper {
public:
    void init(ImageData& srcImage, uint8_t sdfThreshold, bool isVerbose = false);
    void mipmap(ImageData& dstImage, int32_t mipLevel);

private:
    // Note: 120 is slightly less than 2 pixels contributing to a 255 grayscale
    // pixel
    uint8_t threshold = 120;
    float maxD = 0.0;
    bool isVerbose = false;
    my_image srcBitmapImage;
    vector<uint8_t> srcBitmap;
};

}  // namespace kram
