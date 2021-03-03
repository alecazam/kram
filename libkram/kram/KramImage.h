// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <string>
#include <vector>

#include "KTXImage.h"  // for MyMTLTextureType
#include "KramMipper.h"
#include "KramConfig.h"
#include "KramImageInfo.h"

namespace kram {

using namespace std;
using namespace simd;

class Mipper;
class KTXHeader;
class TextureData;

enum ImageResizeFilter {
    kImageResizeFilterPoint,
    //kImageResizeFilterLinear,
    //kImageResizeFilterLanczos3, Mitchell, Kaiser, etc,
};

//---------------------------

// TODO: this can only holds one level of mips, so custom mips aren't possible.
// Mipmap generation is all in-place to this storage.
class Image {
public:
    Image();

    // these 3 calls for Encode
    bool loadImageFromPixels(const vector<uint8_t>& pixels, int32_t width,
                             int32_t height, bool hasColor, bool hasAlpha);

    bool loadImageFromKTX(const KTXImage& image);

    // encode/ecode to a file
    bool encode(ImageInfo& info, FILE* dstFile) const;

    bool decode(const KTXImage& image, FILE* dstFile, TexEncoder decoder, bool isVerbose, const string& swizzleText) const;
    
    // encode/decode to a memory block
    bool encode(ImageInfo& info, KTXImage& dstImage) const;

    bool decode(const KTXImage& image, KTXImage& dstImage, TexEncoder decoder, bool isVerbose, const string& swizzleText) const;

    // this is only for 2d images
    bool resizeImage(int32_t wResize, int32_t hResize, bool resizePow2, ImageResizeFilter filter = kImageResizeFilterPoint);

    // return state
    int32_t width() const { return _width; }
    int32_t height() const { return _height; }

    const uint8_t* pixels() const { return _pixels.data(); }
    const float4* pixelsFloat() const { return _pixelsFloat.data(); }

    bool hasColor() const { return _hasColor; }
    bool hasAlpha() const { return _hasAlpha; }

private:
    bool encodeImpl(ImageInfo& info, FILE* dstFile, KTXImage& dstImage) const;
    bool decodeImpl(const KTXImage& srcImage, FILE* dstFile, KTXImage& dstImage, TexEncoder decoder, bool isVerbose, const string& swizzleText) const;

    // compute how big mips will be
    void computeMipStorage(const KTXImage& image, int32_t w, int32_t h,
                           bool doMipmaps, int32_t mipMinSize, int32_t mipMaxSize,
                           int32_t& storageSize, int32_t& storageSizeTotal,
                           vector<int32_t>& mipStorageSizes,
                           int32_t& numDstMipLevels, int32_t& numMipLevels) const;

    // ugh, reduce the params into this
    bool compressMipLevel(const ImageInfo& info, KTXImage& image,
                          ImageData& mipImage, TextureData& outputTexture,
                          int32_t mipStorageSize) const;

    // can pass in which channels to average
    void averageChannelsInBlock(const char* averageChannels,
                                const KTXImage& image, ImageData& srcImage,
                                vector<Color>& tmpImage) const;

    // convert x field to normals
    void heightToNormals(float scale);

private:
    // pixel size of image
    int32_t _width = 0;
    int32_t _height = 0;

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
