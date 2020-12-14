// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include "KramConfig.h"

#include <string>
#include <vector>

#include "KTXImage.h"  // for MyMTLTextureType
#include "KTXMipper.h"
#include "KramImageInfo.h"

namespace kram {

using namespace std;
using namespace simd;

class Mipper;
class KTXHeader;
class TextureData;

enum ImageResizeFilter {
    kImageResizeFilterPoint,
    kImageResizeFilterLinear,
    kImageResizeFilterLanczos3
};

//---------------------------

// TODO: this can only holds one level of mips, so custom mips aren't possible.
// Mipmap generation is all in-place to this storage.
class Image {
public:
    Image();

    // these 3 calls for Encode
    bool loadImageFromPixels(const vector<uint8_t>& pixels, int width,
                             int height, bool hasColor, bool hasAlpha);

    bool loadImageFromKTX(const KTXImage& image);

    bool encode(ImageInfo& info, FILE* dstFile) const;

    bool decode(const KTXImage& image, FILE* dstFile, TexEncoder decoder, bool isVerbose, const string& swizzleText) const;

    // this is only for 2d images
    bool resizeImage(int wResize, int hResize, bool resizePow2, ImageResizeFilter filter = kImageResizeFilterPoint);

    // return state
    int width() const { return _width; }
    int height() const { return _height; }

    const uint8_t* pixels() const { return _pixels.data(); }
    const float4* pixelsFloat() const { return _pixelsFloat.data(); }

    bool hasColor() const { return _hasColor; }
    bool hasAlpha() const { return _hasAlpha; }

private:
    // compute how big mips will be
    void computeMipStorage(const KTXImage& image, int w, int h,
                           bool doMipmaps, int mipMinSize, int mipMaxSize,
                           int& storageSize, int& storageSizeTotal,
                           vector<int>& mipStorageSizes,
                           int& numDstMipLevels, int& numMipLevels) const;

    // ugh, reduce the params into this
    bool compressMipLevel(const ImageInfo& info, KTXImage& image,
                          Mipper& mipper,
                          ImageData& mipImage, TextureData& outputTexture,
                          int mipStorageSize) const;

    // can pass in which channels to average
    void averageChannelsInBlock(const char* averageChannels,
                                const KTXImage& image, ImageData& srcImage,
                                vector<Color>& tmpImage) const;

    // convert x field to normals
    void heightToNormals(float scale);
    
private:
    // pixel size of image
    int _width = 0;
    int _height = 0;

    // this is whether png/ktx source image  format was L or LA or A or RGB
    // if unknown then set to true, and the pixel walk will set to false
    bool _hasColor = true;
    bool _hasAlpha = true;

    // this is the entire strip data, float version can be passed for HDR
    // sources always 4 channels RGBA for 8 and 32f data.  16f promoted to 32f.
    vector<uint8_t> _pixels;  // TODO: change to Color?
    //vector<half4> _pixelsHalf; // TODO: add support to import fp16
    vector<float4> _pixelsFloat;
};

}  // namespace kram
