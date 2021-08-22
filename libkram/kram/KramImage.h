// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

//#include <string>
//#include <vector>

#include "KTXImage.h"  // for MyMTLTextureType
#include "KramConfig.h"
#include "KramImageInfo.h"
#include "KramMipper.h"

namespace kram {

using namespace NAMESPACE_STL;
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

struct MipConstructData;

// TODO: this can only hold one level of mips, so custom mips aren't possible.
// Mipmap generation is all in-place to this storage.
// Multiple chunks are possible in strip or grid form.
class Image {
public:
    Image();

    // these 3 calls for Encode
    bool loadImageFromPixels(const vector<uint8_t>& pixels, int32_t width,
                             int32_t height, bool hasColor, bool hasAlpha);

    // convert top level to single-image
    bool loadImageFromKTX(const KTXImage& image);

    // this is only for 2d images
    bool resizeImage(int32_t wResize, int32_t hResize, bool resizePow2, ImageResizeFilter filter = kImageResizeFilterPoint);

    // this is width and height of the strip/grid, chunks may be copied out of this
    int32_t width() const { return _width; }
    int32_t height() const { return _height; }

    const vector<uint8_t>& pixels() const { return _pixels; }
    const vector<float4>& pixelsFloat() const { return _pixelsFloat; }

    bool hasColor() const { return _hasColor; }
    bool hasAlpha() const { return _hasAlpha; }

    // if converted a KTX/2 image to Image, then this field will be non-zero
    uint32_t chunksY() const { return _chunksY; }
    void setChunksY(uint32_t chunksY) { _chunksY = chunksY; }

private:
    bool convertToFourChannel(const KTXImage& image);

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

    uint32_t _chunksY = 0;
};

class KramDecoderParams {
public:
    TexEncoder decoder = kTexEncoderUnknown;  // will pick best available from format
    bool isVerbose = false;
    string swizzleText;
};

// The decoder can decode an entire KTX/KTX2 into RGBA8u/16F/32F data.
// This is useful on platforms to display formats unsupported by the gpu, but the expanded pixels
// can take up much more memory.
class KramDecoder {
public:
    bool decode(const KTXImage& image, FILE* dstFile, const KramDecoderParams& params) const;

    bool decode(const KTXImage& image, KTXImage& dstImage, const KramDecoderParams& params) const;

    bool decodeBlocks(
        int32_t w, int32_t h,
        const uint8_t* blockData, uint32_t numBlocks, MyMTLPixelFormat blockFormat,
        vector<uint8_t>& dstPixels,  // currently Color
        const KramDecoderParams& params) const;

private:
    bool decodeImpl(const KTXImage& srcImage, FILE* dstFile, KTXImage& dstImage, const KramDecoderParams& params) const;
};

// The encoder takes a single-mip image, and in-place encodes mips and applies other
// requested operations from ImageInfo as it writes those mips.   Note that KTX2 must
// accumulate all mips if compressed so that offsets of where to write data are known.
class KramEncoder {
public:
    // encode/ecode to a file
    bool encode(ImageInfo& info, Image& singleImage, FILE* dstFile) const;

    // encode/decode to a memory block
    bool encode(ImageInfo& info, Image& singleImage, KTXImage& dstImage) const;

    // TODO: supply encode() that takes a KTXImage src with mips already generated
    // and then can encode them to a block format.  In-place mips from Image don't
    // allow for custom mips, and also require conversion of KTXImage to Image.

private:
    bool encodeImpl(ImageInfo& info, Image& singleImage, FILE* dstFile, KTXImage& dstImage) const;

    // compute how big mips will be
    void computeMipStorage(const KTXImage& image, int32_t& w, int32_t& h, int32_t& numSkippedMips,
                           bool doMipmaps, int32_t mipMinSize, int32_t mipMaxSize,
                           vector<KTXImageLevel>& dstMipLevels) const;

    // ugh, reduce the params into this
    bool compressMipLevel(const ImageInfo& info, KTXImage& image,
                          ImageData& mipImage, TextureData& outputTexture,
                          int32_t mipStorageSize) const;

    // can pass in which channels to average
    void averageChannelsInBlock(const char* averageChannels,
                                const KTXImage& image, ImageData& srcImage,
                                vector<Color>& tmpImage) const;

    bool createMipsFromChunks(ImageInfo& info,
                              Image& singleImage,
                              MipConstructData& data,
                              FILE* dstFile, KTXImage& dstImage) const;

    bool writeKTX1FileOrImage(
        ImageInfo& info,
        Image& singleImage,
        MipConstructData& mipConstructData,
        const vector<uint8_t>& propsData,
        FILE* dstFile, KTXImage& dstImage) const;

    void addBaseProps(const ImageInfo& info, KTXImage& dstImage) const;
};

}  // namespace kram
