// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramImage.h"

#if COMPILE_ATE
#include "ate/ateencoder.h"  // astc/bc encoder, apple only
#endif

#if COMPILE_ETCENC
#include "EtcImage.h"  // etc encoder
#endif

#if COMPILE_SQUISH
#include "squish/squish.h"  // bc encoder
#endif

#if COMPILE_BCENC
// TODO: move to CMake file?
#define RGBCX_IMPLEMENTATION 1
#define RGBCX_USE_SMALLER_TABLES 1

#include "bc7enc/bc7decomp.h"
#include "bc7enc/bc7enc.h"  // bc encoder
#include "bc7enc/rgbcx.h"
#endif

#if COMPILE_ASTCENC
#include "astc-encoder/astcenc.h"  // astc encoder

// hack to improve block generation on L1 and LA encoding
//extern thread_local int32_t gAstcenc_UniqueChannelsInPartitioning;
#endif

#include <cassert>
#include <cstdio>
#include <string>
#include <algorithm>

#include "KTXImage.h"
#include "KramMipper.h"
#include "KramFileHelper.h"
#include "KramSDFMipper.h"
#include "KramTimer.h"

#if KRAM_MAC || KRAM_LINUX
#include <sys/errno.h>
#endif

namespace kram {

using namespace std;
using namespace simd;
using namespace heman;

template <typename T>
void pointFilterImage(int32_t w, int32_t h, const T* srcImage,
                      int32_t dstW, int32_t dstH, T* dstImage)
{
    float scaleX = (float)w / dstW;
    float scaleY = (float)h / dstH;

    for (int32_t y = 0; y < dstH; ++y) {
        int32_t ySrc = (int32_t)floorf((float)y * scaleY);

        for (int32_t x = 0; x < dstW; ++x) {
            int32_t xSrc = (int32_t)floorf((float)x * scaleX);

            dstImage[y * dstW + x] = srcImage[ySrc * w + xSrc];
        }
    }
}

/// Rrepresents output data
class TextureData {
public:
    int32_t width;
    int32_t height;
    //int32_t format;
    vector<uint8_t> data;
};

Image::Image() : _width(0), _height(0), _hasColor(false), _hasAlpha(false)
{
}

// TODO: use KTXImage everywhere so can have explicit mip chains
// this routine converts KTX to float4, but don't need if already matching 4 channels
// could do other formata conversions here on more supported formats (101010A2, etc).

// TODO: handle loading KTXImage with custom mips
// TODO: handle loading KTXImage with other texture types (cube, array, etc)

// TODO: image here is very specifically a single level of chunks of float4 or Color (RGBA8Unorm)
// the encoder is only written to deal with those types.

// TODO: for png need to turn grid/horizontal strip into a vertical strip if not already
// that way can move through the chunks and overwrite them in-place.
// That would avoid copying each chunk out in the encode, but have to do in reodering.
// That way data is stored as KTX would instead of how PNG does.

bool Image::loadImageFromKTX(const KTXImage& image)
{
    // copy the data into a contiguous array
    _width = image.width;
    _height = image.height;

    // TODO: handle more texture types with custom mips
    if (image.textureType != MyMTLTextureType2D) {
        KLOGE("Image", "Only support 2D texture type import for KTX");
        return false;
    }

    // TODO: handle custom mips, this will currently box filter to build
    // remaining mips but for SDF or coverage scaled alpha test, need to
    // preserve original data.
    if (image.header.numberOfMipmapLevels > 1) {
        KLOGW("Image", "Skipping custom mip levels");
    }

    // so can call through to blockSize
    KTXHeader header;
    header.initFormatGL(image.pixelFormat);
    int32_t blockSize = image.blockSize();

    _hasColor = isColorFormat(image.pixelFormat);
    _hasAlpha = isAlphaFormat(image.pixelFormat);

    switch (image.pixelFormat) {
        case MyMTLPixelFormatR8Unorm:
        case MyMTLPixelFormatRG8Unorm:
#if SUPPORT_RGB
        case MyMTLPixelFormatRGB8Unorm_sRGB_internal:
        case MyMTLPixelFormatRGB8Unorm_internal:
#endif
        case MyMTLPixelFormatRGBA8Unorm_sRGB:
        case MyMTLPixelFormatRGBA8Unorm:
        {
            const uint8_t* srcPixels =
                image.fileData + image.mipLevels[0].offset;

            int32_t numSrcChannels = blockSize / sizeof(uint8_t);
            int32_t numDstChannels = 4;

            // Note: clearing unspecified channels to 0000, not 0001
            // can set swizzleText when encoding
            _pixels.resize(4 * _width * _height);
            if (numSrcChannels != 4) {
                memset(_pixels.data(), 0, _pixels.size());
            }

            for (int32_t y = 0; y < _height; ++y) {
                int32_t y0 = _height * y;

                for (int32_t x = 0, xEnd = _width; x < xEnd; ++x) {
                    int32_t srcX = (y0 + x) * numSrcChannels;
                    int32_t dstX = (y0 + x) * numDstChannels;

                    switch (numSrcChannels) {
                        // all fallthrough
                        case 4:
                            _pixels[dstX + 3] = srcPixels[srcX + 3];
                        case 3:
                            _pixels[dstX + 2] = srcPixels[srcX + 2];
                        case 2:
                            _pixels[dstX + 1] = srcPixels[srcX + 1];
                        case 1:
                            _pixels[dstX + 0] = srcPixels[srcX + 0];
                    }
                }
            }

            // caller can use swizzle after loading data here, and even compress
            // content
            break;
        }

        case MyMTLPixelFormatR16Float:
        case MyMTLPixelFormatRG16Float:
#if SUPPORT_RGB
        case MyMTLPixelFormatRGB16Float_internal:
#endif
        case MyMTLPixelFormatRGBA16Float: {
            int32_t numSrcChannels = blockSize / 2;  // 2 = sizeof(_float16)
            int32_t numDstChannels = 4;

            // Note: clearing unspecified channels to 0000, not 0001
            // can set swizzleText when encoding
            _pixelsFloat.resize(_width * _height);
            if (numSrcChannels != 4) {
                memset(_pixelsFloat.data(), 0,
                       _pixelsFloat.size() * sizeof(float4));
            }

            // treat as float for per channel copies
            float4* dstPixels = _pixelsFloat.data();

            const half* srcPixels =
                (const half*)(image.fileData + image.mipLevels[0].offset);

            half4 srcPixel;
            for (int32_t i = 0; i < 4; ++i) {
                srcPixel.v[i] = 0;
            }

            for (int32_t y = 0; y < _height; ++y) {
                int32_t y0 = _height * y;

                for (int32_t x = 0, xEnd = _width; x < xEnd; ++x) {
                    int32_t srcX = (y0 + x) * numSrcChannels;
                    int32_t dstX = (y0 + x) * numDstChannels;

                    // copy in available values
                    for (int32_t i = 0; i < numSrcChannels; ++i) {
                        srcPixel.v[i] = srcPixels[srcX + i];
                    }

                    // use AVX to convert
                    dstPixels[dstX] = toFloat4(srcPixel);
                }
            }

            // caller can swizzle
            // caller can compress to BC6H or ASTC-HDR if encoders available
            // some textures could even go to LDR, but would need to tonemap or
            // clamp the values

            break;
        }

        case MyMTLPixelFormatR32Float:
        case MyMTLPixelFormatRG32Float:
#if SUPPORT_RGB
        case MyMTLPixelFormatRGB32Float_internal:
#endif
       case MyMTLPixelFormatRGBA32Float: {
            const float* srcPixels =
                (const float*)(image.fileData + image.mipLevels[0].offset);

            int32_t numSrcChannels = blockSize / sizeof(float);
            int32_t numDstChannels = 4;

            // Note: clearing unspecified channels to 0000, not 0001
            // can set swizzleText when encoding
            _pixelsFloat.resize(_width * _height);
            if (numSrcChannels != 4) {
                memset(_pixelsFloat.data(), 0,
                       _pixelsFloat.size() * sizeof(float4));
            }

            // treat as float for per channel copies
            float* dstPixels = (float*)(_pixelsFloat.data());

            for (int32_t y = 0; y < _height; ++y) {
                int32_t y0 = _height * y;

                for (int32_t x = 0, xEnd = _width; x < xEnd; ++x) {
                    int32_t srcX = (y0 + x) * numSrcChannels;
                    int32_t dstX = (y0 + x) * numDstChannels;

                    switch (numSrcChannels) {
                        // all fallthrough
                        case 4:
                            dstPixels[dstX + 3] = srcPixels[srcX + 3];
                        case 3:
                            dstPixels[dstX + 2] = srcPixels[srcX + 2];
                        case 2:
                            dstPixels[dstX + 1] = srcPixels[srcX + 1];
                        case 1:
                            dstPixels[dstX + 0] = srcPixels[srcX + 0];
                    }
                }
            }

            // caller can swizzle
            // caller can compress to BC6H or ASTC-HDR if encoders available
            // some textures could even go to LDR, but would need to tonemap or
            // clamp the values

            break;
        }
        default:
            KLOGE("Image", "Unsupported KTX format\n");
            return false;
    }

    return true;
}

bool Image::loadImageFromPixels(const vector<uint8_t>& pixels, int32_t width,
                                int32_t height, bool hasColor, bool hasAlpha)
{
    // copy the data into a contiguous array
    _width = width;
    _height = height;

    // if true, then don't actually know this unless walk the pixels.
    // Format can also affect this, since 1/2 channel don't have color or alpha.
    _hasColor = hasColor;  // grayscale or no rgb when false
    _hasAlpha = hasAlpha;

    // always assumes 4 rgba8 channels
    // _pixels.resize(4 * _width * _height);
    assert((int32_t)pixels.size() == (width * height * 4));
    _pixels = pixels;

    return true;
}

void Image::computeMipStorage(const KTXImage& image, int32_t w, int32_t h,
                              bool doMipmaps, int32_t mipMinSize, int32_t mipMaxSize,
                              int32_t& storageSize, int32_t& storageSizeTotal,
                              vector<int32_t>& mipStorageSizes,
                              int32_t& numDstMipLevels, int32_t& numMipLevels) const
{
    bool canMipmap = true;  // isPow2(w) && isPow2(h); // DONE: removed pow2 requirement, mip gen handles non-pow2

    bool needsDownsample = (w > mipMaxSize || h > mipMaxSize);

    int32_t maxMipLevels = 16;  // 64K x 64K
    if ((!doMipmaps) && needsDownsample) {
        maxMipLevels = 1;
    }

    if (canMipmap && (doMipmaps || needsDownsample)) {
        numMipLevels++;

        bool keepMip =
            (w >= mipMinSize && w <= mipMaxSize) &&
            (h >= mipMinSize && h <= mipMaxSize);

        if (keepMip) {
            mipStorageSizes.push_back(storageSize);
            numDstMipLevels++;
        }
        else {
            mipStorageSizes.push_back(0);  // 0 means skip storing this mip
        }

        do {
            mipDown(w, h);

            keepMip =
                (w >= mipMinSize && w <= mipMaxSize) &&
                (h >= mipMinSize && h <= mipMaxSize);

            if (keepMip && (numDstMipLevels < maxMipLevels)) {
                int32_t mipStorageSize = image.mipLevelSize(w, h);
                mipStorageSizes.push_back(mipStorageSize);
                storageSizeTotal += mipStorageSize;
                numDstMipLevels++;
            }
            else {
                mipStorageSizes.push_back(0);  // - means skip storing this mip
            }

            // a count of how many mips exist from topmost
            numMipLevels++;
        } while (w > 1 || h > 1);

        // adjust the pixel storage area to the first/largest exported mip
        for (auto mipStorageSize : mipStorageSizes) {
            if (mipStorageSize != 0) {
                storageSize = mipStorageSize;
                break;
            }
        }
    }
    else {
        mipStorageSizes.push_back(storageSize);
        numDstMipLevels++;
        numMipLevels++;
    }
}

// Can average any channels per block, this means they are constant across the
// block and use endpoint storage but do not affect the endpoint fitting.
// Results in a low-res, blocky version of those channels, but better
// reconstruction for channels which are not averaged.  BC3nm + rb average.
// BC1nm + b average.  That way color endpoints are of some use rather than just
// being set ot 0.  This runs counter to ASTC L+A mode though which eliminates
// the endpoint storage.
void Image::averageChannelsInBlock(
    const char* averageChannels, const KTXImage& image, ImageData& srcImage,
    vector<Color>& tmpImageData8) const  // otherwise, it's BlueAlpha averaging
{
    int32_t w = srcImage.width;
    int32_t h = srcImage.height;

    // DONE: switch to mask, then can average b on astc4x4, code averages all
    // channels, then apply mask on writes back over the pixel.
    bool isRed = averageChannels[0] == 'r';
    bool isGreen = averageChannels[1] == 'g';
    bool isBlue = averageChannels[2] == 'b';
    bool isAlpha = averageChannels[3] == 'a';

    // can use this to squeeze 2 more block constant channels into the texture
    // these then don't affect the fitting, but do affect endpoint storage (f.e.
    // RGB instead of L+A) must be done before the encode due to complexity of
    // BC6/7 and ASTC

    // copy the level into a temp buffer and average all values for the block
    // dims
    Int2 blockDims = image.blockDims();
    tmpImageData8.resize(w * h);

    for (int32_t yy = 0; yy < h; yy += blockDims.y) {
        for (int32_t xx = 0; xx < w; xx += blockDims.x) {
            // compute clamped blockDims
            Int2 clampedBlockDims;
            clampedBlockDims.x = std::min(w - xx, blockDims.x);
            clampedBlockDims.y = std::min(h - yy, blockDims.y);

            // average red, blue
            int32_t red32 = 0;
            int32_t green32 = 0;
            int32_t blue32 = 0;
            int32_t alpha32 = 0;

            for (int32_t y = 0; y < clampedBlockDims.y; ++y) {
                int32_t y0 = yy + y;
                for (int32_t x = xx, xEnd = xx + clampedBlockDims.x; x < xEnd;
                     ++x) {
                    red32 += srcImage.pixels[y0 + x].r;
                    green32 += srcImage.pixels[y0 + x].g;
                    blue32 += srcImage.pixels[y0 + x].b;
                    alpha32 += srcImage.pixels[y0 + x].a;
                }
            }

            int32_t numBlockPixelsClamped = clampedBlockDims.x * clampedBlockDims.y;
            uint8_t red =
                (red32 + numBlockPixelsClamped / 2) / numBlockPixelsClamped;
            uint8_t green =
                (green32 + numBlockPixelsClamped / 2) / numBlockPixelsClamped;
            uint8_t blue =
                (blue32 + numBlockPixelsClamped / 2) / numBlockPixelsClamped;
            uint8_t alpha =
                (alpha32 + numBlockPixelsClamped / 2) / numBlockPixelsClamped;

            // copy over the red/blue chennsl
            for (int32_t y = 0; y < clampedBlockDims.y; ++y) {
                int32_t y0 = yy + y;
                for (int32_t x = xx, xEnd = xx + clampedBlockDims.x; x < xEnd;
                     ++x) {
                    // replace red/blue with average of block
                    Color c = srcImage.pixels[y0 + x];
                    Color& cDst = tmpImageData8[y0 + x];
                    if (isRed) c.r = red;
                    if (isGreen) c.g = green;
                    if (isBlue) c.b = blue;
                    if (isAlpha) c.a = alpha;
                    cDst = c;
                }
            }
        }
    }
}

// this can return on failure to write
static bool writeDataAtOffset(const uint8_t* data, size_t dataSize, size_t dataOffset, FILE* dstFile, KTXImage& dstImage)
{
    if (dstFile) {
        fseek(dstFile, dataOffset, SEEK_SET);
        if (!FileHelper::writeBytes(dstFile, data, dataSize))
            return false;
    }
    else {
        memcpy(dstImage.imageData().data() + dataOffset, data, dataSize);
    }
    return true;
}

bool Image::decode(const KTXImage& srcImage, FILE* dstFile, TexEncoder decoder, bool isVerbose, const string& swizzleText) const
{
    KTXImage dstImage; // thrown out, data written to file
    return decodeImpl(srcImage, dstFile, dstImage, decoder, isVerbose, swizzleText);
}

bool Image::decode(const KTXImage& srcImage, KTXImage& dstImage, TexEncoder decoder, bool isVerbose, const string& swizzleText) const
{
    return decodeImpl(srcImage, nullptr, dstImage, decoder, isVerbose, swizzleText);
}

bool Image::decodeImpl(const KTXImage& srcImage, FILE* dstFile, KTXImage& dstImage, TexEncoder decoder, bool isVerbose, const string& swizzleText) const
{
    // read existing KTX file into mip offset, then start decoding the blocks
    // and write these to 8u,16f,32f ktx with mips
    // write out KTXHeader for the explicit image, this should be similar to other code

    // Image sorta represents uncompressed Image mips, not compressed.
    // But wriing things out to dstFile.
    int32_t numChunks = srcImage.totalChunks();
   
    MyMTLPixelFormat pixelFormat = srcImage.pixelFormat;
    bool isSrgb = isSrgbFormat(pixelFormat);
    bool isHDR = isHdrFormat(pixelFormat);

    // setup dstImage
    //KTXImage dstImage;
    dstImage = srcImage;  // copy src (name-value pairs copied too)
    
    // important otherwise offsets are wrong if src is ktx2
    if (srcImage.skipImageLength) {
        dstImage.skipImageLength = false;
    }
    dstImage.fileData = nullptr;
    dstImage.fileDataLength = 0;

    KTXHeader& dstHeader = dstImage.header;
    
    // changing format, so update props
    auto dstPixelFormat = isSrgb ? MyMTLPixelFormatRGBA8Unorm_sRGB : MyMTLPixelFormatRGBA8Unorm;
    
    // DONE: Support ASTC and BC7 HDR decode to RGBA16F here
    if (isHdrFormat(srcImage.pixelFormat)) {
        dstPixelFormat = MyMTLPixelFormatRGBA16Float;
    }
    
    dstHeader.initFormatGL(dstPixelFormat);
    dstImage.pixelFormat = dstPixelFormat;
    dstImage.addFormatProps();  // update format prop

    vector<uint8_t> propsData;
    dstImage.toPropsData(propsData);
    dstHeader.bytesOfKeyValueData = uint32_t(propsData.size());
    if (!dstImage.initMipLevels(false, sizeof(KTXHeader) + dstHeader.bytesOfKeyValueData)) {
        return false;
    }
    
    // allocate to hold props and entire image to write out
    if (!dstFile) {
        dstImage.reserveImageData();
    }
    
    bool success = false;

    // 1d textures need to write out 0 width
    KTXHeader headerCopy = dstHeader;
    
    if (dstImage.textureType == MyMTLTextureType1DArray) {
        headerCopy.pixelHeight = 0;
        headerCopy.pixelDepth = 0;
    }
    
   
    // write the header out
    if (!writeDataAtOffset((const uint8_t*)&headerCopy, sizeof(headerCopy), 0, dstFile, dstImage)) {
        return false;
    }
    
    // write out the props
    if (!writeDataAtOffset(propsData.data(), propsData.size(), sizeof(KTXHeader), dstFile, dstImage)) {
        return false;
    }

    // TODO: more work on handling snorm -> unorm conversions ?

    vector<uint8_t> outputTexture;
    vector<uint8_t> srcTexture;

    // could tie use flags to format filter, or encoder settings
    // or may want to disable if decoders don't gen correct output
#if COMPILE_ATE
    // Encode/decode formats differ depending on library version
    // but it's likely the fastest decoder.  Only on macOS/iOS.
    bool useATE = decoder == kTexEncoderATE;
#endif
#if COMPILE_SQUISH
    bool useSquish = decoder == kTexEncoderSquish;
#endif
#if COMPILE_BCENC
    bool useBcenc = decoder == kTexEncoderBcenc;
#endif
#if COMPILE_ASTCENC
    bool useAstcenc = decoder == kTexEncoderAstcenc;
#endif

    // DONE: walk chunks here and seek to src and dst offsets in conversion
    // make sure to walk chunks in the exact same order they are written, array then face, or slice
    
    int32_t w = srcImage.width;
    int32_t h = srcImage.height;

    for (int32_t chunk = 0; chunk < numChunks; ++chunk) {
        w = srcImage.width;
        h = srcImage.height;
        
        for (int32_t i = 0; i < (int32_t)srcImage.header.numberOfMipmapLevels; ++i) {
            const KTXImageLevel& dstMipLevel = dstImage.mipLevels[i];
            outputTexture.resize(dstMipLevel.length);

            const KTXImageLevel& srcMipLevel = srcImage.mipLevels[i];
            const uint8_t* srcData = srcImage.fileData + srcMipLevel.offset + chunk * srcMipLevel.length;
            
            // copy srcData if using ATE, it says it needs 16-byte aligned data for encode
            // and assume for decode too.  Output texture is already 16-byte aligned.
            if (((uintptr_t)srcData & 15) != 0) {
                srcTexture.resize(srcMipLevel.length);
                memcpy(srcTexture.data(), srcData, srcMipLevel.length);
                srcData = srcTexture.data();
            }

            // start decoding after format pulled from KTX file
            if (isBCFormat(pixelFormat)) {
                // bc via ate, or squish for bc1-5 if on other platforms
                // bcenc also likely has decode for bc7
                if (false) {
                    // just to chain if/else
                }
    #if COMPILE_BCENC
                else if (useBcenc) {
                    Color* dstPixels = (Color*)outputTexture.data();

                    const int32_t blockDim = 4;
                    int32_t blocks_x = (w + blockDim - 1) / blockDim;
                    //int32_t blocks_y = (h + blockDim - 1) / blockDim;
                    int32_t blockSize = blockSizeOfFormat(pixelFormat);

                    for (int32_t y = 0; y < h; y += blockDim) {
                        for (int32_t x = 0; x < w; x += blockDim) {
                            int32_t bbx = x / blockDim;
                            int32_t bby = y / blockDim;
                            int32_t bb0 = bby * blocks_x + bbx;
                            const uint8_t* srcBlock = &srcData[bb0 * blockSize];

                            // decode into temp 4x4 pixels
                            Color pixels[blockDim * blockDim];

                            success = true;

                            switch (pixelFormat) {
                                case MyMTLPixelFormatBC1_RGBA:
                                case MyMTLPixelFormatBC1_RGBA_sRGB:
                                    // Returns true if the block uses 3 color punchthrough alpha mode.
                                    rgbcx::unpack_bc1(srcBlock, pixels);
                                    break;
                                case MyMTLPixelFormatBC3_RGBA_sRGB:
                                case MyMTLPixelFormatBC3_RGBA:
                                    // Returns true if the block uses 3 color punchthrough alpha mode.
                                    rgbcx::unpack_bc3(srcBlock, pixels);
                                    break;
                                case MyMTLPixelFormatBC4_RSnorm:
                                case MyMTLPixelFormatBC4_RUnorm:
                                    rgbcx::unpack_bc4(srcBlock, (uint8_t*)pixels);
                                    break;
                                case MyMTLPixelFormatBC5_RGSnorm:
                                case MyMTLPixelFormatBC5_RGUnorm:
                                    rgbcx::unpack_bc5(srcBlock, pixels);
                                    break;

                                case MyMTLPixelFormatBC7_RGBAUnorm:
                                case MyMTLPixelFormatBC7_RGBAUnorm_sRGB:
                                    bc7decomp::unpack_bc7(srcBlock, (bc7decomp::color_rgba*)pixels);
                                    break;

                                default:
                                    KLOGE("Image", "decode unsupported format");
                                    success = false;
                                    break;
                            }

                            if (!success) {
                                return false;
                            }

                            // copy temp pixels to outputTexture
                            for (int32_t by = 0; by < blockDim; ++by) {
                                int32_t yy = y + by;
                                if (yy >= h) {
                                    break;
                                }

                                for (int32_t bx = 0; bx < blockDim; ++bx) {
                                    int32_t xx = x + bx;
                                    if (xx >= w) {
                                        break;  // go to next y above
                                    }

                                    dstPixels[yy * w + xx] = pixels[by * blockDim + bx];
                                }
                            }
                        }
                    }
                }
    #endif
    #if COMPILE_SQUISH
                else if (useSquish) {
                    squish::TexFormat format = squish::kBC1;

                    success = true;

                    switch (pixelFormat) {
                        case MyMTLPixelFormatBC1_RGBA:
                        case MyMTLPixelFormatBC1_RGBA_sRGB:
                            format = squish::kBC1;
                            break;
                        case MyMTLPixelFormatBC3_RGBA_sRGB:
                        case MyMTLPixelFormatBC3_RGBA:
                            format = squish::kBC3;
                            break;
                        case MyMTLPixelFormatBC4_RSnorm:
                        case MyMTLPixelFormatBC4_RUnorm:
                            format = squish::kBC4;
                            break;
                        case MyMTLPixelFormatBC5_RGSnorm:
                        case MyMTLPixelFormatBC5_RGUnorm:
                            format = squish::kBC5;
                            break;
                        default:
                            KLOGE("Image", "decode unsupported format");
                            success = false;
                            break;
                    }

                    if (success) {
                        // only handles bc1,3,4,5
                        squish::DecompressImage(outputTexture.data(), w, h, srcData, format);
                        success = true;
                    }
                }
    #endif
    #if COMPILE_ATE
                else if (useATE) {
                    ATEEncoder encoder;
                    success = encoder.Decode(pixelFormat, (int32_t)srcMipLevel.length, srcImage.blockDims().y,
                                             isVerbose,
                                             w, h, srcData, outputTexture.data());
                }
    #endif
            }
            else if (isETCFormat(pixelFormat)) {
                // etc via etc2comp
    #if COMPILE_ETCENC
                Etc::Image::Format format = Etc::Image::Format::R11;

                success = true;

                switch (pixelFormat) {
                    case MyMTLPixelFormatEAC_R11Unorm:
                        format = Etc::Image::Format::R11;
                        break;
                    case MyMTLPixelFormatEAC_R11Snorm:
                        format = Etc::Image::Format::SIGNED_R11;
                        break;
                    case MyMTLPixelFormatEAC_RG11Unorm:
                        format = Etc::Image::Format::RG11;
                        break;
                    case MyMTLPixelFormatEAC_RG11Snorm:
                        format = Etc::Image::Format::SIGNED_RG11;
                        break;

                    case MyMTLPixelFormatETC2_RGB8:
                        format = Etc::Image::Format::RGB8;
                        break;
                    case MyMTLPixelFormatETC2_RGB8_sRGB:
                        format = Etc::Image::Format::SRGB8;
                        break;
                    case MyMTLPixelFormatEAC_RGBA8:
                        format = Etc::Image::Format::RGBA8;
                        break;
                    case MyMTLPixelFormatEAC_RGBA8_sRGB:
                        format = Etc::Image::Format::SRGBA8;
                        break;

                    default:
                        KLOGE("Image", "decode unsupported format");
                        success = false;
                        break;
                }

                if (success) {
                    Etc::Image etcImage(format, nullptr,
                                        w, h, Etc::ErrorMetric::NUMERIC);

                    success = etcImage.Decode(srcData, outputTexture.data()) == Etc::Image::SUCCESS;
                }
    #endif
            }
            else if (isASTCFormat(pixelFormat)) {
                // ate can decode more than it encodes
                if (false) {
                    // just to chain if/else
                }
    #if COMPILE_ASTCENC
                else if (useAstcenc) {
                    // decode the mip
                    astcenc_image dstImageASTC;
                    dstImageASTC.dim_x = w;
                    dstImageASTC.dim_y = h;
                    dstImageASTC.dim_z = 1;  // Not using 3D blocks, not supported on iOS
                    //dstImageASTC.dim_pad = 0;
                    dstImageASTC.data_type = ASTCENC_TYPE_U8;
                    
                    
                    // encode/encode still setup on array of 2d slices, so need address of data
                    uint8_t* outData = outputTexture.data();
                    dstImageASTC.data = (void**)&outData;

                    int32_t srcDataLength = (int32_t)srcMipLevel.length;
                    Int2 blockDims = srcImage.blockDims();

                    astcenc_profile profile;
                    profile = ASTCENC_PRF_LDR;  // isSrgb ? ASTCENC_PRF_LDR_SRGB : ASTCENC_PRF_LDR;
                    if (isHDR) {
                        profile = ASTCENC_PRF_HDR;  // TODO: also ASTCENC_PRF_HDR_RGB_LDR_A
                    }

                    astcenc_config config;
                    astcenc_error error = astcenc_config_init(
                        profile, blockDims.x, blockDims.y, 1, ASTCENC_PRE_FAST, ASTCENC_FLG_DECOMPRESS_ONLY, &config);
                    if (error != ASTCENC_SUCCESS) {
                        return false;
                    }

                    astcenc_context* codec_context = nullptr;
                    error = astcenc_context_alloc(&config, 1, &codec_context);
                    if (error != ASTCENC_SUCCESS) {
                        return false;
                    }
                    // no swizzle
                    astcenc_swizzle swizzleDecode = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};

                    error = astcenc_decompress_image(codec_context, srcData, srcDataLength, &dstImageASTC, swizzleDecode, 0);

                    astcenc_context_free(codec_context);

                    success = (error == ASTCENC_SUCCESS);
                }
    #endif
    #if COMPILE_ATE
                else if (useATE) {
                    // this decods all except hdr/bc6
                    ATEEncoder encoder;
                    success = encoder.Decode(pixelFormat, (int32_t)srcMipLevel.length, srcImage.blockDims().y,
                                             isVerbose,
                                             w, h, srcData, outputTexture.data());
                }
    #endif
            }
            else {
                KLOGE("Image", "unsupported pixel format for decode");
                success = false;
            }

            // stop processing mips, since failed above
            if (!success) {
                break;
            }

            // swizzle the data back to a more viewable layout (f.e. gggr -> rg01)
            // This swizzleText is currently explicit, but could be reversed from prop of content channels and preswizzle.
            // It's hard to specify this swizzle for arbitrary content otherwise.
            if (!swizzleText.empty()) {
                ImageInfo::swizzleTextureLDR(w, h, (Color*)outputTexture.data(), swizzleText.c_str());
            }

            // write the mips out to the file, and code above can then decode into the same buffer
            // This isn't correct for cubes, arrays, and other types.  The mip length is only written out once for all mips.
            int32_t dstMipOffset = dstMipLevel.offset + chunk * dstMipLevel.length;
            
            if (chunk == 0 && !dstImage.skipImageLength) {
                uint32_t levelSize = dstMipLevel.length;
                
                // cubes write the face size, not the levels size, ugh
                if (srcImage.textureType != MyMTLTextureTypeCube) {
                    levelSize *= numChunks;
                }
                
                if (!writeDataAtOffset((const uint8_t*)&levelSize, sizeof(levelSize), dstMipOffset - sizeof(levelSize), dstFile, dstImage)) {
                    return false;
                }
            }
            
            if (!writeDataAtOffset(outputTexture.data(), dstMipLevel.length, dstMipOffset, dstFile, dstImage)) {
                return false;
            }
            
            // next mip level
            mipDown(w, h);
        }
    }
    
