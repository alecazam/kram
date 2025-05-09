// kram - Copyright 2020-2025 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

//#include <string>
//#include <vector>

//#include "KramConfig.h"
#include "KTXImage.h"
#include "KramMipper.h" // for Color

namespace kram {
class Image;

using namespace SIMD_NAMESPACE;
using namespace STL_NAMESPACE;

// each encoder has it's own set of outputs, can request encoder if overlap
enum TexEncoder {
    kTexEncoderUnknown = 0, // pick best encoder

    kTexEncoderExplicit, // r,rg,rgba 8|16f|32f

    kTexEncoderATE, // bc1,3,4,5,7,  and astc4x4,8x8 (macOS/iOS only),
                    // different lib versions and support based on OS version

    kTexEncoderSquish, // bc1,2,3,4,5

    kTexEncoderBcenc, // bc1,3,4,5,7

    kTexEncoderEtcenc, // etc-r,rg11, etc2, no HDR format

    kTexEncoderAstcenc,
};

// Fill this out from CLI, and hand to ImageInfo::init
class ImageInfoArgs {
public:
    MyMTLTextureType textureType = MyMTLTextureType2D;
    TexEncoder textureEncoder = kTexEncoderUnknown;
    MyMTLPixelFormat pixelFormat = MyMTLPixelFormatInvalid;
    string formatString = ""; // will convert to pixelFormat

    int32_t mipMinSize = 1;
    int32_t mipMaxSize = 32 * 1024;
    int32_t mipSkip = 0;

    int32_t quality = 49; // may want float

    // ktx2 has a compression type and level
    KTX2Compressor compressor;
    bool isKTX2 = false;

    bool doMipmaps = true; // default to mips on
    bool doMipflood = false;
    bool isVerbose = false;
    bool doSDF = false;

    bool isSourcePremultiplied = false; // skip further premul of src
    bool isPremultiplied = false;
    bool isPrezero = false;

    bool isNormal = false; // signed, but may be stored unorm and swizzled (f.e. astc/bc3nm gggr or rrrg)

    // can pick a smaller format if alpha = 1 (only for bc and etc)
    bool optimizeFormatForOpaque = false;

    // Two conversions occur - srgb -> lin to premul and build mip
    //                         lin -> srgb to encode
    // isSRGBDst and formatString set the pixelFormat
    bool isSRGBSrc = false;
    bool isSRGBSrcFlag = false;
    bool isSRGBDst = false;

    // For dst. TODO: could have signed source passed in
    bool isSigned = false;

    // Applies to src.  But also have hdr specific output formats.
    bool isHDR = false;

    // for now these are only usable with normal to height
    bool isHeight = false;
    bool isWrap = false;
    float heightScale = 1.0f;

    string swizzleText;
    string averageChannels;

    // count of chunks to help turn 2d texture into 2d array, 3d, cubearray
    int32_t chunksX = 0;
    int32_t chunksY = 0;
    int32_t chunksCount = 0;

    int32_t sdfThreshold = 120;
};

// preset data that contains all inputs about the encoding
class ImageInfo {
public:
    // Call initWithArgs first, and then initWithSourceImage after loading
    // srcImage.
    void initWithArgs(const ImageInfoArgs& args);
    void initWithSourceImage(Image& srcImage);

    // swizzles are run first, this is done in-place
    // this makea input pixels non-const.
    static void swizzleTextureHDR(int32_t w, int32_t h, float4* srcPixelsFloat_,
                                  const char* swizzleText);
    static void swizzleTextureLDR(int32_t w, int32_t h, Color* srcPixels_,
                                  const char* swizzleText);

    // convert x field to normals
    static void heightToNormals(int32_t w, int32_t h,
                                float4* srcPixelsFloat_,
                                Color* srcPixels_,
                                float scale, bool isWrap = false);

private:
    // this walks pixels for hasColor and hasAlpha if not already set to false
    void updateImageTraitsHDR(int32_t w, int32_t h,
                              const float4* srcPixelsFloat_);
    void updateImageTraitsLDR(int32_t w, int32_t h, const Color* srcPixels_);

    void optimizeFormat();

public:
    MyMTLTextureType textureType = MyMTLTextureType2D;
    MyMTLPixelFormat pixelFormat = MyMTLPixelFormatInvalid;
    TexEncoder textureEncoder = kTexEncoderUnknown;

    string averageChannels;
    string swizzleText;

    // ktx2 has a compression type and level
    KTX2Compressor compressor;
    bool isKTX2 = false;

    // source image state
    bool hasColor = false;
    bool hasAlpha = false;
    bool isSRGBSrc = false;
    bool isSRGBSrcFlag = false;

    // output image state
    bool isSRGBDst = false;
    bool isSigned = false;
    bool isNormal = false;
    bool isColorWeighted = false;
    bool isSourcePremultiplied = false;
    bool isPremultiplied = false; // don't premul
    bool isPrezero = false;
    bool isHDR = false;

    bool doSDF = false;
    bool doMipmaps = false;
    bool doMipflood = false;
    bool optimizeFormatForOpaque = false;

    bool isVerbose = false;

    // compression format
    bool isASTC = false;
    bool isBC = false;
    bool isETC = false;
    bool isExplicit = false;

    // encoder
    bool useATE = false;
    bool useSquish = false;
    bool useBcenc = false;
    bool useAstcenc = false;
    bool useEtcenc = false;
    bool useExplicit = false;

    // for now these are only usable with normal to height
    bool isHeight = false;
    bool isWrap = false;
    float heightScale = 1.0f;

    int32_t quality = 49;

    int32_t mipMinSize = 1;
    int32_t mipMaxSize = 32 * 1024;
    int32_t mipSkip = 0; // count of large mips to skip

    int32_t chunksX = 0;
    int32_t chunksY = 0;
    int32_t chunksCount = 0;

    // This converts incoming image channel to bitmap
    int32_t sdfThreshold = 120;
};

bool isSwizzleValid(const char* swizzle);

bool isChannelValid(const char* channels);

bool validateTextureType(MyMTLTextureType textureType, int32_t& w, int32_t& h,
                         vector<Int2>& chunkOffsets, KTXHeader& header,
                         bool& doMipmaps,
                         int32_t chunksX, int32_t chunksY, int32_t chunksCount);

bool validateFormatAndEncoder(ImageInfoArgs& infoArgs);

// decoders sometimes decode more than they encode.  Pick best from available decoders for format.
bool validateFormatAndDecoder(MyMTLTextureType textureType, MyMTLPixelFormat format, TexEncoder& textureEncoder);

MyMTLTextureType parseTextureType(const char* typeName);

TexEncoder parseEncoder(const char* encoder);

bool isEncoderAvailable(TexEncoder encoder);

const char* encoderName(TexEncoder encoder);

} // namespace kram
