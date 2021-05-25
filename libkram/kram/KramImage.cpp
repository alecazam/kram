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

// for zlib compress
#include "miniz.h"

// for zstd compress
#include "zstd.h"

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

// return the block mode of a bc7 block, or -1 if finvalid
int32_t decodeBC7BlockMode(const void *pBlock)
{
    const uint32_t first_byte = static_cast<const uint8_t*>(pBlock)[0];

    for (uint32_t mode = 0; mode <= 7; mode++)
    {
        // bit followed by zeros, mask out upper
        uint8_t bits = (1U << mode);
        
        if ((first_byte & bits) == bits)
        {
            return mode;
        }
    }

    return -1;
}

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

    // TODO: handle loading custom mips.  Save will currently box filter to build
    // remaining mips but for SDF or coverage scaled alpha test, need to
    // preserve original data.  Problem is that Image save to KTX/2 always does in-place
    // mipgen.
    
    if (image.header.numberOfMipmapLevels > 1) {
        KLOGW("Image", "Skipping custom mip levels from KTX load");
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
                int32_t y0 = _width * y;

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
                int32_t y0 = _width * y;

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
                int32_t y0 = _width * y;

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
    dstHeader.bytesOfKeyValueData = (uint32_t)vsizeof(propsData);
    
    size_t mipOffset = sizeof(KTXHeader) + dstHeader.bytesOfKeyValueData;
    dstImage.initMipLevels(mipOffset);
    
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
    if (!writeDataAtOffset((const uint8_t*)&headerCopy, sizeof(KTXHeader), 0, dstFile, dstImage)) {
        return false;
    }
    
    // write out the props
    if (!writeDataAtOffset(propsData.data(), vsizeof(propsData), sizeof(KTXHeader), dstFile, dstImage)) {
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
    
    int32_t w = 0;
    int32_t h = 0;
    int32_t d = 0;
    
    for (int32_t chunk = 0; chunk < numChunks; ++chunk) {
        w = srcImage.width;
        h = srcImage.height;
        d = srcImage.depth;
        
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
            
            if (chunk == 0 && !dstImage.skipImageLength) {
                // sie of one mip
                uint32_t levelSize = dstMipLevel.length;
                
                // cubes write the face size, not the levels size, ugh
                if (srcImage.textureType != MyMTLTextureTypeCube) {
                    levelSize *= numChunks;
                }
                
                if (!writeDataAtOffset((const uint8_t*)&levelSize, sizeof(levelSize), dstMipLevel.offset - sizeof(levelSize), dstFile, dstImage)) {
                    return false;
                }
            }
            
            // only writing one mip at a time in the level here
            // so written bytes are only length and not numChunks * length
            int32_t dstMipOffset = dstMipLevel.offset + chunk * dstMipLevel.length;
            
            if (!writeDataAtOffset(outputTexture.data(), dstMipLevel.length, dstMipOffset, dstFile, dstImage)) {
                return false;
            }
            
            // next mip level
            mipDown(w, h, d);
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
    // dstImage will be ignored
    KTXImage dstImage;
    
    return encodeImpl(info, dstFile, dstImage);
}

// Use this for in-place construction of mips
struct MipConstructData {
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

    // Subdividing strips of larger images into cube/atlas/etc.
    // These offsets are where to find each chunk in that larger image
    vector<Int2> chunkOffsets;
    
    // Can skip the larger and smaller mips.  This is the larger mips skipped.
    uint32_t numSkippedMips = 0;
    
    // 2d image src after accounting for chunks for a strip of array/cube data
    uint32_t chunkWidth = 0;
    uint32_t chunkHeight = 0;
};



// See here:
// https://www.khronos.org/registry/DataFormat/specs/1.3/dataformat.1.3.html

enum KHR_DF_MODEL {
    KHR_DF_MODEL_RGBSDA = 1,
    
    KHR_DF_MODEL_BC1A = 128,
    // KHR_DF_MODEL_BC2 = 129,
    KHR_DF_MODEL_BC3 = 130,
    KHR_DF_MODEL_BC4 = 131,
    KHR_DF_MODEL_BC5 = 132,
    KHR_DF_MODEL_BC6H = 133,
    KHR_DF_MODEL_BC7 = 134,
    
    //KHR_DF_MODEL_ETC1 = 160,
    KHR_DF_MODEL_ETC2 = 161,
    
    KHR_DF_MODEL_ASTC = 162,
    
    //KHR_DF_MODEL_ETC1S = 163,
    
};

enum KHR_DF_CHANNEL {
    // guessing at these
    KHR_DF_CHANNEL_RED = 0,
    KHR_DF_CHANNEL_GREEN = 1,
    KHR_DF_CHANNEL_BLUE = 2,
    KHR_DF_CHANNEL_ALPHA = 15,
    
    // BC
    //KHR_DF_CHANNEL_BC1A_COLOR = 0,
    KHR_DF_CHANNEL_BC1A_ALPHA = 15,
    
    //KHR_DF_CHANNEL_BC2_COLOR = 0,
    KHR_DF_CHANNEL_BC2_ALPHA = 15,

    //KHR_DF_CHANNEL_BC3_COLOR = 0,
    KHR_DF_CHANNEL_BC3_ALPHA = 15,
    
    //KHR_DF_CHANNEL_BC4_DATA = 0,
  
    //KHR_DF_CHANNEL_BC5_RED = 0,
    KHR_DF_CHANNEL_BC5_GREEN = 1,
  
    //KHR_DF_CHANNEL_BC6H_COLOR = 0,
    //KHR_DF_CHANNEL_BC7_COLOR = 0,
    
    // ETC2
    //KHR_DF_CHANNEL_ETC2_RED = 0,
    KHR_DF_CHANNEL_ETC2_GREEN = 1,
    KHR_DF_CHANNEL_ETC2_COLOR = 2, // RGB
    KHR_DF_CHANNEL_ETC2_ALPHA = 16,
    
    // ASTC
    //KHR_DF_CHANNEL_ASTC_DATA = 0,
};


enum KHR_DF_PRIMARIES {
    KHR_DF_PRIMARIES_BT709 = 1
};

enum KHR_DF_TRANSFER {
    KHR_DF_TRANSFER_LINEAR = 1, // ?
    KHR_DF_TRANSFER_SRGB = 2,
};

enum KHR_DF_ALPHA {
    KHR_DF_FLAG_ALPHA_STRAIGHT = 0,
    KHR_DF_FLAG_ALPHA_PREMULTIPLIED = 1,
};
    
// 16 bytes total
struct KTX2DescriptorChannelBlock {
    
    // 32-bits
    uint16_t bitOffset = 0;
    uint8_t bitLength = 0;
    uint8_t channelType : 4; // RED, GREEN, BLUE, RRR, GGG
    uint8_t FSEL : 4; // L is low bit - Float, Signed, Exponent, Linear (used on Alpha)
    
    // 32-bits
    uint8_t samplePositions[4] = {0};

    uint32_t sampleLower = 0;
    uint32_t sampleUpper = UINT32_MAX;
};

// This can be up to 7 x 4 = 24 + 16 x channels in size
struct KTX2DescriptorFileBlock {
    KTX2DescriptorFileBlock(MyMTLPixelFormat format, bool isPremul, bool isCompressed);
    
    uint32_t totalSize = 0; // descriptorBlockSize + 4
    
    uint32_t vendorID : 18;
    uint32_t descriptorType : 14;
    uint16_t versionNumber = 2;
    uint16_t descriptorBlockSize = 0; // 24B + channels (doesn't include totalSize)
    
    uint8_t colorModel = 0;
    uint8_t colorPrimaries = 0;
    uint8_t transferFunction = 0;
    uint8_t flags = 0;
    
    uint8_t textureBlockDimensions[4] = {0};
    uint8_t bytesPlane[8] = {0};
    
    // now 16 bytes for each channel present
    KTX2DescriptorChannelBlock channels[4]; // max channels
};

KTX2DescriptorFileBlock::KTX2DescriptorFileBlock(MyMTLPixelFormat format, bool isPremul, bool isCompressed) {
    uint32_t numChannels = numChannelsOfFormat(format);
    Int2 blockDims = blockDimsOfFormat(format);
    bool isSrgb = isSrgbFormat(format);
    uint32_t blockSize = blockSizeOfFormat(format);
    bool isFloat = isFloatFormat(format);
    bool isSigned = isSignedFormat(format);
    
    totalSize = sizeof(KTX2DescriptorFileBlock) -
        (4 - numChannels) * sizeof(KTX2DescriptorChannelBlock);
    descriptorBlockSize = totalSize - 4;
    
    // ugly that these are all -1, can't simply read them in debugger
    textureBlockDimensions[0] = blockDims.x - 1;
    textureBlockDimensions[1] = blockDims.y - 1;
    
    vendorID = 0;
    descriptorType = 0;
    
    // these formats are all single-planes
    // some indication this should be 0 if zstd applied
    if (!isCompressed) {
        bytesPlane[0] = blockSize;
    }
    
    for (uint32_t i = 0; i < numChannels; ++i) {
        auto& c = channels[i];
        
        c.FSEL = 0;
        if (isSigned)
            c.FSEL |= 0x4;
        if (isFloat)
            c.FSEL |= 0x8;
    
        // TODO: what are E & L, nothing in docs about these ?
        // no examples of use of these either
        
        c.channelType = 0;
        
        if (isFloat) {
            // This is for BC6H, TODO: might be half only so test for isHalf?
            if (isSigned) {
                c.sampleLower = 0xBF800000U; // -1.0f;
                c.sampleUpper = 0x7F800000U; //  1.0f;
            }
            else {
                c.sampleLower = 0xBF800000U; //  -1.0f;
                c.sampleUpper = 0x7F800000U; //   1.0f;
            }
        }
        else if (isSigned) {
            c.sampleLower = INT32_MIN;
            c.sampleUpper = INT32_MAX;
        }
    }

    // set this since it applies to so many block formats
    channels[0].bitOffset = 0;
    channels[0].bitLength = blockSize * 8 - 1; // needs to be split of channel bits
   
    
    switch(format) {
        case MyMTLPixelFormatBC1_RGBA:
        case MyMTLPixelFormatBC1_RGBA_sRGB:
            // if ever do punchthrough-alpha
            //channels[1].channelType = KHR_DF_CHANNEL_BC1A_ALPHA;
            break;
            
        case MyMTLPixelFormatBC3_RGBA:
        case MyMTLPixelFormatBC3_RGBA_sRGB:
            // alpha is first
            channels[0].channelType = KHR_DF_CHANNEL_BC3_ALPHA;
            
            channels[0].bitOffset = 0;
            channels[0].bitLength = 64 - 1;
            
            channels[1].bitOffset = 64;
            channels[1].bitLength = 64 - 1;
            
            break;
            
        case MyMTLPixelFormatBC5_RGUnorm:
        case MyMTLPixelFormatBC5_RGSnorm:
            channels[1].channelType = KHR_DF_CHANNEL_BC3_ALPHA;
            
            channels[0].bitOffset = 0;
            channels[0].bitLength = 64 - 1;
            
            channels[1].bitOffset = 64;
            channels[1].bitLength = 64 - 1;
            
            break;
            
        // TODO: fix bc6h sampleLower/Upper
            
        // TODO: handle etc2
        case MyMTLPixelFormatEAC_RG11Unorm:
        case MyMTLPixelFormatEAC_RG11Snorm:
            channels[1].channelType = KHR_DF_CHANNEL_ETC2_GREEN;
            
            channels[0].bitOffset = 0;
            channels[0].bitLength = 64 - 1;
            
            channels[1].bitOffset = 64;
            channels[1].bitLength = 64 - 1;
            break;
            
        case MyMTLPixelFormatETC2_RGB8:
        case MyMTLPixelFormatETC2_RGB8_sRGB:
            channels[0].channelType = KHR_DF_CHANNEL_ETC2_COLOR;
            break;
            
            
        case MyMTLPixelFormatEAC_RGBA8:
        case MyMTLPixelFormatEAC_RGBA8_sRGB:
            channels[0].channelType = KHR_DF_CHANNEL_ETC2_ALPHA;
            channels[1].channelType = KHR_DF_CHANNEL_ETC2_COLOR;
            
            channels[0].bitOffset = 0;
            channels[0].bitLength = 64 - 1;
            
            channels[1].bitOffset = 64;
            channels[1].bitLength = 64 - 1;
            break;
            
            
        // NOTE: astc is all the same, and can already use defaults
    
        default: {
            uint32_t numChannelBits = (blockSize * 8) / numChannels;
            // handle uniform explcit types with offset per channel
            uint32_t lastBitOffset = 0;
            for (uint32_t i = 0; i < numChannels; ++i) {
                auto& c = channels[i];
                c.channelType = KHR_DF_CHANNEL_RED + i;
                c.bitOffset = lastBitOffset;
                c.bitLength = numChannelBits - 1;
                
                lastBitOffset += numChannelBits;
            }
                    
            colorModel = KHR_DF_MODEL_RGBSDA;
            break;
        }
    }
            
    colorPrimaries = KHR_DF_PRIMARIES_BT709;
    transferFunction = isSrgb ? KHR_DF_TRANSFER_SRGB : KHR_DF_TRANSFER_LINEAR;
    flags = isPremul ? KHR_DF_FLAG_ALPHA_PREMULTIPLIED : KHR_DF_FLAG_ALPHA_STRAIGHT;
}

void Image::addBaseProps(const ImageInfo& info, KTXImage& dstImage) const
{
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
}

// wish C++ had a defer
struct ZSTDScope
{
    ZSTDScope(ZSTD_CCtx* ctx_) : ctx(ctx_) {}
    ~ZSTDScope() { ZSTD_freeCCtx(ctx); }
    
private:
    ZSTD_CCtx* ctx = nullptr;
};


bool Image::encodeImpl(ImageInfo& info, FILE* dstFile, KTXImage& dstImage) const
{
    KTXHeader& header = dstImage.header;
    MipConstructData mipConstructData;
    
    vector<Int2>& chunkOffsets = mipConstructData.chunkOffsets;

    int32_t w = _width;
    int32_t h = _height;

    // compute chunks, and adjust w/h based on that
    // the code allows a vertical or horizontal strip or grid of chunks
    if (!validateTextureType(info.textureType, w, h, chunkOffsets, header,
                             info.doMipmaps,
                             info.chunksX, info.chunksY, info.chunksCount))
    {
        return false;
    }

    // This is wxh of source in case it has chunks
    // dstImage will start at this, but may mip down smaller base on mipMaxSize
    mipConstructData.chunkWidth = w;
    mipConstructData.chunkHeight = h;
    
    // work out how much memory we need to load
    header.initFormatGL(info.pixelFormat);

    dstImage.pixelFormat = info.pixelFormat;
    dstImage.textureType = info.textureType;

    // whd might be changed by initMipLevels based on min/max mip size
    dstImage.width = w;
    dstImage.height = h;
    dstImage.depth = header.pixelDepth; // from validate above
    
    dstImage.initMipLevels(info.doMipmaps, info.mipMinSize, info.mipMaxSize, info.mipSkip, mipConstructData.numSkippedMips);
    
    if (dstImage.mipLevels.empty()) {
        KLOGE("kram", "skipped all mips");
        return false;
    }
    // ----------------------------------------------------

    int32_t numChunks = (int32_t)chunkOffsets.size();

    //---------------
    // props

    addBaseProps(info, dstImage);
    
    // convert props into a data blob that can be written out
    vector<uint8_t> propsData;
    dstImage.toPropsData(propsData);
    header.bytesOfKeyValueData = (uint32_t)vsizeof(propsData);

    // ----------------------------------------------------

    // can go out to KTX2 here instead
    // It has two different blocks, supercompression for BasisLZ
    // and a DFD block which details the block content.
    // And mips are reversed.
    
    // dstImage case - in memory version will always be KTX1 format for now
    // this even gens a KTX1 dstImage, and then just compresses the mip levels
    
    if (info.isKTX2 && dstFile)
    {
        // generate KTX1 file with uncompressed mips first
        // a big memory hit here, since all mips stored in memory despite built in-place
        // could build and compress and entire level at a time, but can't write any of it
        // out until smallest mips are constructed.  Only then are offsets resolved.
        
        // A better way would be to do mips in-place, but in-order, and compressing the large
        // to small mips into an array of open compressor streams.  Then only need one mip instead of
        // all levels in memory.
        if (!writeKTX1FileOrImage(info,  mipConstructData, propsData, nullptr, dstImage)) {
            return false;
        }
        
        // now convert from ktx1 to ktx2
        
        KTX2Header header2;
        
        header2.vkFormat = vulkanType(info.pixelFormat);
        // header2.typeSize = 1; // skip
        
        header2.pixelWidth = header.pixelWidth;
        header2.pixelHeight = header.pixelHeight;
        header2.pixelDepth = header.pixelDepth;
                
        header2.layerCount = header.numberOfArrayElements;
        header2.faceCount = header.numberOfFaces;
        header2.levelCount = header.numberOfMipmapLevels;
        
        header2.supercompressionScheme = info.compressor.compressorType;
        
        // compute the dfd
        KTX2DescriptorFileBlock dfdData(info.pixelFormat, info.hasAlpha && info.isPremultiplied, info.compressor.isCompressed());
        
        // TODO: sgdData only used for BasisLZ, UASTC + zstd don't use this
        vector<uint8_t> sgdData;
        
        size_t levelByteLength = header2.levelCount * sizeof(KTXImageLevel);
        size_t levelByteOffset = sizeof(KTX2Header);
       
        // compute offsets and lengts of data blocks
        header2.dfdByteOffset = levelByteOffset + levelByteLength;
        header2.kvdByteOffset = header2.dfdByteOffset + dfdData.totalSize;
        header2.sgdByteOffset = header2.kvdByteOffset + vsizeof(propsData);
        
        header2.dfdByteLength = dfdData.totalSize;
        header2.kvdByteLength = vsizeof(propsData);
        header2.sgdByteLength = vsizeof(sgdData);
         
        // write the header
        if (!writeDataAtOffset((const uint8_t*)&header2, sizeof(KTX2Header), 0, dstFile, dstImage)) {
            return false;
        }
        
        // next are levels, but those are written out later
        
        // write the dfd
        if (!writeDataAtOffset((const uint8_t*)&dfdData, dfdData.totalSize, header2.dfdByteOffset, dstFile, dstImage)) {
            return false;
        }
        
        // write the props
        if (!writeDataAtOffset(propsData.data(), vsizeof(propsData), header2.kvdByteOffset, dstFile, dstImage)) {
            return false;
        }
        
        // skip supercompression block
        if (!sgdData.empty()) {
            // TODO: align(8) sgdPadding
            if (!writeDataAtOffset(sgdData.data(), vsizeof(sgdData), header2.sgdByteOffset, dstFile, dstImage)) {
                return false;
            }
        }
        
        // offsets will be largest last unlike KTX
        // data is packed without any length or alignment unllike in KTX
        // reverse the mip levels offsets (but not the order) for KTX2
        
        size_t imageByteOffset = header2.sgdByteOffset + header2.sgdByteLength;
        
        size_t lastImageByteOffset = imageByteOffset;
        
        vector<KTXImageLevel> ktx2Levels(dstImage.mipLevels);
        for (int32_t i = ktx2Levels.size() - 1; i >= 0; --i) {
            
            // align the offset to leastCommonMultiple(4, texel_block_size);
            if (lastImageByteOffset & 0x3) {
                lastImageByteOffset += 4 - (lastImageByteOffset & 0x3);
            }
            
            auto& level = ktx2Levels[i];
            level.length *= numChunks;
            level.lengthCompressed = level.length;
            level.offset = lastImageByteOffset;
            
            lastImageByteOffset = level.offset + level.length;
        }
    
        if (!info.compressor.isCompressed()) {
            if (!writeDataAtOffset((const uint8_t*)ktx2Levels.data(), vsizeof(ktx2Levels), levelByteOffset, dstFile, dstImage)) {
                return false;
            }
            
            // write the levels out
            for (int32_t i = 0; i < (int32_t)ktx2Levels.size(); ++i) {
                auto& level2 = ktx2Levels[i];
                auto& level1 = dstImage.mipLevels[i];
               
                if (!writeDataAtOffset(dstImage.fileData + level1.offset, level2.length, level2.offset, dstFile, dstImage)) {
                    return false;
                }
            }
        }
        else {

            // start compression with the smallest mips first, then can write out data as we go through it all
        
            // update the offsets and compressed sizes
            lastImageByteOffset = imageByteOffset;
            
            // allocate big enough to hold entire uncompressed level
            vector<uint8_t> compressedData;
            compressedData.resize(mz_compressBound(ktx2Levels.front().length)); // largest mip
            size_t compressedDataSize = 0;
            
            // reuse a context here
            ZSTD_CCtx* cctx = nullptr;
            int zlibLevel = MZ_DEFAULT_COMPRESSION;
                
            if (info.compressor.compressorType == KTX2SupercompressionZstd) {
                cctx = ZSTD_createCCtx();
                if (!cctx) {
                    return false;
                }
                
                if (info.compressor.compressorLevel > 0.0f) {
                   int zstdLevel = (int)round(info.compressor.compressorLevel);
                    if (zstdLevel > 100) {
                        zstdLevel = 100;
                    }
                    
                    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, zstdLevel);
                    
                    // may need to reset the compressor context, but says call starts a new frame
                }
            }
            else if (info.compressor.compressorType == KTX2SupercompressionZlib) {
                // set the level up
                if (info.compressor.compressorLevel > 0.0f) {
                    zlibLevel = (int)round(info.compressor.compressorLevel);
                    if (zlibLevel > 10) {
                        zlibLevel = 10;
                    }
                }
            }
            
            ZSTDScope scope(cctx);
            
            for (int32_t i = (int32_t)ktx2Levels.size() - 1; i >= 0; --i) {
                
                auto& level2 = ktx2Levels[i];
                auto& level1 = dstImage.mipLevels[i];
               
                const uint8_t* levelData = dstImage.fileData + level1.offset;
                
                // compress each mip
                switch(info.compressor.compressorType) {
                    case KTX2SupercompressionZstd: {
                        // this resets the frame on each call
                        compressedDataSize = ZSTD_compress2(cctx, compressedData.data(), compressedData.size(), levelData, level2.length);
                        
                        if (ZSTD_isError(compressedDataSize)) {
                            KLOGE("kram", "encode mip zstd failed");
                            return false;
                        }
                        break;
                    }
                    case KTX2SupercompressionZlib: {
                        mz_ulong dstSize = compressedData.size();
                        if (mz_compress2(compressedData.data(), &dstSize, levelData, level2.length, zlibLevel) != MZ_OK)
                        {
                            KLOGE("kram", "encode mip zlib failed");
                            return false;
                        }
                        compressedDataSize = dstSize;
                        
                        break;
                    }
                    default:
                        // should never get here
                        return false;
                }
                
                // also need for compressed levels?
                // align the offset to leastCommonMultiple(4, texel_block_size);
                if (lastImageByteOffset & 0x3) {
                    lastImageByteOffset += 4 - (lastImageByteOffset & 0x3);
                }
                
                level2.lengthCompressed = compressedDataSize;
                level2.offset = lastImageByteOffset;
                
                lastImageByteOffset = level2.offset + level2.lengthCompressed;
                
                // write the mip
                if (!writeDataAtOffset(compressedData.data(), compressedDataSize, level2.offset, dstFile, dstImage)) {
                    return false;
                }
            }
            
            // write out mip level size/offsets
            if (!writeDataAtOffset((const uint8_t*)ktx2Levels.data(), vsizeof(ktx2Levels), levelByteOffset, dstFile, dstImage)) {
                return false;
            }
        }
    }
    else {
        // this is purely ktx1 output path
        if (!writeKTX1FileOrImage(info, mipConstructData, propsData, dstFile, dstImage)) {
            return false;
        }
    }
    
    return true;
}

bool Image::writeKTX1FileOrImage(
     ImageInfo& info,
     MipConstructData& mipConstructData,
     const vector<uint8_t>& propsData,
     FILE* dstFile, KTXImage& dstImage) const
{
    // recompute, it's had mips added into it above
    size_t mipOffset = sizeof(KTXHeader) + dstImage.header.bytesOfKeyValueData;

    // allocate to hold props and entire image to write out
    if (!dstFile) {
        dstImage.initMipLevels(mipOffset);
        
        dstImage.reserveImageData();
    }
    else {
        int32_t numChunks = (int32_t)mipConstructData.chunkOffsets.size();
        
        // set offsets up for ktx1
        size_t lastMipOffset = mipOffset;
        
        for (int32_t i = 0; i < (int32_t)dstImage.mipLevels.size(); ++i) {
            auto& level = dstImage.mipLevels[i];
            level.offset = lastMipOffset + 4; // offset by length
            
            lastMipOffset = level.offset + level.length * numChunks;
        }
    }
    
    // write the header out
    KTXHeader headerCopy = dstImage.header;
    
    // fix header for 1d array
    // TODO: move to initMipLevels, and just use the header
    if (dstImage.textureType == MyMTLTextureType1DArray) {
        headerCopy.pixelHeight = 0;
        headerCopy.pixelDepth = 0;
    }
    
    if (!writeDataAtOffset((const uint8_t*)&headerCopy, sizeof(headerCopy), 0, dstFile, dstImage)) {
        return false;
    }

    // write out the props
    if (!writeDataAtOffset(propsData.data(), vsizeof(propsData), sizeof(KTXHeader), dstFile, dstImage)) {
        return false;
    }

    // build and weite out the mip data
    if (!createMipsFromChunks(info, mipConstructData, dstFile, dstImage)) {
        return false;
    }
    
    return true;
}

void printBCBlock(const uint8_t* bcBlock, MyMTLPixelFormat format) {
    // https://docs.microsoft.com/en-us/windows/win32/direct3d11/bc7-format-mode-reference#mode-6
    if (!(format == MyMTLPixelFormatBC7_RGBAUnorm || format == MyMTLPixelFormatBC7_RGBAUnorm_sRGB)) {
        return;
    }
    
    uint32_t mode = decodeBC7BlockMode(bcBlock);
    
    switch(mode) {
        case 6: {
            const uint64_t* block = (const uint64_t*)bcBlock;
            // 6 bits of signature - LSB 000001
            // 7 bits R0, 7 bits R1
            // 7 bits G0, 7 bits G1
            // 7 bits B0, 7 bits B1
            // 7 bits A0, 7 bits A1
            
            // 1 bit P0, 1 bit  P1
            // 63 bits of index data, how dos that work?
            
            uint32_t R0 = (uint32_t)((block[0] >> uint64_t(7*1)) & uint64_t(0b1111111));
            uint32_t R1 = (uint32_t)((block[0] >> uint64_t(7*2)) & uint64_t(0b1111111));
            
            uint32_t G0 = (uint32_t)((block[0] >> uint64_t(7*3)) & uint64_t(0b1111111));
            uint32_t G1 = (uint32_t)((block[0] >> uint64_t(7*4)) & uint64_t(0b1111111));
            
            uint32_t B0 = (uint32_t)((block[0] >> uint64_t(7*5)) & uint64_t(0b1111111));
            uint32_t B1 = (uint32_t)((block[0] >> uint64_t(7*6)) & uint64_t(0b1111111));
            
            uint32_t A0 = (uint32_t)((block[0] >> uint64_t(7*7)) & uint64_t(0b1111111));
            uint32_t A1 = (uint32_t)((block[0] >> uint64_t(7*8)) & uint64_t(0b1111111));
            
            uint32_t P0 = (uint32_t)((block[0] >> uint64_t(7*9)) & uint64_t(0b1));
            uint32_t P1 = (uint32_t)((block[1] >> uint64_t(0)) & uint64_t(0b1));
           
            // r,g,b,a to be or-ed with the pbit to get tha actual value of the endpoints
            
            KLOGI("kram",
                  "R0=%d, R1=%d\n"
                  "G0=%d, G1=%d\n"
                  "B0=%d, B1=%d\n"
                  "A0=%d, A1=%d\n"
                  "P0=%d, P1=%d\n",
                  R0, R1,
                  G0, G1,
                  B0, B1,
                  A0, A1,
                  P0, P1);
            
            break;
        }
    }
    
    // Have a block debug mode that hud's the mode pixel values
    // over the hovered block.
    uint32_t pixels[4*4];
    if (!unpack_bc7(bcBlock, (bc7decomp::color_rgba*)pixels)) {
        return;
    }
    
    for (uint32_t y = 0; y < 4; ++y) {
        KLOGI("kram",
              "[%u] = %08X %08X %08X %08X\n",
              y, pixels[4*y + 0], pixels[4*y + 1], pixels[4*y + 2], pixels[4*y + 3]
              );
    }
}

bool Image::createMipsFromChunks(
    ImageInfo& info,
    MipConstructData& data,
    FILE* dstFile,
    KTXImage& dstImage
) const
{
    // ----------------------------------------------------
    
    // set the structure fields and allocate it, only need enough to hold single
    // mip (reuses mem) also because mips are written out to file after
    // generated.
    TextureData outputTexture;
    outputTexture.width = dstImage.width;
    outputTexture.height = dstImage.height;
    outputTexture.data.resize(dstImage.mipLengthLargest());

    // This is for 8-bit data (pixelsFloat used for in-place mipgen)
    ImageData srcImage;
    srcImage.width = data.chunkWidth;
    srcImage.height = data.chunkHeight;
    
    // KramMipper uses these
    srcImage.isSRGB = info.isSRGB;
    srcImage.isHDR = info.isHDR;

    int32_t w = srcImage.width;
    int32_t h = srcImage.height;
    
    // ----------------------------------------------------
    
    // use this for complex texture types, copy data from vertical/horizotnal
    // strip image into here to then gen mips
    vector<Color>& copyImage = data.copyImage;

    // So can use simd ops to do conversions, use float4.
    // using half4 for mips of ldr data to cut memory in half
    // processing large textures nees lots of memory for src image
    // 8k x 8k x 8b = 500 mb
    // 8k x 8k x 16b = 1 gb
    vector<half4>& halfImage = data.halfImage;
    vector<float4>& floatImage = data.floatImage;

    int32_t numChunks = (int32_t)data.chunkOffsets.size();
    bool doPremultiply = info.hasAlpha && (info.isPremultiplied || info.isPrezero);
    bool isMultichunk = numChunks > 1;

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
    
    Mipper mipper;
    SDFMipper sdfMipper;
    
    vector<KTXImageLevel>& dstMipLevels = dstImage.mipLevels;
    
    int32_t srcTopMipWidth = srcImage.width;
    int32_t srcTopMipHeight = srcImage.height;
    
    for (int32_t chunk = 0; chunk < numChunks; ++chunk) {
        // this needs to append before chunkOffset copy below
        w = srcTopMipWidth;
        h = srcTopMipHeight;

        // copy a chunk at a time, mip that if needed, and then move to next chunk
        Int2 chunkOffset = data.chunkOffsets[chunk];

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
                        float4& d0 = data.floatImage[y0 + x];
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
                        Color& d0 = data.copyImage[y0 + x];
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
                mipper.initPixelsHalfIfNeeded(srcImage,
                                              doPremultiply && info.isPremultiplied,
                                              doPremultiply && info.isPrezero,
                                              data.halfImage);
            }
        }

        // doing in-place mips
        ImageData dstImageData = srcImage;

        //----------------------------------------------

        // build mips for the chunk, dropping mips as needed, but downsampling
        // from available image

        int32_t numSkippedMips = data.numSkippedMips;
        
        for (int32_t mipLevel = 0; mipLevel < (int32_t)dstMipLevels.size(); ++mipLevel) {
            const auto& dstMipLevel = dstMipLevels[mipLevel];
            
            if (mipLevel == 0 && !info.doSDF)
            {
                if (numSkippedMips > 0) {
                    // this does in-place mipmap to dstImage (also updates floatPixels if used)
                    for (int32_t i = 0; i < numSkippedMips; ++i) {
                        // have to build the submips even with skipMip
                        mipper.mipmap(srcImage, dstImageData);

                        // dst becomes src for next in-place mipmap
                        srcImage = dstImageData;

                        w = dstImageData.width;
                        h = dstImageData.height;
                    }
                }
            }
            else {
                if (info.doSDF) {
                    // have to process all images to SDF
                    // sdf mipper has to build from origin sourceImage
                    // but it can in-place write to the same dstImage
                    sdfMipper.mipmap(dstImageData, mipLevel);

                    w = dstImageData.width;
                    h = dstImageData.height;
                }
                else {
                    // have to build the submips even with skipMip
                    mipper.mipmap(srcImage, dstImageData);

                    // dst becomes src for next in-place mipmap
                    srcImage = dstImageData;

                    w = dstImageData.width;
                    h = dstImageData.height;
                }
            }
           
            // size of one mip, not levelSize = numChunks * mipStorageSize
            size_t mipStorageSize = dstMipLevel.length;
            
            // offset only valid for KTX and KTX2 w/o isCompressed
            size_t mipChunkOffset = dstMipLevel.offset + chunk * mipStorageSize;
           
            // just to check that each mip has a unique offset
            //KLOGI("Image", "chunk:%d %d\n", chunk, mipOffset);

            // average channels per block if requested (mods 8-bit data on a per block basis)
            ImageData mipImage = dstImageData;

            if (!info.averageChannels.empty()) {
                // this isn't applied to srgb data (what about premul?)
                averageChannelsInBlock(info.averageChannels.c_str(), dstImage,
                                       mipImage, data.tmpImageData8);

                mipImage.pixels = data.tmpImageData8.data();
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

            // Write out the mip size on chunk 0, all other mips are this size since not supercompressed.
            // This throws off block alignment and gpu loading of ktx files from mmap.  I guess 3d textures
            // and arrays can then load entire level in a single call.
            if ((!info.isKTX2) && chunk == 0) {
                // some clarification on what imageSize means, but best to look at ktx codebase itself
                // https://github.com/BinomialLLC/basis_universal/issues/40

                // this contains all bytes at a mipLOD but not any padding
                uint32_t levelSize = (uint32_t)mipStorageSize;

                // this is size of one face for non-array cubes
                // but for everything else, it's the numChunks * mipStorageSize
                if (info.textureType != MyMTLTextureTypeCube) {
                    levelSize *= numChunks;
                }

                int32_t levelSizeOf = sizeof(levelSize);
                assert(levelSizeOf == 4);

                if (!writeDataAtOffset((const uint8_t*)&levelSize, levelSizeOf, dstMipLevel.offset - levelSizeOf, dstFile, dstImage)) {
                    return false;
                }
            }
            
            // Note that default ktx alignment is 4, so r8u, r16f mips need to be padded out to 4 bytes
            // may need to write these out row by row, and let fseek pad the rows to 4.

            if (!writeDataAtOffset(outputTexture.data.data(), mipStorageSize, mipChunkOffset, dstFile, dstImage)) {
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

                    // bc7enc is not setting pbit on bc7 mode6 and doesn's support opaque mode3 yet
                    // , so opaque textures repro as 254 alpha on Toof-a.png.
                    // ate sets pbits on mode 6 for same block.  Also fixed mip weights in non-pow2 mipper.
                    
//                    bool doPrintBlock = false;
//                    if (bx == 8 && by == 1) {
//                        int32_t bp = 0;
//                        bp = bp;
//                        doPrintBlock = true;
//                    }
                    
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
                            
//                            if (doPrintBlock) {
//                                printBCBlock(dstBlock, info.pixelFormat);
//                            }
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
            
            
            // find the 8,1 block and print it
//            uint32_t numRowBlocks = image.blockCountRows(w);
//            const uint8_t* block = outputTexture.data.data() + (numRowBlocks * 1 + 8) * image.blockSize();
//            printBCBlock(block, pixelFormatRemap);
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