    return success;
}

bool Image::resizeImage(int32_t wResize, int32_t hResize, bool resizePow2, ImageResizeFilter /* filter */)
{
    if (resizePow2) {
        if (isPow2(_width) && isPow2(_height)) {
            return true;
        }

        // ignore incoming
        wResize = nextPow2(_width);
        hResize = nextPow2(_height);
    }

    if (_width == wResize && _height == hResize) {
        return true;
    }
    if (_pixels.empty()) {
        return false;
    }

    if (!_pixels.empty()) {
        vector<uint8_t> pixelsResize;
        pixelsResize.resize(wResize * hResize * sizeof(Color));

        pointFilterImage(_width, _height, (const Color*)_pixels.data(), wResize, hResize, (Color*)pixelsResize.data());

        _pixels = pixelsResize;
    }
    else if (!_pixelsFloat.empty()) {
        vector<float4> pixelsResize;
        pixelsResize.resize(wResize * hResize);

        pointFilterImage(_width, _height, _pixelsFloat.data(), wResize, hResize, pixelsResize.data());

        _pixelsFloat = pixelsResize;
    }

    _width = wResize;
    _height = hResize;

    return true;
}


bool Image::encode(ImageInfo& info, KTXImage& dstImage) const
{
    return encodeImpl(info, nullptr, dstImage);
}

