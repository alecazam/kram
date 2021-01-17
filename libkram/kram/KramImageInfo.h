// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <string>
#include <vector>

#include "KTXImage.h"
#include "KTXMipper.h"  // for Color
#include "KramConfig.h"

namespace kram {
class Image;

using namespace simd;
using namespace std;

// each encoder has it's own set of outputs, can request encoder if overlap
enum TexEncoder {
    kTexEncoderUnknown = 0,  // pick best encoder

    kTexEncoderExplicit,  // r,rg,rgba 8|16f|32f

    kTexEncoderATE,  // bc1,3,4,5,7,  and astc4x4,8x8 (macOS/iOS only),
                     // different lib versions and support based on OS version

    kTexEncoderSquish,  // bc1,2,3,4,5

    kTexEncoderBcenc,  // bc1,3,4,5,7

    kTexEncoderEtcenc,  // etc-r,rg11, etc2, no HDR format

    kTexEncoderAstcenc,

    // TODO: add these for cross-platform support
    // bc - icbc, dxtex (BC6H)
};

// Fill this out from CLI, and hand to ImageInfo::init
class ImageInfoArgs {
public:
    MyMTLTextureType textureType = MyMTLTextureType2D;
    TexEncoder textureEncoder = kTexEncoderUnknown;
    MyMTLPixelFormat pixelFormat = MyMTLPixelFormatInvalid;
    string formatString = "";  // will convert to pixelFormat

    int mipMinSize = 1;
    int mipMaxSize = 32 * 1024;

    int quality = 49;  // may want float

    //bool skipImageLength = false;
    bool doMipmaps = true;  // default to mips on
    bool isVerbose = false;
    bool doSDF = false;
    bool isPremultiplied = false;
    bool isNormal = false;  // signed, but may be stored unorm and swizzled (f.e. astc/bc3nm gggr or rrrg)

    // can pick a smaller format if alpha = 1 (only for bc and etc)
    bool optimizeFormatForOpaque = false;

    // these and formatString set the pixelFormat
    bool isSigned = false;
    bool isSRGB = false;
    bool isHDR = false;

    string swizzleText;
    string averageChannels;
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
    static void swizzleTextureHDR(int w, int h, float4* srcPixelsFloat_,
                                  const char* swizzleText);
    static void swizzleTextureLDR(int w, int h, Color* srcPixels_,
                                  const char* swizzleText);

private:
    // this walks pixels for hasColor and hasAlpha if not already set to false
    void updateImageTraitsHDR(int w, int h,
                              const float4* srcPixelsFloat_);
    void updateImageTraitsLDR(int w, int h, const Color* srcPixels_);

    void optimizeFormat();

public:
    MyMTLTextureType textureType = MyMTLTextureType2D;
    //MyTexFormat format = kUnknown;
    MyMTLPixelFormat pixelFormat = MyMTLPixelFormatInvalid;
    TexEncoder textureEncoder = kTexEncoderUnknown;

    string averageChannels;
    string swizzleText;

    // output image state
    // Note: difference between input srgb and output srgb, but it's mingled
    // here a bit
    //bool isSnorm = false; // TODO: rename to isNormalized (with isSigned = snorm, without = unorm)
    bool isSigned = false;
    bool isNormal = false;
    bool isSRGB = false;
    bool isColorWeighted = false;
    bool isPremultiplied = false;  // don't premul
    bool isHDR = false;

    //bool skipImageLength = false;  // gen ktxa
    bool doSDF = false;
    bool doMipmaps = false;
    bool optimizeFormatForOpaque = false;

    // source image state
    bool hasColor = false;
    bool hasAlpha = false;

    bool isVerbose = false;

    // info about the format
    bool isASTC = false;
    bool isBC = false;
    bool isETC = false;
    bool isExplicit = false;

    bool useATE = false;
    bool useSquish = false;
    bool useBcenc = false;
    bool useAstcenc = false;
    bool useEtcenc = false;
    bool useExplicit = false;

    int quality = 49;

    int mipMinSize = 1;
    int mipMaxSize = 32 * 1024;
};

bool isSwizzleValid(const char* swizzle);

bool isChannelValid(const char* channels);

bool validateTextureType(MyMTLTextureType textureType, int& w, int& h,
                         vector<Int2>& chunkOffsets, KTXHeader& header,
                         bool& doMipmaps);

bool validateFormatAndEncoder(ImageInfoArgs& infoArgs);

// decoders sometimes decode more than they encode.  Pick best from available decoders for format.
bool validateFormatAndDecoder(MyMTLTextureType textureType, MyMTLPixelFormat format, TexEncoder& textureEncoder);

MyMTLTextureType parseTextureType(const char* typeName);

TexEncoder parseEncoder(const char* encoder);

bool isEncoderAvailable(TexEncoder encoder);

const char* encoderName(TexEncoder encoder);

}  // namespace kram
