// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

//#include <vector>

#include "KramConfig.h"
#include "heman/hedistance.h"

namespace kram {
using namespace NAMESPACE_STL;

class ImageData;

class SDFMipper {
public:
    void init(ImageData& srcImage, bool isVerbose = false);
    void mipmap(ImageData& dstImage, int32_t mipLevel);

private:
    // Note: 120 is slightly less than 2 pixels contributing to a 255 grayscale
    // pixel
    uint8_t threshold = 120;
    float maxD = 0.0;
    bool isVerbose = false;
    heman::my_image srcBitmapImage;
    vector<uint8_t> srcBitmap;
};

}  // namespace kram