bool Image::encode(ImageInfo& info, FILE* dstFile) const
{
    // this will be throw out
    KTXImage dstImage;
    return encodeImpl(info, dstFile, dstImage);
}


bool Image::encodeImpl(ImageInfo& info, FILE* dstFile, KTXImage& dstImage) const
{
    //KTXImage image;
    KTXHeader& header = dstImage.header;

    vector<Int2> chunkOffsets;

    int32_t w = _width;
    int32_t h = _height;

    if (!validateTextureType(info.textureType, w, h, chunkOffsets, header,
                             info.doMipmaps,
                             info.chunksX, info.chunksY, info.chunksCount))
    {
        return false;
    }

    // cube and array this is the size of one face/slice
    const int32_t modifiedWidth = w;
    const int32_t modifiedHeight = h;

    // work out how much memory we need to load
    header.initFormatGL(info.pixelFormat);

    dstImage.pixelFormat = info.pixelFormat;
    dstImage.textureType = info.textureType;

    dstImage.addFormatProps();

    // TODO: caller should really set post swizzle
    string postSwizzleText;
    if (info.swizzleText == "gggr")
        postSwizzleText = "ag01";
    else if (info.swizzleText == "rrrg")
        postSwizzleText = "ga01";  // or ra01
    else if (info.swizzleText == "rrr1")
        postSwizzleText = "r001";  // to match up with BC4/EAC_R11

    dstImage.addSwizzleProps(info.swizzleText.c_str(), postSwizzleText.c_str());

    // TODO: caller should really set this, channels and address/filter
    // three letter codes for the channel names so viewer/game can interpret them
    if (info.isNormal) {
        dstImage.addChannelProps("Nrm.x,Nrm.y,X,X");
    }
    else if (info.isSRGB) {
        // !hasAlpha doesn't change the channel designation
        if (info.isPremultiplied) {
            dstImage.addChannelProps("Alb.ra,Alb.ga,Alb.ba,Alb.a");
        }
        else {
            dstImage.addChannelProps("Alb.r,Alb.g,Alb.b,Alb.a");
        }
    }

    // TODO: texture encode can depend on wrap vs. clamp state (f.e. normal map gen, sdf)
    // and formsts like PVRTC must know wrap/clamp before encode
    // address: Wrap, Clamp, MirrorWrap, MirrorClamp, BorderClamp, BorderClamp0
    // filter: Point, Linear, None (Mip only), TODO: what about Aniso (Mip only + level?)
    //   min/maxLOD too for which range of mips to use, atlas should stop before entries merge
    if (dstImage.textureType == MyMTLTextureType1DArray) {
        dstImage.addAddressProps("Rep,X,X");
    }
    else if (dstImage.textureType == MyMTLTextureType3D) {
        dstImage.addAddressProps("Rep,Rep,Rep");
    }
    else {
        dstImage.addAddressProps("Rep,Rep,X");
    }
    
    if (info.doMipmaps) {
        dstImage.addFilterProps("Lin,Lin,Lin");  // min,mag,mip
    }
    else {
        dstImage.addFilterProps("Lin,Lin,X");  // min,mag,mip
    }
   
    // This is hash of source png/ktx file (use xxhash32 or crc32)
    // can quickly check header if multiple copies of same source w/diff names.
    // May also need to store command line args in a prop to reject duplicate processing
    // TODO: ktxImage.addSourceHashProps(0);

    // convert props into a data blob that can be written out
    vector<uint8_t> propsData;
    dstImage.toPropsData(propsData);
    header.bytesOfKeyValueData = (uint32_t)propsData.size();

    //ktxImage.bytesPerBlock = header.blockSize();
    //ktxImage.blockDims = header.blockDims();

    int32_t storageSize = dstImage.mipLevelSize(w, h);

    // how much to store to store biggest level of ktx (will in-place mip to
    // this)
    int32_t storageSizeTotal = storageSize;

    vector<int32_t> mipOffsets;
    vector<int32_t> mipStorageSizes;
    int32_t numDstMipLevels = 0;
    int32_t numMipLevels = 0;

    // header only holds pixelFormat, but can generate block info from that
    computeMipStorage(dstImage, w, h,  // pixelFormat,
                      info.doMipmaps, info.mipMinSize, info.mipMaxSize,
                      storageSize, storageSizeTotal, mipStorageSizes,
                      numDstMipLevels, numMipLevels);

    // now compute the mip base offsets
    int32_t mipOffset = sizeof(KTXHeader) + header.bytesOfKeyValueData;

    for (int32_t i = 0; i < numMipLevels; ++i) {
        int32_t mipStorageSize = mipStorageSizes[i];
        if (mipStorageSize == 0) {
            mipOffsets.push_back(0);
            continue;
        }

        // 4 byte length of mip level is written out, this totally throws off block alignment
        // this is size of one mip not the array of mips of that size
        //if (!info.skipImageLength) {
            int32_t levelSizeOf = sizeof(uint32_t);
            mipOffset += levelSizeOf;
        //}

        // start of the mips
        mipOffsets.push_back(mipOffset);

        // ktx requires 4 byte alignment to rows of pixels (affext r8, rg8, r16f)
        // it's not enough to fix alignment below, so this needs fixed in mipStorage calc.
//        int32_t numPadding = 3 - ((mipStorageSize + 3) % 4);
//        if (numPadding != 0) {
//            // TODO: add error, need to pad rows not just stick pad at end
//            // this can happen on mips with formats below that don't align to 4 byte boundaries
//            // rgb8/16f also have this, but not supporting those formats currently.
//            return false;
//        }

        // next row of mips are offset
        mipOffset += mipStorageSize * header.totalChunks();
    }

    //----------------------------------------------

    header.numberOfMipmapLevels = numDstMipLevels;

    // store the largest mip size that isn't skipped
    for (auto mipStorageSize : mipStorageSizes) {
        if (mipStorageSize != 0) {
            header.pixelWidth = w;
            header.pixelHeight = h;
            break;
        }

        mipDown(w, h);
    }

    // update image to match
    dstImage.width = header.pixelWidth;
    dstImage.height = header.pixelHeight;
    dstImage.depth = header.pixelDepth;

    // ----------------------------------------------------

    // set the structure fields and allocate it, only need enough to hold single
    // mip (reuses mem) also because mips are written out to file after
    // generated.
    TextureData outputTexture;
    outputTexture.width = w;
    outputTexture.height = h;
    outputTexture.data.resize(storageSize);

    // restore full src size to build the mips
    w = modifiedWidth;
    h = modifiedHeight;

    // This is for 8-bit data (pixelsFloat used for in-place mipgen)
    ImageData srcImage;
    srcImage.width = w;
    srcImage.height = h;
    srcImage.isSRGB = info.isSRGB;
    srcImage.isHDR = info.isHDR;

    // ----------------------------------------------------

    vector<Color> tmpImageData8;  // for average channels per block

    // use this for complex texture types, copy data from vertical/horizotnal
    // strip image into here to then gen mips
    vector<Color> copyImage;

    // So can use simd ops to do conversions, use float4.
    // using half4 for mips of ldr data to cut memory in half
    // processing large textures nees lots of memory for src image
    // 8k x 8k x 8b = 500 mb
    // 8k x 8k x 16b = 1 gb
    vector<half4> halfImage;
    vector<float4> floatImage;

    bool doPremultiply = info.hasAlpha && (info.isPremultiplied || info.isPrezero);
    bool isMultichunk = chunkOffsets.size() > 1;

    if (info.isHDR) {
        // here the source is float

        // used to store chunks of the strip data
        if (isMultichunk) {
            floatImage.resize(w * h);
            srcImage.pixelsFloat = floatImage.data();
        }
        else {
            srcImage.pixelsFloat = (float4*)_pixelsFloat.data();
        }

        // run this across all the source data
        // do this in-place before mips are generated
        if (doPremultiply) {
            if (info.isPrezero) {
                for (const auto& pixel : _pixelsFloat) {
                    float alpha = pixel.w;
                    float4& pixelChange = const_cast<float4&>(pixel);
                    
                    // only premul at 0 alpha regions
                    if (alpha == 0.0f) {
                        pixelChange *= alpha;
                        pixelChange.w = alpha;
                    }
                }
            }
            else {
                for (const auto& pixel : _pixelsFloat) {
                    float alpha = pixel.w;
                    float4& pixelChange = const_cast<float4&>(pixel);
                    pixelChange *= alpha;
                    pixelChange.w = alpha;
                }
            }
        }
    }
    else {
        // used to store chunks of the strip data
        if (isMultichunk) {
            copyImage.resize(w * h);
            srcImage.pixels = copyImage.data();
        }
        else {
            srcImage.pixels = (Color*)_pixels.data();
        }

        // used to store premul and linear color
        if (info.isSRGB || doPremultiply) {
            halfImage.resize(w * h);

            // so large mips even if clamped with -mipmax allocate to largest mip size (2k x 2k @16 = 64MB)
            // have to build the mips off that.  srgb and premul is why fp32 is
            // needed, and need to downsample in linear space.

            //            int32_t size = w * h * sizeof(float4);
            //            if (size > 16*1024*1024) {
            //                int32_t bp = 0;
            //                bp = bp;
            //            }

            srcImage.pixelsHalf = halfImage.data();
        }
    }
    
    int32_t numChunks = (int32_t)chunkOffsets.size();

    // allocate to hold props and entire image to write out
    if (!dstFile) {
        // recompute, it's had mips added into it above
        mipOffset = sizeof(KTXHeader) + header.bytesOfKeyValueData;

        dstImage.initMipLevels(false, mipOffset);
        
        dstImage.reserveImageData();
    }
    
    // ----------------------------------------------------

    Mipper mipper;
    SDFMipper sdfMipper;
#if 0
    // TODO: can go out to KTX2 here instead
    // It has two different blocks, supercompression for BasisLZ
    // and a DFD block which details the block content.
    // And mips are reversed.
    bool doWriteKTX2 = false;
    if (doWriteKTX2 && dstFile) // in memory version will always be KTX1 format for nwo
    {
        KTX2Header header2;
        
        header2.vkFormat = vulkanType(info.pixelFormat);
        // header2.typeSize = 1; // skip
        
        header2.pixelWidth = header.pixelWidth;
        header2.pixelHeight = header.pixelHeight;
        header2.pixelDepth = header.pixelDepth;
        
        if (dstImage.textureType == MyMTLTextureType1DArray) {
            header2.pixelHeight = 0;
            header2.pixelDepth = 0;
        }
        
        header2.layerCount = header.numberOfArrayElements;
        header2.faceCount = header.numberOfFaces;
        header2.levelCount = numDstMipLevels; // header.numberOfMipmapLevels;
        
        // compute size of dfd
        vector<uint8_t> dfdData;
        
        // compute offsets and lengts of data blocks
        header2.dfdByteOffset = sizeof(header2);
        header2.kvdByteOffset = header2.dfdByteOffset + dfdData.size();
        header2.sgdByteOffset = header2.kvdByteOffset + propsData.size();
        
        header2.dfdByteLength = dfdData.size();
        header2.kvdByteLength = propsData.size();
        header2.sgdByteLength = 0;
        
        // TODO: figure out dfd here
        
        // write the header
        if (!writeDataAtOffset((const uint8_t*)&header2, sizeof(header2), 0, dstFile, dstImage)) {
            return false;
        }
        
        // write the dfd
        if (!writeDataAtOffset(dfdData.data(), dfdData.size(), header2.dfdByteOffset, dstFile, dstImage)) {
            return false;
        }
        
        // write the props
        if (!writeDataAtOffset(propsData.data(), propsData.size(), header2.kvdByteOffset, dstFile, dstImage)) {
            return false;
        }
        
        // skip supercompression block
        
        // TODO: this either writes to file or to dstImage (in-memory KTX file)
        
        // TODO: also need to support a few compressions
        // zstd and zlib, does dfd contain the offsets of each chunk
        // and the compressed sizes of mips.  Know format and sizes uncompressed.
        // but need to fill out the compressed size field.
        
        vector<KTX2ImageLevel> levels;
        levels.resize(numDstMipLevels);
        
        size_t levelListStartOffset = header2.sgdByteOffset + header2.sgdByteLength;
        size_t levelStartOffset = levelListStartOffset + levels.size() * sizeof(KTX2ImageLevel);
       
        size_t lastLevelOffset = levelStartOffset;
        for (int32_t i = 0; i < numDstMipLevels; ++i) {
            levels[i].length = numChunks * numDstMipLevels;
            levels[i].lengthCompressed = levels[i].length;
            levels[i].offset = lastLevelOffset + levels[i].lengthCompressed;
            lastLevelOffset = levels[i].offset;
        }
    
        // TODO: compress to a seperate zstd stream for each level
        // then can continue to do mips in place, and just append the bytes to that level
        // after compression.   If not compressed, then code from KTX1 can be used.
        bool isCompressed = false;
        
        if (!isCompressed) {
            if (!writeDataAtOffset(levels.data(), levels.size(), levelListStartOffset, dstFile, dstImage)) {
                return false;
            }
        }
        
        // TODO: here allocate a zstd encoder for each level
        vector< vector<uint8_t> > compressedLevels;
        if (isCompressed) {
            compressedLevels.resize(numDstMipLevels);
        }
        
        // write the chunks of mips see code below, seeks are important since
        // it's building mips on the fly.
        for (int32_t chunk = 0; chunk < numChunks; ++chunk) {
            // TODO: actually build the mip (reuse code below for KTX)
            
            if (!isCompressed)
                continue;
            
            // handle zstd compression here, and add to end of existing encoder for level
            zstd_compress(level);
            
            // append the compressed bytes to each strea
            levels[mipLevel].append(data);
        }
        
        if (isCompressed) {
            
            // update the offsets and compressed sizes
            lastLevelOffset = levelStartOffset;
            for (int32_t i = 0; i < numDstMipLevels; ++i) {
                levels[i].lengthCompressed = compressedLevels[i].size();
                levels[i].offset = lastLevelOffset + levels[i].lengthCompressed;
                lastLevelOffset = levels[i].offset;
            }
            
            // write out sizes
            if (!writeDataAtOffset(levels.data(), levels.size(), levelListStartOffset, dstFile, dstImage)) {
                return false;
            }
            
            // and now seek and write out each compressed level
            for (int32_t i = 0; i < numDstMipLevels; ++i) {
                if (!writeDataAtOffset(compressedLevels[i].data(), compressedLevels[i].size(), levels[i].offset, dstFile, dstImage)) {
                    return false;
                }
            }
        }
        
        return true;
    }
#endif
    
    // ----------------------------------------------------

    // write the header out
    KTXHeader headerCopy = header;
    if (dstImage.textureType == MyMTLTextureType1DArray) {
        headerCopy.pixelHeight = 0;
        headerCopy.pixelDepth = 0;
    }
    if (!writeDataAtOffset((const uint8_t*)&headerCopy, sizeof(headerCopy), 0, dstFile, dstImage)) {
        return false;
    }

    // write out the props
    if (!writeDataAtOffset(propsData.data(), propsData.size(), sizeof(KTXHeader), dstFile, dstImage)) {
        return false;
    }

    for (int32_t chunk = 0; chunk < numChunks; ++chunk) {
        // this needs to append before chunkOffset copy below
        w = modifiedWidth;
        h = modifiedHeight;

        // copy a chunk at a time, mip that if needed, and then move to next chunk
        Int2 chunkOffset = chunkOffsets[chunk];

        // reset these dimensions, or the mip mapping drops them to 1x1
        srcImage.width = w;
        srcImage.height = h;
        
        if (info.isHDR) {
            if (isMultichunk) {
                const float4* srcPixels = (const float4*)_pixelsFloat.data();
                for (int32_t y = 0; y < h; ++y) {
                    int32_t y0 = y * w;
                    
                    // offset into original strip/atlas
                    int32_t yOffset = (y + chunkOffset.y) * _width + chunkOffset.x;

                    for (int32_t x = 0; x < w; ++x) {
                        float4 c0 = srcPixels[yOffset + x];
                        float4& d0 = floatImage[y0 + x];
                        d0 = c0;
                    }
                }
            }
        }
        else {
            if (isMultichunk) {
                const Color* srcPixels = (const Color*)_pixels.data();
                for (int32_t y = 0; y < h; ++y) {
                    int32_t y0 = y * w;
                    
                    // offset into original strip/atlas
                    int32_t yOffset = (y + chunkOffset.y) * _width + chunkOffset.x;

                    for (int32_t x = 0; x < w; ++x) {
                        Color c0 = srcPixels[yOffset + x];
                        Color& d0 = copyImage[y0 + x];
                        d0 = c0;
                    }
                }
            }

            if (info.doSDF) {
                sdfMipper.init(srcImage, info.isVerbose);
            }
            else {
                // copy and convert to half4 or float4 image
                // srcImage already points to float data, so could modify that
                // only need doPremultiply at the top mip
                mipper.initPixelsHalfIfNeeded(srcImage, doPremultiply && !info.isPrezero, info.isPrezero,
                                              halfImage);
            }
        }

        // doing in-place mips
        ImageData dstImageData = srcImage;

        //----------------------------------------------

        // build mips for the chunk, dropping mips as needed, but downsampling
        // from available image

        int32_t numDstMipLevelsWritten = 0;
        for (int32_t mipLevel = 0; mipLevel < numMipLevels; ++mipLevel) {
            // no need to mip futher
            if (numDstMipLevelsWritten >= numDstMipLevels) {
                break;
            }

            bool skipMip = false;
            uint32_t mipStorageSize = mipStorageSizes[mipLevel];
            if (mipStorageSize == 0) {
                skipMip = true;
            }

            // this does in-place mipmap to dstImage (also updates floatPixels
            // if used)
            if (info.doSDF) {
                // have to process all images to SDF
                if (!skipMip) {
                    // sdf mipper has to build from origin sourceImage
                    // but it can in-place write to the same dstImage
                    sdfMipper.mipmap(dstImageData, mipLevel);

                    w = dstImageData.width;
                    h = dstImageData.height;
                }
            }
            else {
                // can export existing image for mip 0
                if (mipLevel > 0) {
                    // have to build the submips even with skipMip
                    mipper.mipmap(srcImage, dstImageData);

                    // dst becomes src for next in-place mipmap
                    srcImage = dstImageData;

                    w = dstImageData.width;
                    h = dstImageData.height;
                }
            }

            // only write out mip if non-zero storage
            if (skipMip) {
                continue;
            }

            // mipOffsets are start of first chunk of a given mip size
            mipOffset = mipOffsets[mipLevel] + chunk * mipStorageSize;
            numDstMipLevelsWritten++;

            // just to check that each mip has a unique offset
            //KLOGI("Image", "chunk:%d %d\n", chunk, mipOffset);

            // average channels per block if requested (mods 8-bit data on a per block basis)
            ImageData mipImage = dstImageData;

            if (!info.averageChannels.empty()) {
                // this isn't applied to srgb data (what about premul?)
                averageChannelsInBlock(info.averageChannels.c_str(), dstImage,
                                       mipImage, tmpImageData8);

                mipImage.pixels = tmpImageData8.data();
                mipImage.pixelsFloat = nullptr;
            }

            Timer timer;
            bool success =
                compressMipLevel(info, dstImage,
                                 mipImage, outputTexture, mipStorageSize);
            assert(success);

            if (success) {
                if (info.isVerbose) {
                    KLOGI("Image", "Compressed mipLevel %dx%d in %0.3fs\n", w, h, timer.timeElapsed());
                }
            }

            // Write out the mip size on chunk0, all other mips are this size since not supercompressed.
            // This throws off block alignment so have option to skip for ktxa files.  I guess 3d textures
            // and arrays can then load entire level in a single call.
            if (chunk == 0) {
                // some clarification on what imageSize means, but best to look at ktx codebase itself
                // https://github.com/BinomialLLC/basis_universal/issues/40

                // this contains all bytes at a mipLOD but not any padding
                uint32_t levelSize = (int32_t)chunkOffsets.size() * mipStorageSize;

                // this is size of one face for non-array cubes
                if (info.textureType == MyMTLTextureTypeCube) {
                    levelSize = mipStorageSize;
                }

                int32_t levelSizeOf = sizeof(levelSize);
                assert(levelSizeOf == 4);

                //fseek(dstFile, mipOffset - levelSizeOf, SEEK_SET);  // from begin

                if (!writeDataAtOffset((const uint8_t*)&levelSize, levelSizeOf, mipOffset - levelSizeOf, dstFile, dstImage)) {
                    return false;
                }
            }

            //fseek(dstFile, mipOffset, SEEK_SET);  // from begin

            // Note that default ktx alignment is 4, so r8u, r16f mips need to be padded out to 4 bytes
            // may need to write these out row by row, and let fseek pad the rows to 4.

            if (!writeDataAtOffset(outputTexture.data.data(), mipStorageSize, mipOffset, dstFile, dstImage)) {
                return false;
            }
        }
    }
    return true;
}

// TODO: try to elim KTXImage passed into this
bool Image::compressMipLevel(const ImageInfo& info, KTXImage& image,
                             ImageData& mipImage, TextureData& outputTexture,
                             int32_t mipStorageSize) const
{
    int32_t w = mipImage.width;

    const Color* srcPixelData = mipImage.pixels;
    const float4* srcPixelDataFloat4 = mipImage.pixelsFloat;

    int32_t h = mipImage.height;
    ;

    if (info.isExplicit) {
        switch (info.pixelFormat) {
            case MyMTLPixelFormatR8Unorm:
            case MyMTLPixelFormatRG8Unorm:
            case MyMTLPixelFormatRGBA8Unorm:
            case MyMTLPixelFormatRGBA8Unorm_sRGB: {
                int32_t count = image.blockSize() / 1;

                uint8_t* dst = (uint8_t*)outputTexture.data.data();

                // the 8-bit data is already in srcPixelData
                const Color* src = srcPixelData;

                // assumes we don't need to align r/rg rows to 4 bytes
                for (int32_t i = 0, iEnd = w * h; i < iEnd; ++i) {
                    switch (count) {
                        case 4:
                            dst[count * i + 3] = src[i].a;
                        case 3:
                            dst[count * i + 2] = src[i].b;
                        case 2:
                            dst[count * i + 1] = src[i].g;
                        case 1:
                            dst[count * i + 0] = src[i].r;
                    }
                }

                break;
            }

            case MyMTLPixelFormatR16Float:
            case MyMTLPixelFormatRG16Float:
            case MyMTLPixelFormatRGBA16Float: {
                int32_t count = image.blockSize() / 2;

                half* dst = (half*)outputTexture.data.data();

                const float4* src = mipImage.pixelsFloat;

                // assumes we don't need to align r16f rows to 4 bytes
                for (int32_t i = 0, iEnd = w * h; i < iEnd; ++i) {
                    half4 src16 = toHalf4(src[0]);

                    switch (count) {
                        case 4:
                            dst[count * i + 3] = src16.w;
                        case 3:
                            dst[count * i + 2] = src16.z;
                        case 2:
                            dst[count * i + 1] = src16.y;
                        case 1:
                            dst[count * i + 0] = src16.x;
                    }
                }
                break;
            }
            case MyMTLPixelFormatR32Float:
            case MyMTLPixelFormatRG32Float:
            case MyMTLPixelFormatRGBA32Float: {
                int32_t count = image.blockSize() / 4;

                float* dst = (float*)outputTexture.data.data();

                const float4* src = mipImage.pixelsFloat;

                for (int32_t i = 0, iEnd = w * h; i < iEnd; ++i) {
                    switch (count) {
                        case 4:
                            dst[count * i + 3] = src[i].w;
                        case 3:
                            dst[count * i + 2] = src[i].z;
                        case 2:
                            dst[count * i + 1] = src[i].y;
                        case 1:
                            dst[count * i + 0] = src[i].x;
                    }
                }

                break;
            }
            default:
                return false;
                break;
        }

        return true;
    }
    else if (info.isETC) {
#if COMPILE_ETCENC
        if (info.useEtcenc) {
            // only have 8-bit data above in srcPixelData, not full float data
            // at this point why can't encoders take that.  They shouldn't
            // require Float4.
            float effort = (int32_t)info.quality;
            // NOTE: have qualty control, RG11 runs all after 50.  Unity
            // uses 70.0

            Etc::ErrorMetric errMetric = Etc::ErrorMetric::NUMERIC;
            Etc::Image::Format format = Etc::Image::Format::R11;

            // errorMetric is unused for r/rg11 format, it just uses sum(deltaChannel^2),
            // but each channel can be done independently.

            // Note: the error calcs for Rec709 cause an 8s (20s vs. 28s) slowdown over numeric
            // on color_grid-a.png to etc2rgb.  So stop using it.  The error calc is called
            // millions of times, so any work done there adds up.

            bool useRec709 = false;  // info.isColorWeighted;

            switch (info.pixelFormat) {
                case MyMTLPixelFormatEAC_R11Unorm:
                    format = Etc::Image::Format::R11;
                    break;
                case MyMTLPixelFormatEAC_R11Snorm:
                    format = Etc::Image::Format::SIGNED_R11;
                    break;
                case MyMTLPixelFormatEAC_RG11Unorm:
                    format = Etc::Image::Format::RG11;
                    break;
                case MyMTLPixelFormatEAC_RG11Snorm:
                    format = Etc::Image::Format::SIGNED_RG11;
                    break;

                case MyMTLPixelFormatETC2_RGB8:
                    format = Etc::Image::Format::RGB8;
                    if (useRec709)
                        errMetric = Etc::ErrorMetric::REC709;
                    if (!info.hasColor)
                        errMetric = Etc::ErrorMetric::GRAY;
                    break;
                case MyMTLPixelFormatETC2_RGB8_sRGB:
                    format = Etc::Image::Format::SRGB8;
                    if (useRec709)
                        errMetric = Etc::ErrorMetric::REC709;
                    if (!info.hasColor)
                        errMetric = Etc::ErrorMetric::GRAY;
                    break;
                case MyMTLPixelFormatEAC_RGBA8:
                    format = Etc::Image::Format::RGBA8;
                    if (useRec709) errMetric = Etc::ErrorMetric::REC709;
                    if (!info.hasColor)
                        errMetric = Etc::ErrorMetric::GRAY;
                    break;
                case MyMTLPixelFormatEAC_RGBA8_sRGB:
                    format = Etc::Image::Format::SRGBA8;
                    if (useRec709) errMetric = Etc::ErrorMetric::REC709;
                    if (!info.hasColor)
                        errMetric = Etc::ErrorMetric::GRAY;
                    break;

                default:
                    return false;
                    break;
            }

            Etc::Image imageEtc(format, (const Etc::ColorR8G8B8A8*)srcPixelData, w, h, errMetric);

            imageEtc.SetVerboseOutput(info.isVerbose);
            Etc::Image::EncodingStatus status;

            // TODO: have encoder setting to enable multipass
            bool doSinglepass = true;  // || (effort == 100.0f); // problem is 100% quality also runs all passes
            if (doSinglepass) {
                // single pass iterates each block until done
                status = imageEtc.EncodeSinglepass(effort, outputTexture.data.data());
            }
            else {
                // multipass iterates all blocks once, then a percentage of the blocks with highest errors
                // if that percentage isn't already reached in the first pass.  Below a certain block count
                // all blocks are processed until done.  So only the largest mips have less quality.
                float blockPercent = effort;
                status = imageEtc.Encode(blockPercent, effort, outputTexture.data.data());
            }

            // all errors/warnings turned into asserts
            if (status != Etc::Image::SUCCESS) {
                return false;
            }

            return true;
        }
#endif
    }
    else if (info.isBC) {
        // use the whole image compression function to do the work
        bool doRemapSnormEndpoints = false;
        bool success = false;

        if (false) {
            // just to keep chain below
        }
#if COMPILE_BCENC
        else if (info.useBcenc) {
            // these must be called once before any compress call, might be able to move out to ctor
            rgbcx::init();
            bc7enc_compress_block_init();

            bc7enc_compress_block_params bc7params;
            uint32_t bc1QualityLevel = 0;
            uint32_t bc3QualityLevel = 0;

            if (info.pixelFormat == MyMTLPixelFormatBC7_RGBAUnorm ||
                info.pixelFormat == MyMTLPixelFormatBC7_RGBAUnorm_sRGB) {
                bc7enc_compress_block_params_init(&bc7params);
                if (!info.isColorWeighted) {
                    bc7enc_compress_block_params_init_linear_weights(&bc7params);
                }

                uint32_t uberLevel = 0;
                uint32_t maxPartitions = 0;

                // Can see timings on the home page here.  bc7enc isn't vectorized.
                // https://github.com/richgel999/bc7enc16

                if (info.quality <= 10) {
                    uberLevel = 0;
                    maxPartitions = 0;
                    bc7params.m_try_least_squares = false;
                    bc7params.m_mode_partition_estimation_filterbank = true;
                }
                else if (info.quality <= 40) {
                    uberLevel = 0;
                    maxPartitions = 16;
                    bc7params.m_try_least_squares = false;
                    bc7params.m_mode_partition_estimation_filterbank = true;
                }
                else if (info.quality <= 90) {
                    uberLevel = 1;
                    maxPartitions = 64;
                    bc7params.m_try_least_squares = true;  // true = 0.7s on test case
                    bc7params.m_mode_partition_estimation_filterbank = true;
                }
                else {
                    uberLevel = 4;
                    maxPartitions = 64;
                    bc7params.m_try_least_squares = true;
                    bc7params.m_mode_partition_estimation_filterbank = true;
                }

                bc7params.m_uber_level = std::min(uberLevel, (uint32_t)BC7ENC_MAX_UBER_LEVEL);
                bc7params.m_max_partitions_mode = std::min(maxPartitions, (uint32_t)BC7ENC_MAX_PARTITIONS1);
            }
            else if (info.pixelFormat == MyMTLPixelFormatBC1_RGBA ||
                     info.pixelFormat == MyMTLPixelFormatBC1_RGBA_sRGB ||
                     info.pixelFormat == MyMTLPixelFormatBC3_RGBA ||
                     info.pixelFormat == MyMTLPixelFormatBC3_RGBA_sRGB) {
                if (info.quality <= 10) {
                    bc1QualityLevel = (rgbcx::MAX_LEVEL * 1) / 4;
                }
                else if (info.quality <= 50) {
                    bc1QualityLevel = (rgbcx::MAX_LEVEL * 2) / 4;
                }
                else if (info.quality <= 90) {
                    bc1QualityLevel = (rgbcx::MAX_LEVEL * 3) / 4;
                }
                else {
                    bc1QualityLevel = rgbcx::MAX_LEVEL;
                }
                bc3QualityLevel = bc1QualityLevel;
            }

            uint8_t* dstData = (uint8_t*)outputTexture.data.data();

            const int32_t blockDim = 4;
            int32_t blocks_x = (w + blockDim - 1) / blockDim;
            //int32_t blocks_y = (h + blockDim - 1) / blockDim;
            int32_t blockSize = image.blockSize();
            for (int32_t y = 0; y < h; y += blockDim) {
                for (int32_t x = 0; x < w; x += blockDim) {
                    // Have to copy to temp block, since encode doesn't test w/h edges
                    // copy src to 4x4 clamping the edge pixels
                    // TODO: do clamped edge pixels get weighted more then on non-multiple of 4 images ?
                    Color srcPixelCopyAsBlock[blockDim * blockDim];
                    for (int32_t by = 0; by < blockDim; ++by) {
                        int32_t yy = y + by;
                        if (yy >= h) {
                            yy = h - 1;
                        }
                        for (int32_t bx = 0; bx < blockDim; ++bx) {
                            int32_t xx = x + bx;
                            if (xx >= w) {
                                xx = w - 1;
                            }

                            srcPixelCopyAsBlock[by * blockDim + bx] = srcPixelData[yy * w + xx];
                        }
                    }

                    const uint8_t* srcPixelCopy = (const uint8_t*)(srcPixelCopyAsBlock);

                    int32_t bx = x / blockDim;
                    int32_t by = y / blockDim;
                    int32_t b0 = by * blocks_x + bx;
                    uint8_t* dstBlock = &dstData[b0 * blockSize];

                    switch (info.pixelFormat) {
                        case MyMTLPixelFormatBC1_RGBA:
                        case MyMTLPixelFormatBC1_RGBA_sRGB: {
                            rgbcx::encode_bc1(bc1QualityLevel, dstBlock,
                                              srcPixelCopy, false, false);
                            break;
                        }
                        case MyMTLPixelFormatBC3_RGBA:
                        case MyMTLPixelFormatBC3_RGBA_sRGB: {
                            rgbcx::encode_bc3(bc3QualityLevel, dstBlock,
                                              srcPixelCopy);
                            break;
                        }

                        case MyMTLPixelFormatBC4_RUnorm:
                        case MyMTLPixelFormatBC4_RSnorm: {
                            rgbcx::encode_bc4(dstBlock, srcPixelCopy);
                            break;
                        }

                        case MyMTLPixelFormatBC5_RGUnorm:
                        case MyMTLPixelFormatBC5_RGSnorm: {
                            rgbcx::encode_bc5(dstBlock, srcPixelCopy);
                            break;
                        }

                        case MyMTLPixelFormatBC7_RGBAUnorm:
                        case MyMTLPixelFormatBC7_RGBAUnorm_sRGB: {
                            bc7enc_compress_block(dstBlock, srcPixelCopy, &bc7params);
                            break;
                        }
                        default: {
                            assert(false);
                        }
                    }
                }
            }

            if (info.isSigned) {
                doRemapSnormEndpoints = true;
            }

            success = true;
        }
#endif
#if COMPILE_ATE
        else if (info.useATE) {
            // Note: ATE can handle all BC formats except BC6.  But BC6 needs
            // fp16 data. Update in ate 2a, doxygen comment says decode only,
            // but actual comment doesn't say that.

            // encoder handles snorm, but then we'd need to fixup inputs
            // then.  Always take b4/b5 unorm path, and remap endpoints below.
            MyMTLPixelFormat pixelFormatRemap = info.pixelFormat;
            if (pixelFormatRemap == MyMTLPixelFormatBC4_RSnorm) {
                pixelFormatRemap = MyMTLPixelFormatBC4_RUnorm;
            }
            else if (pixelFormatRemap == MyMTLPixelFormatBC5_RGSnorm) {
                pixelFormatRemap = MyMTLPixelFormatBC5_RGUnorm;
            }

            ATEEncoder encoder;
            success = encoder.Encode(
                (int32_t)metalType(pixelFormatRemap), image.mipLevelSize(w, h), image.blockDims().y,
                info.hasAlpha,
                info.isColorWeighted, info.isVerbose, info.quality, w, h,
                (const uint8_t*)srcPixelData, outputTexture.data.data());

            if (info.isSigned) {
                doRemapSnormEndpoints = true;
            }
        }
#endif
#if COMPILE_SQUISH
        else if (info.useSquish) {
            static const float* noWeights = NULL;
            static const float perceptualWeights[3] = {
                0.2126f, 0.7152f, 0.0722f};  // weight g > r > b

            const float* weights =
                info.isColorWeighted ? &perceptualWeights[0] : noWeights;

            int32_t flags = 0;

            if (info.quality <= 10)
                flags = squish::kColourRangeFit;  // fast but inferior, only uses corner of color cube
            else if (info.quality <= 90)
                flags = squish::kColourClusterFit;  // decent speed and quality, fits to best line
            else
                flags = squish::kColourIterativeClusterFit;  // very slow, but
                                                             // slighting better
                                                             // quality

            squish::TexFormat format = squish::kBC1;

            success = true;

            switch (info.pixelFormat) {
                case MyMTLPixelFormatBC1_RGBA:
                case MyMTLPixelFormatBC1_RGBA_sRGB:
                    format = squish::kBC1;
                    break;
                case MyMTLPixelFormatBC3_RGBA_sRGB:
                case MyMTLPixelFormatBC3_RGBA:
                    format = squish::kBC3;
                    break;
                case MyMTLPixelFormatBC4_RSnorm:
                case MyMTLPixelFormatBC4_RUnorm:
                    format = squish::kBC4;
                    break;
                case MyMTLPixelFormatBC5_RGSnorm:
                case MyMTLPixelFormatBC5_RGUnorm:
                    format = squish::kBC5;
                    break;
                default:
                    success = false;
                    break;
            }

            if (success) {
                squish::CompressImage((const squish::u8*)srcPixelData, w, h,
                                      outputTexture.data.data(), format, flags,
                                      weights);

                if (info.isSigned) {
                    doRemapSnormEndpoints = true;
                }
            }
        }
#endif

        // Note: ate added signed formats to it's encoder library, so may
        // only need this for squish-based BC encoders.

        // have to remap endpoints to signed values (-1,1) to (0,127) for
        // (0,1) and (-128,-127,0) for (-1,0)/                        else
        if (success && info.isSigned && doRemapSnormEndpoints) {
            int32_t numBlocks = image.blockCount(w, h);
            int32_t blockSize = image.blockSize();

            int32_t blockSize16 = blockSize / sizeof(uint16_t);

            uint16_t* blockPtr = (uint16_t*)outputTexture.data.data();

            for (int32_t i = 0; i < numBlocks; ++i) {
                uint16_t* e0;
                uint16_t* e1;

                e0 = &blockPtr[0];

                if (info.pixelFormat == MyMTLPixelFormatBC4_RSnorm) {
                    // 2 8-bit endpoints
                    remapToSignedBCEndpoint88(*e0);
                }
                else if (info.pixelFormat == MyMTLPixelFormatBC5_RGSnorm) {
                    e1 = &blockPtr[4];

                    // 4 8-bit endpoints
                    remapToSignedBCEndpoint88(*e0);
                    remapToSignedBCEndpoint88(*e1);
                }

                // one encoding recommendation is not to specify -128 as
                // endpoints (use -127) e0 < e1 is meaningless then. since
                // they both represent -1, 0 is 128, 1 is 255

                blockPtr += blockSize16;
            }
        }

        return success;
    }
    else if (info.isASTC) {
#if COMPILE_ATE
        if (info.useATE) {
            ATEEncoder encoder;
            bool success = encoder.Encode(
                (int32_t)metalType(info.pixelFormat), image.mipLevelSize(w, h), image.blockDims().y,
                info.hasAlpha,
                info.isColorWeighted, info.isVerbose, info.quality, w, h,
                (const uint8_t*)srcPixelData, outputTexture.data.data());
            return success;
        }
#endif

#if COMPILE_ASTCENC

        if (info.useAstcenc) {
            /* can check help page on astcenc, to learn more about all of the
               weight settings
                "-normal_psnr -> "-rn -esw rrrg -dsw raz1 -ch 1 0 0 1 -oplimit
               1000 -mincorrel 0.99" " -rn" // " -esw gggr" // swizzle before
               encoding " -ch 0 1 0 1" // weight g and a " -oplimit 1000
               -mincorrel 0.99"

             */

            enum ChannelType {
                kChannelTypeOneR1 = 1,
                kChannelTypeTwoAG = 2,
                kChannelTypeTwoNormalAG = 3,  // not channel count
                kChannelTypeNormalFour = 4,
            };

            // TODO: handle isHDR, no L+A mode, so usr RG0?
            ChannelType channelType = kChannelTypeNormalFour;

            if (info.isNormal) {
                channelType = kChannelTypeTwoNormalAG;  // changes error metric
                assert(info.swizzleText == "rrrg" || info.swizzleText == "gggr");
            }
            else if (info.swizzleText == "rrrg" || info.swizzleText == "gggr") {
                channelType = kChannelTypeTwoAG;
            }
            else if (info.swizzleText == "rrr1") {
                channelType = kChannelTypeOneR1;
            }

            astcenc_profile profile;
            profile = info.isSRGB ? ASTCENC_PRF_LDR_SRGB : ASTCENC_PRF_LDR;
            if (info.isHDR) {
                profile =
                    ASTCENC_PRF_HDR;  // TODO: also ASTCENC_PRF_HDR_RGB_LDR_A
            }

            // not generating 3d ASTC ever, even for 3D textures
            Int2 blockDims = image.blockDims();

            // setup flags
            uint32_t flags = 0;

            if (channelType == kChannelTypeTwoNormalAG) {
                flags |= ASTCENC_FLG_MAP_NORMAL;  // weights r and a channels only in error calc
            }
            else if (info.isColorWeighted) {
                flags |= ASTCENC_FLG_USE_PERCEPTUAL;
            }
            else {
                // all channels are weighted the same
                flags |= ASTCENC_FLG_MAP_MASK;
            }
            // don't really need this
            // flags |= ASTCENC_FLG_USE_ALPHA_WEIGHT;

            // convert quality to present
            float quality = info.quality;
            
//            ASTCENC_PRE_FAST;
//            if (info.quality <= 10) {
//                preset = ASTCENC_PRE_FAST;
//            }
//            else if (info.quality <= 50) {
//                preset = ASTCENC_PRE_MEDIUM;
//            }
//            else if (info.quality < 90) {
//                preset = ASTCENC_PRE_THOROUGH;
//            }
//            else {
//                preset = ASTCENC_PRE_EXHAUSTIVE;
//            }

            astcenc_config config;
            astcenc_error error = astcenc_config_init(
                profile, blockDims.x, blockDims.y, 1, quality, flags, &config);
            if (error != ASTCENC_SUCCESS) {
                return false;
            }

            // weights are to channels after swizzle
            // pull from rgb = y, a = x, to match up with astcenc internals
            if (channelType == kChannelTypeOneR1) {
                config.cw_r_weight = 1.0f;
                config.cw_g_weight = 0.0f;  // rgb same value
                config.cw_b_weight = 0.0f;
                config.cw_a_weight = 0.0f;  // set to 0 to indicate alpha error doesn't matter (always 1)
            }
            else if (channelType == kChannelTypeTwoAG) {
                config.cw_r_weight = 1.0f;
                config.cw_g_weight = 0.0f;  // rgb same value
                config.cw_b_weight = 0.0f;
                config.cw_a_weight = 1.0f;
            }
            else if (channelType == kChannelTypeTwoNormalAG) {
                config.cw_r_weight = 1.0f;
                config.cw_g_weight = 0.0f;  // rgb same value
                config.cw_b_weight = 0.0f;
                config.cw_a_weight = 1.0f;
            }

            // TOOD: have other weightings here for 1/2/3 channel.
            // Note: no L+A mode for HDR, only L, RGB, RGBA
            // and RGB can be stored as dual plane (f.e. RBA - plane1, G plane
            // 2)

            // these are already set above, but can override
            //            config.tune_partition_early_out_limit =
            //            config.tune_two_plane_early_out_limit =
            //            config.tune_refinement_limit =
            //            config.tune_db_limit = // ldr only
            //            config.tune_partition_limit =
            //            config.tune_block_mode_limit =
            //            config.a_scale_radius =

            // Note: this can accept fp16 src, but have already converted to
            // float4
            astcenc_image srcImage;
            srcImage.dim_x = w;
            srcImage.dim_y = h;
            srcImage.dim_z = 1;  // Not using 3D blocks, not supported on iOS
            //srcImage.dim_pad = 0;

            // data is triple-pointer so it can work with 3d textures, but only
            // have 2d image
            // hacked the src pixel handling to only do slices, not a 3D texture
            if (info.isHDR) {
                srcImage.data = (void**)&srcPixelDataFloat4;
                srcImage.data_type = ASTCENC_TYPE_F32;
            }
            else {
                srcImage.data = (void**)&srcPixelData;
                srcImage.data_type = ASTCENC_TYPE_U8;
            }

            // swizzle not used, already do swizzle
            astcenc_swizzle swizzleEncode = {ASTCENC_SWZ_R, ASTCENC_SWZ_G,
                                             ASTCENC_SWZ_B, ASTCENC_SWZ_A};

            // could this be built once, and reused across all mips
            astcenc_context* codec_context = nullptr;
            error = astcenc_context_alloc(&config, 1, &codec_context);
            if (error != ASTCENC_SUCCESS) {
                return false;
            }

#if 0
            // This hackimproves L1 and LA block generating
            // even enabled dual-plane mode for LA.  Otherwise rgb and rgba blocks
            // are generated on data that only contain L or LA blocks.

            bool useUniqueChannels = true;
            if (useUniqueChannels) {
                gAstcenc_UniqueChannelsInPartitioning = 4;

                switch (channelType) {
                    case kChannelTypeOneR1:
                        gAstcenc_UniqueChannelsInPartitioning = 1;
                        break;

                    case kChannelTypeTwoAG:
                    case kChannelTypeTwoNormalAG:
                        gAstcenc_UniqueChannelsInPartitioning = 2;
                        break;

                    default:
                        useUniqueChannels = false;
                        break;
                };
            }

            error = astcenc_compress_image(
                codec_context, &srcImage, swizzleEncode,
                outputTexture.data.data(), mipStorageSize,
                0);  // threadIndex

            if (useUniqueChannels && gAstcenc_UniqueChannelsInPartitioning != 4) {
                gAstcenc_UniqueChannelsInPartitioning = 4;
            }
#else
            error = astcenc_compress_image(
                codec_context, &srcImage, swizzleEncode,
                outputTexture.data.data(), mipStorageSize,
                0);  // threadIndex
#endif

            // Or should this context only be freed after all mips?
            astcenc_context_free(codec_context);

            if (error != ASTCENC_SUCCESS) {
                return false;
            }
            return true;
        }
#endif
    }

    return false;
}

}  // namespace kram
