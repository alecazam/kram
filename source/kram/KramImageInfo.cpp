// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramImageInfo.h"

#include "KramImage.h"

#if COMPILE_ATE
// encode/decode formats vary with version
#include "ate/ateencoder.h"
#endif

namespace kram {
using namespace std;
USING_SIMD;

#define isStringEqual(lhs, rhs) (strcmp(lhs, rhs) == 0)

MyMTLTextureType parseTextureType(const char* typeName)
{
    MyMTLTextureType type = MyMTLTextureType2D;

    if (isStringEqual(typeName, "cubearray")) {
        type = MyMTLTextureTypeCubeArray;
    }
    else if (isStringEqual(typeName, "2darray")) {
        type = MyMTLTextureType2DArray;
    }
    else if (isStringEqual(typeName, "1darray")) {
        type = MyMTLTextureType1DArray;
    }
    else if (isStringEqual(typeName, "3d")) {
        type = MyMTLTextureType3D;
    }
    else if (isStringEqual(typeName, "2d")) {
        type = MyMTLTextureType2D;
    }
    //    else if (isStringEqual(typeName, "1d")) {
    //        type = MyMTLTextureType1D;
    //    }
    else if (isStringEqual(typeName, "cube")) {
        type = MyMTLTextureTypeCube;
    }
    return type;
}

TexEncoder parseEncoder(const char* encoder)
{
    TexEncoder textureEncoder = kTexEncoderUnknown;

    if (isStringEqual(encoder, "explicit")) {
        textureEncoder = kTexEncoderExplicit;
    }
    else if (isStringEqual(encoder, "squish")) {
        textureEncoder = kTexEncoderSquish;
    }
    else if (isStringEqual(encoder, "etcenc")) {
        textureEncoder = kTexEncoderEtcenc;
    }
    else if (isStringEqual(encoder, "bcenc")) {
        textureEncoder = kTexEncoderBcenc;
    }
    else if (isStringEqual(encoder,
                           "ate")) {  // platform specific, no sources
        textureEncoder = kTexEncoderATE;
    }
    else if (isStringEqual(encoder,
                           "astcenc")) {  // platform specific, no sources
        textureEncoder = kTexEncoderAstcenc;
    }

    return textureEncoder;
}

static MyMTLPixelFormat parseFormat(ImageInfoArgs& infoArgs)
{
    MyMTLPixelFormat format = MyMTLPixelFormatInvalid;
    const char* formatString = infoArgs.formatString.c_str();

    // bc
    if (isStringEqual(formatString, "bc1")) {
        format = infoArgs.isSRGB ? MyMTLPixelFormatBC1_RGBA_sRGB : MyMTLPixelFormatBC1_RGBA;
    }
    //    else if (isStringEqual(formatString, "bc2")) {
    //        format = MyMTLPixelFormatBC2_RGBA;
    //    }
    else if (isStringEqual(formatString, "bc3")) {
        format = infoArgs.isSRGB ? MyMTLPixelFormatBC3_RGBA_sRGB : MyMTLPixelFormatBC3_RGBA;
    }
    else if (isStringEqual(formatString, "bc4")) {
        format = infoArgs.isSigned ? MyMTLPixelFormatBC4_RSnorm : MyMTLPixelFormatBC4_RUnorm;
    }
    else if (isStringEqual(formatString, "bc5")) {
        format = infoArgs.isSigned ? MyMTLPixelFormatBC5_RGSnorm : MyMTLPixelFormatBC5_RGUnorm;
    }
    else if (isStringEqual(formatString, "bc6")) {
        format = infoArgs.isSigned ? MyMTLPixelFormatBC6H_RGBFloat : MyMTLPixelFormatBC6H_RGBUfloat;
    }
    else if (isStringEqual(formatString, "bc7")) {
        format = infoArgs.isSRGB ? MyMTLPixelFormatBC7_RGBAUnorm_sRGB : MyMTLPixelFormatBC7_RGBAUnorm;
    }

    // etc2
    else if (isStringEqual(formatString, "etc2r")) {
        format = infoArgs.isSigned ? MyMTLPixelFormatEAC_R11Snorm : MyMTLPixelFormatEAC_R11Unorm;
    }
    else if (isStringEqual(formatString, "etc2rg")) {
        format = infoArgs.isSigned ? MyMTLPixelFormatEAC_RG11Snorm : MyMTLPixelFormatEAC_RG11Unorm;
    }
    else if (isStringEqual(formatString, "etc2rgb")) {
        format = infoArgs.isSRGB ? MyMTLPixelFormatETC2_RGB8_sRGB : MyMTLPixelFormatETC2_RGB8;
    }
    else if (isStringEqual(formatString, "etc2rgba")) {  // for rgb/rgba
        format = infoArgs.isSRGB ? MyMTLPixelFormatEAC_RGBA8_sRGB : MyMTLPixelFormatEAC_RGBA8;
    }

    // astc is always 4 channels, but can be swizzled to LLL1, LLLA, RG0, RGB1,
    // or RGBA to save endpoint storage dual plane can occur for more than just
    // RGB+A, any one channel can be a plane to itself if encoder supports
    else if (isStringEqual(formatString, "astc4x4")) {
        format = infoArgs.isHDR ? MyMTLPixelFormatASTC_4x4_HDR : infoArgs.isSRGB ? MyMTLPixelFormatASTC_4x4_sRGB : MyMTLPixelFormatASTC_4x4_LDR;
    }
    else if (isStringEqual(formatString, "astc5x5")) {
        format = infoArgs.isHDR ? MyMTLPixelFormatASTC_5x5_HDR : infoArgs.isSRGB ? MyMTLPixelFormatASTC_5x5_sRGB : MyMTLPixelFormatASTC_5x5_LDR;
    }
    else if (isStringEqual(formatString, "astc6x6")) {
        format = infoArgs.isHDR ? MyMTLPixelFormatASTC_5x5_HDR : infoArgs.isSRGB ? MyMTLPixelFormatASTC_5x5_sRGB : MyMTLPixelFormatASTC_5x5_LDR;
    }
    else if (isStringEqual(formatString, "astc8x8")) {
        format = infoArgs.isHDR ? MyMTLPixelFormatASTC_8x8_HDR : infoArgs.isSRGB ? MyMTLPixelFormatASTC_8x8_sRGB : MyMTLPixelFormatASTC_8x8_LDR;
    }

    // explicit formats
    else if (isStringEqual(formatString, "r8")) {
        format = MyMTLPixelFormatR8Unorm;
    }
    else if (isStringEqual(formatString, "rg8")) {
        format = MyMTLPixelFormatRG8Unorm;
    }
    else if (isStringEqual(formatString, "rgba8")) {  // for rgb/rgba
        format = infoArgs.isSRGB ? MTLPixelFormatRGBA8Unorm_sRGB : MyMTLPixelFormatRGBA8Unorm;
    }

    else if (isStringEqual(formatString, "r16f")) {
        format = MyMTLPixelFormatR16Float;
    }
    else if (isStringEqual(formatString, "rg16f")) {
        format = MyMTLPixelFormatRG16Float;
    }
    else if (isStringEqual(formatString, "rgba16f")) {  // for rgb/rgba
        format = MyMTLPixelFormatRGBA16Float;
    }

    else if (isStringEqual(formatString, "r32f")) {
        format = MyMTLPixelFormatR32Float;
    }
    else if (isStringEqual(formatString, "rg32f")) {
        format = MyMTLPixelFormatRG32Float;
    }
    else if (isStringEqual(formatString, "rgba32f")) {  // for rgb/rgba
        format = MyMTLPixelFormatRGBA32Float;
    }

    return format;
}

struct SwizzleIndex {
    int index[4];
};

int toSwizzleIndex(char swizzle, int xChannel)
{
    int index = -16;
    switch (swizzle) {
        case 'r':
            index = 0;
            break;
        case 'g':
            index = 1;
            break;
        case 'b':
            index = 2;
            break;
        case 'a':
            index = 3;
            break;
        case 'x':
            index = xChannel;
            break;

        // negative index is a constant value
        case '0':
            index = -2;
            break;
        case '1':
            index = -1;
            break;
    }
    // TODO: lum, minus1
    return index;
}

SwizzleIndex toSwizzleIndex(const char* swizzleText)
{
    SwizzleIndex swizzle;
    swizzle.index[0] = toSwizzleIndex(swizzleText[0], 0);
    swizzle.index[1] = toSwizzleIndex(swizzleText[1], 1);
    swizzle.index[2] = toSwizzleIndex(swizzleText[2], 2);
    swizzle.index[3] = toSwizzleIndex(swizzleText[3], 3);
    return swizzle;
}

bool isSwizzleValid(const char* swizzleText)
{
    if (strlen(swizzleText) != 4) {
        return false;
    }
    SwizzleIndex swizzle = toSwizzleIndex(swizzleText);
    for (int i = 0; i < 4; ++i) {
        if (swizzle.index[i] == -16) {
            return false;
        }
    }
    return true;
}

bool isChannelValid(const char* channelText)
{
    if (strlen(channelText) != 4) {
        return false;
    }
    // rxbx would be a valid channel mask, or xxbx
    for (int i = 0; i < 4; ++i) {
        int index = toSwizzleIndex(channelText[i], i);
        if (index != i) {
            return false;
        }
    }
    return true;
}

bool isEncoderAvailable(TexEncoder encoder)
{
    bool isValid = false;

    switch (encoder) {
#if COMPILE_ATE
        case kTexEncoderATE:
            isValid = true;
            break;
#endif
#if COMPILE_ASTCENC
        case kTexEncoderAstcenc:
            isValid = true;
            break;
#endif
#if COMPILE_BCENC
        case kTexEncoderBcenc:
            isValid = true;
            break;
#endif
#if COMPILE_ETCENC
        case kTexEncoderEtcenc:
            isValid = true;
            break;
#endif
#if COMPILE_SQUISH
        case kTexEncoderSquish:
            isValid = true;
            break;
#endif
        case kTexEncoderExplicit:
            isValid = true;
            break;

        // default in case above aren't compiled in
        default:
            isValid = false;
            break;
    }

    return isValid;
}

// encoding formats, many more decoding format
static const MyMTLPixelFormat kEncodingFormatsAstcenc[] =
    {
        MyMTLPixelFormatASTC_4x4_HDR,
        MyMTLPixelFormatASTC_4x4_LDR,
        MyMTLPixelFormatASTC_4x4_sRGB,

        MyMTLPixelFormatASTC_5x5_HDR,
        MyMTLPixelFormatASTC_5x5_LDR,
        MyMTLPixelFormatASTC_5x5_sRGB,

        MyMTLPixelFormatASTC_6x6_HDR,
        MyMTLPixelFormatASTC_6x6_LDR,
        MyMTLPixelFormatASTC_6x6_sRGB,

        MyMTLPixelFormatASTC_8x8_HDR,
        MyMTLPixelFormatASTC_8x8_LDR,
        MyMTLPixelFormatASTC_8x8_sRGB,
};

static const MyMTLPixelFormat kEncodingFormatsSquish[] =
    {
        MyMTLPixelFormatBC1_RGBA,
        MyMTLPixelFormatBC1_RGBA_sRGB,

        MyMTLPixelFormatBC3_RGBA,
        MyMTLPixelFormatBC3_RGBA_sRGB,

        MyMTLPixelFormatBC4_RSnorm,
        MyMTLPixelFormatBC4_RUnorm,

        MyMTLPixelFormatBC5_RGSnorm,
        MyMTLPixelFormatBC5_RGUnorm,
};

static const MyMTLPixelFormat kEncodingFormatsBcenc[] =
    {
        MyMTLPixelFormatBC1_RGBA,
        MyMTLPixelFormatBC1_RGBA_sRGB,

        MyMTLPixelFormatBC3_RGBA,
        MyMTLPixelFormatBC3_RGBA_sRGB,

        MyMTLPixelFormatBC4_RSnorm,
        MyMTLPixelFormatBC4_RUnorm,

        MyMTLPixelFormatBC5_RGSnorm,
        MyMTLPixelFormatBC5_RGUnorm,

        MyMTLPixelFormatBC7_RGBAUnorm,
        MyMTLPixelFormatBC7_RGBAUnorm_sRGB,
};

static const MyMTLPixelFormat kEncodingFormatsATEv1[] =
    {
        // astc support
        MyMTLPixelFormatASTC_4x4_LDR,
        MyMTLPixelFormatASTC_4x4_sRGB,

        MyMTLPixelFormatASTC_8x8_LDR,
        MyMTLPixelFormatASTC_8x8_sRGB,
};

static const MyMTLPixelFormat kEncodingFormatsATEv2[] =
    {
        // astc support
        MyMTLPixelFormatASTC_4x4_LDR,
        MyMTLPixelFormatASTC_4x4_sRGB,

        MyMTLPixelFormatASTC_8x8_LDR,
        MyMTLPixelFormatASTC_8x8_sRGB,

        // bc support
        MyMTLPixelFormatBC1_RGBA,
        MyMTLPixelFormatBC1_RGBA_sRGB,

        MyMTLPixelFormatBC4_RSnorm,
        MyMTLPixelFormatBC4_RUnorm,

        MyMTLPixelFormatBC5_RGSnorm,
        MyMTLPixelFormatBC5_RGUnorm,

        MyMTLPixelFormatBC7_RGBAUnorm,
        MyMTLPixelFormatBC7_RGBAUnorm_sRGB,
};

static const MyMTLPixelFormat kEncodingFormatsEtcenc[] =
    {
        // astc support
        MyMTLPixelFormatEAC_R11Unorm,
        MyMTLPixelFormatEAC_R11Snorm,

        MyMTLPixelFormatEAC_RG11Unorm,
        MyMTLPixelFormatEAC_RG11Snorm,

        MyMTLPixelFormatETC2_RGB8,
        MyMTLPixelFormatETC2_RGB8_sRGB,

        MyMTLPixelFormatEAC_RGBA8,
        MyMTLPixelFormatEAC_RGBA8_sRGB,
};

static const MyMTLPixelFormat kEncodingFormatsExplicit[] =
    {
        // astc support
        MyMTLPixelFormatR8Unorm,
        MyMTLPixelFormatRG8Unorm,
        MyMTLPixelFormatRGBA8Unorm,

        MTLPixelFormatRGBA8Unorm_sRGB,

        MyMTLPixelFormatR16Float,
        MyMTLPixelFormatRG16Float,
        MyMTLPixelFormatRGBA16Float,

        MyMTLPixelFormatR32Float,
        MyMTLPixelFormatRG32Float,
        MyMTLPixelFormatRGBA32Float,
};

// better countof in C++11, https://www.g-truc.net/post-0708.html
template <typename T, size_t N>
constexpr size_t countof(T const (&)[N]) noexcept
{
    return N;
}

bool isSupportedFormat(TexEncoder encoder, MyMTLPixelFormat format)
{
    if (!isEncoderAvailable(encoder)) {
        return false;
    }

    const MyMTLPixelFormat* table = nullptr;
    int tableSize = 0;

    switch (encoder) {
        case kTexEncoderAstcenc:
            table = kEncodingFormatsAstcenc;
            tableSize = countof(kEncodingFormatsAstcenc);
            break;
        case kTexEncoderSquish:
            table = kEncodingFormatsSquish;
            tableSize = countof(kEncodingFormatsSquish);
            break;
        case kTexEncoderATE: {
            bool isBCSupported = false;
#if COMPILE_ATE
            ATEEncoder encoder;
            isBCSupported = encoder.isBCSupported();
#endif
            table = isBCSupported ? kEncodingFormatsATEv2 : kEncodingFormatsATEv1;
            tableSize = isBCSupported ? countof(kEncodingFormatsATEv2) : countof(kEncodingFormatsATEv1);
            break;
        }
        case kTexEncoderBcenc:
            table = kEncodingFormatsBcenc;
            tableSize = countof(kEncodingFormatsBcenc);
            break;
        case kTexEncoderEtcenc:
            table = kEncodingFormatsEtcenc;
            tableSize = countof(kEncodingFormatsEtcenc);
            break;
        case kTexEncoderExplicit:
            table = kEncodingFormatsExplicit;
            tableSize = countof(kEncodingFormatsExplicit);
            break;
        case kTexEncoderUnknown:
            break;
    }

    for (int i = 0; i < tableSize; ++i) {
        if (table[i] == format) {
            return true;
        }
    }

    return false;
}

bool validateFormatAndDecoder(MyMTLTextureType textureType, MyMTLPixelFormat format, TexEncoder& textureEncoder)
{
    bool error = false;

    if (format == MyMTLPixelFormatInvalid) {
        return false;
    }

    // TODO: support decode of more types
    if (textureType != MyMTLTextureType2D) {
        return false;
    }

    // TODO: for now this logic is same as for encode, but ATE can decode more formats than it encodes

    // enforce the encoder
    if (textureEncoder == kTexEncoderUnknown) {
        // for now pick best encoder for each format
        // note there is a priority to this ordering that may skip better encoders
        if (isBCFormat(format)) {
            // prefer BCEnc since it's supposed to be faster and highest quality, but also newest encoder
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderBcenc, format)) {
                textureEncoder = kTexEncoderBcenc;
            }

            // one of the oldest encoders, and easy to follow
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderSquish, format)) {
                textureEncoder = kTexEncoderSquish;
            }

            // macOS/iOS only, so put it last but optimized encoder
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderATE, format)) {
                textureEncoder = kTexEncoderATE;
            }
        }
        else if (isETCFormat(format)) {
            // optimized etc encoder, but can be the slowest due to vast permutation iteration
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderEtcenc, format)) {
                textureEncoder = kTexEncoderEtcenc;
            }
        }
        else if (isASTCFormat(format)) {
            // this encoder is getting faster
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderAstcenc, format)) {
                textureEncoder = kTexEncoderAstcenc;
            }

            // macOS/iOS only, didn't find this using dual-plane like astenc
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderATE, format)) {
                textureEncoder = kTexEncoderATE;
            }
        }
        else {
            // Explicit for 8/16f/32f
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderExplicit, format)) {
                textureEncoder = kTexEncoderExplicit;
            }
        }

        if (textureEncoder == kTexEncoderUnknown) {
            error = true;
        }
    }
    else {
        if (!isSupportedFormat(textureEncoder, format)) {
            error = true;
        }
    }

    return !error;
}

bool validateFormatAndEncoder(ImageInfoArgs& infoArgs)
{
    bool error = false;

    MyMTLPixelFormat format = parseFormat(infoArgs);
    if (format == MyMTLPixelFormatInvalid) {
        return false;
    }

    // check arguments
    // flag unsupported formats
    if (format == MyMTLPixelFormatBC6H_RGBUfloat ||
        format == MyMTLPixelFormatBC6H_RGBFloat) {
        KLOGE("ImageInfo", "bc6 not supported\n");
        error = true;
    }

    infoArgs.pixelFormat = format;

    // enforce the encoder
    TexEncoder textureEncoder = infoArgs.textureEncoder;
    if (textureEncoder == kTexEncoderUnknown) {
        // for now pick best encoder for each format
        // note there is a priority to this ordering that may skip better encoders
        if (isBCFormat(format)) {
            // prefer BCEnc since it's supposed to be faster and highest quality, but also newest encoder
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderBcenc, format)) {
                textureEncoder = kTexEncoderBcenc;
            }

            // one of the oldest encoders, and easy to follow
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderSquish, format)) {
                textureEncoder = kTexEncoderSquish;
            }

            // macOS/iOS only, so put it last but optimized encoder
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderATE, format)) {
                textureEncoder = kTexEncoderATE;
            }
        }
        else if (isETCFormat(format)) {
            // optimized etc encoder, but can be the slowest due to vast permutation iteration
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderEtcenc, format)) {
                textureEncoder = kTexEncoderEtcenc;
            }
        }
        else if (isASTCFormat(format)) {
            // this encoder is getting faster
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderAstcenc, format)) {
                textureEncoder = kTexEncoderAstcenc;
            }

            // macOS/iOS only, didn't find this using dual-plane like astenc
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderATE, format)) {
                textureEncoder = kTexEncoderATE;
            }
        }
        else {
            // Explicit for 8/16f/32f
            if (textureEncoder == kTexEncoderUnknown && isSupportedFormat(kTexEncoderExplicit, format)) {
                textureEncoder = kTexEncoderExplicit;
            }
        }

        if (textureEncoder == kTexEncoderUnknown) {
            error = true;
        }
        else {
            infoArgs.textureEncoder = textureEncoder;
        }
    }
    else {
        if (!isSupportedFormat(textureEncoder, format)) {
            error = true;
        }
    }

    return !error;
}

bool validateTextureType(MyMTLTextureType textureType, int& w, int& h,
                         vector<Int2>& chunkOffsets, KTXHeader& header,
                         bool& doMipmaps)
{
    if (textureType == MyMTLTextureTypeCube) {
        header.numberOfFaces = 6;

        // validate
        if (w > h) {
            // horizontal strip
            if ((w != 6 * h) || !isPow2(w)) {
                return false;
            }
            w = h;

            for (int i = 0; i < (int)header.numberOfFaces; ++i) {
                Int2 chunkOffset = {w * i, 0};
                chunkOffsets.push_back(chunkOffset);
            }
        }
        else {
            // vertical strip
            if ((h != 6 * w) || !isPow2(w)) {
                return false;
            }
            h = w;

            for (int i = 0; i < (int)header.numberOfFaces; ++i) {
                Int2 chunkOffset = {0, h * i};
                chunkOffsets.push_back(chunkOffset);
            }
        }
    }
    else if (textureType == MyMTLTextureTypeCubeArray) {
        header.numberOfFaces = 6;

        // copy mips into each face - will have 6 * arrayCount
        // validate
        if (w > h) {
            // horizontal strip
            header.numberOfArrayElements = w / (6 * h);

            if ((w != (int)(6 * h * header.numberOfArrayElements)) ||
                !isPow2(h)) {
                return false;
            }
            w = h;

            for (int i = 0;
                 i < (int)(header.numberOfFaces * header.numberOfArrayElements);
                 ++i) {
                Int2 chunkOffset = {w * i, 0};
                chunkOffsets.push_back(chunkOffset);
            }
        }
        else {
            // vertical strip
            header.numberOfArrayElements = h / (6 * w);

            if ((h != (int)(6 * w * header.numberOfArrayElements)) ||
                !isPow2(w)) {
                return false;
            }
            h = w;

            for (int i = 0;
                 i < (int)(header.numberOfFaces * header.numberOfArrayElements);
                 ++i) {
                Int2 chunkOffset = {0, h * i};
                chunkOffsets.push_back(chunkOffset);
            }
        }
    }
    else if (textureType == MyMTLTextureType3D) {
        if (w > h) {
            header.pixelDepth = w / h;
            w = w / h;

            for (int i = 0; i < (int)header.pixelDepth; ++i) {
                Int2 chunkOffset = {w * i, 0};
                chunkOffsets.push_back(chunkOffset);
            }
        }
        else {
            header.pixelDepth = h / w;
            h = h / w;

            for (int i = 0; i < (int)header.pixelDepth; ++i) {
                Int2 chunkOffset = {0, h * i};
                chunkOffsets.push_back(chunkOffset);
            }
        }

        // disallow mips for now, all dims drop in half, and mip gen above won't
        // match that
        doMipmaps = false;

        // copy wxhxd
    }
    else if (textureType == MyMTLTextureType1DArray) {
        // convert 2d tex type to 2d, note mips odd on this one as well (1d mip)
        // disallow mips on these (gradients), but typically these are LUT
        doMipmaps = false;
        header.numberOfArrayElements = h;
        h = 1;

        // all strips are horizontal, and array goes vertical
        for (int i = 0; i < (int)header.numberOfArrayElements; ++i) {
            Int2 chunkOffset = {0, h * i};
            chunkOffsets.push_back(chunkOffset);
        }
    }
    else if (textureType == MyMTLTextureType2DArray) {
        if (w > h) {
            // horizontal strip
            header.numberOfArrayElements = w / h;
            if (w != (int)(h * header.numberOfArrayElements)) {
                return false;
            }

            w = h;
            for (int i = 0; i < (int)header.numberOfArrayElements; ++i) {
                Int2 chunkOffset = {w * i, 0};
                chunkOffsets.push_back(chunkOffset);
            }
        }
        else {
            header.numberOfArrayElements = h / w;
            if (h != (int)(w * header.numberOfArrayElements)) {
                return false;
            }

            h = w;
            for (int i = 0; i < (int)header.numberOfArrayElements; ++i) {
                Int2 chunkOffset = {0, h * i};
                chunkOffsets.push_back(chunkOffset);
            }
        }
    }
    else if (textureType == MyMTLTextureType2D) {
        Int2 chunkOffset = {0, 0};
        chunkOffsets.push_back(chunkOffset);
    }

    return true;
}

// const member function, but it can change the srcPixels.
void ImageInfo::swizzleTextureHDR(int w, int h, float4* srcPixelsFloat_,
                                  const char* swizzleText)
{
    // set any channels that are constant
    SwizzleIndex swizzle = toSwizzleIndex(swizzleText);

    // this is a noop
    if (swizzle.index[0] == 0 && swizzle.index[1] == 1 && swizzle.index[2] == 2 && swizzle.index[3] == 3) {
        return;
    }

    float4 c = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        if (swizzle.index[i] == -1) {
            c[i] = 1.0f;
        }
    }

    float4* srcPixelsFloat = (float4*)srcPixelsFloat_;
    for (int y = 0; y < h; ++y) {
        int y0 = y * w;

        for (int x = 0; x < w; ++x) {
            float4& c0 = srcPixelsFloat[y0 + x];
            const float4& ci = c0;

            // reorder, then writeback
            // this doesn't copy over constants set outside loop
            if (swizzle.index[0] >= 0) {
                c.x = ci[swizzle.index[0]];
            }
            if (swizzle.index[1] >= 0) {
                c.y = ci[swizzle.index[1]];
            }
            if (swizzle.index[2] >= 0) {
                c.z = ci[swizzle.index[2]];
            }
            if (swizzle.index[3] >= 0) {
                c.w = ci[swizzle.index[3]];
            }

            c0 = c;
        }
    }
}

void ImageInfo::swizzleTextureLDR(int w, int h, Color* srcPixels_,
                                  const char* swizzleText_)
{
    // set any channels that are constant
    SwizzleIndex swizzle = toSwizzleIndex(swizzleText_);

    // this is a noop
    if (swizzle.index[0] == 0 && swizzle.index[1] == 1 && swizzle.index[2] == 2 && swizzle.index[3] == 3) {
        return;
    }

    // fixup any negative swizzle indices (constant values)
    Color c = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        if (swizzle.index[i] == -1) {
            (&c.r)[i] = 255;
        }
    }

    Color* srcPixels = (Color*)srcPixels_;
    for (int y = 0; y < h; ++y) {
        int y0 = y * w;

        for (int x = 0; x < w; ++x) {
            Color& c0 = srcPixels[y0 + x];
            const uint8_t* ci = &c0.r;

            // reorder, then writeback
            // this doesn't copy over constants set outside loop
            if (swizzle.index[0] >= 0) {
                c.r = ci[swizzle.index[0]];
            }
            if (swizzle.index[1] >= 0) {
                c.g = ci[swizzle.index[1]];
            }
            if (swizzle.index[2] >= 0) {
                c.b = ci[swizzle.index[2]];
            }
            if (swizzle.index[3] >= 0) {
                c.a = ci[swizzle.index[3]];
            }

            c0 = c;
        }
    }
}

//-------------------------

void ImageInfo::updateImageTraitsHDR(int w, int h, const float4* srcPixels)
{
    if (srcPixels == nullptr) {
        return;
    }

    // validate that image hasColor and isn't grayscale data
    if (hasColor) {
        hasColor = false;

        // stop on first color pixel
        for (int y = 0; y < h && !hasColor; ++y) {
            int y0 = y * w;

            for (int x = 0; x < w; ++x) {
                const float4& c0 = srcPixels[y0 + x];

                if (c0.x != c0.y || c0.x != c0.z) {
                    hasColor = true;
                    break;
                }
            }
        }
    }

    // validate that image truly has alpha
    if (hasAlpha) {
        hasAlpha = false;

        // stop on first non 255 alpha
        for (int y = 0; y < h && !hasAlpha; ++y) {
            int y0 = y * w;

            for (int x = 0; x < w; ++x) {
                const float4& c0 = srcPixels[y0 + x];
                if (c0.w != 1.0f) {
                    hasAlpha = true;
                    break;
                }
            }
        }
    }
}

void ImageInfo::updateImageTraitsLDR(int w, int h, const Color* srcPixels)
{
    if (srcPixels == nullptr) {
        return;
    }

    // validate that image hasColor and isn't grayscale data
    if (hasColor) {
        hasColor = false;

        // stop on first color pixel
        for (int y = 0; y < h && !hasColor; ++y) {
            int y0 = y * w;

            for (int x = 0; x < w; ++x) {
                const Color& c0 = srcPixels[y0 + x];

                if (c0.r != c0.g || c0.r != c0.b) {
                    hasColor = true;
                    break;
                }
            }
        }
    }

    // validate that image truly has alpha
    if (hasAlpha) {
        hasAlpha = false;

        // stop on first non 255 alpha
        for (int y = 0; y < h && !hasAlpha; ++y) {
            int y0 = y * w;

            for (int x = 0; x < w; ++x) {
                const Color& c0 = srcPixels[y0 + x];
                if (c0.a != 255) {
                    hasAlpha = true;
                    break;
                }
            }
        }
    }
}

void ImageInfo::initWithArgs(const ImageInfoArgs& args)
{
    textureEncoder = args.textureEncoder;
    textureType = args.textureType;

    isPremultiplied = args.isPremultiplied;
    isNormal = args.isNormal;

    doSDF = args.doSDF;
    skipImageLength = args.skipImageLength;

    // mips
    doMipmaps = args.doMipmaps;
    mipMinSize = args.mipMinSize;
    mipMaxSize = args.mipMaxSize;

    swizzleText = args.swizzleText;
    averageChannels = args.averageChannels;

    isVerbose = args.isVerbose;

    quality = args.quality;

    // Note: difference between input srgb and output srgb, but it's mingled
    // here a bit

    useATE = textureEncoder == kTexEncoderATE;
    useSquish = textureEncoder == kTexEncoderSquish;
    useBcenc = textureEncoder == kTexEncoderBcenc;
    useAstcenc = textureEncoder == kTexEncoderAstcenc;
    useEtcenc = textureEncoder == kTexEncoderEtcenc;
    useExplicit = textureEncoder == kTexEncoderExplicit;

    pixelFormat = args.pixelFormat;

    // can go ahead and change format here, but have to run again
    // if walking pixels sets hasAlpha to false
    optimizeFormatForOpaque = args.optimizeFormatForOpaque;

    isASTC = isASTCFormat(pixelFormat);
    isBC = isBCFormat(pixelFormat);
    isETC = isETCFormat(pixelFormat);
    isExplicit = !(isASTC || isBC || isETC);

    isSigned = isSignedFormat(pixelFormat);

    isSRGB = isSrgbFormat(pixelFormat);

    hasAlpha = true;
    hasColor = true;
    if (!isAlphaFormat(pixelFormat))
        hasAlpha = false;
    if (!isColorFormat(pixelFormat))
        hasColor = false;
}

void ImageInfo::optimizeFormat()
{
    if (optimizeFormatForOpaque && !hasAlpha) {
        if (pixelFormat == MyMTLPixelFormatBC3_RGBA_sRGB) {
            pixelFormat = MyMTLPixelFormatBC1_RGBA_sRGB;
        }
        else if (pixelFormat == MyMTLPixelFormatBC3_RGBA) {
            pixelFormat = MyMTLPixelFormatBC1_RGBA;
        }

        if (pixelFormat == MyMTLPixelFormatBC7_RGBAUnorm_sRGB) {
            pixelFormat = MyMTLPixelFormatBC1_RGBA_sRGB;
        }
        else if (pixelFormat == MyMTLPixelFormatBC7_RGBAUnorm) {
            pixelFormat = MyMTLPixelFormatBC1_RGBA;
        }

        else if (pixelFormat == MyMTLPixelFormatEAC_RGBA8) {
            pixelFormat = MyMTLPixelFormatETC2_RGB8;
        }
        else if (pixelFormat == MyMTLPixelFormatEAC_RGBA8_sRGB) {
            pixelFormat = MyMTLPixelFormatETC2_RGB8_sRGB;
        }
    }
}

void ImageInfo::initWithSourceImage(Image& sourceImage)
{
    // can only determine this after reading in the source texture
    int w = sourceImage.width();
    int h = sourceImage.height();
    Color* srcPixels = (Color*)sourceImage.pixels();
    float4* srcPixelsFloat = (float4*)sourceImage.pixelsFloat();

    isHDR = srcPixelsFloat != nullptr;

    // these come from png header, but hasn't walked pixels yet
    if (!sourceImage.hasAlpha()) {
        hasAlpha = false;
    }
    if (!sourceImage.hasColor()) {
        hasColor = false;
    }

    // this updates hasColor/hasAlpha
    if (!swizzleText.empty()) {
        // set any channels that are constant
        SwizzleIndex swizzle = toSwizzleIndex(swizzleText.c_str());

        // grayscale, or alpha 1 is no alpha
        // can avoid testing pixels for these
        if (swizzle.index[0] == swizzle.index[1] == swizzle.index[2]) {
            hasColor = false;
        }
        if (swizzle.index[3] == -1) {
            hasAlpha = false;
        }

        if (isHDR) {
            swizzleTextureHDR(w, h, srcPixelsFloat, swizzleText.c_str());
        }
        else {
            swizzleTextureLDR(w, h, srcPixels, swizzleText.c_str());
        }
    }

    // this updates hasColor/hasAlpha by walking pixels
    if (isHDR) {
        updateImageTraitsHDR(w, h, srcPixelsFloat);
    }
    else {
        updateImageTraitsLDR(w, h, srcPixels);
    }

    // this cuts storage of ETC2rgba and BC3/BC7 images to half size if hasAlpha is false
    optimizeFormat();

    // this implies color is stored in rgb
    if (isSRGB) {
        isColorWeighted = hasColor;
    }

    // don't allow per block averaging on hdr data yet
    // it's just a straight copy for explicit,
    // TODO: going to BC6 or ASTC-HDR this might be useful, but need to average
    // floats not 8u
    if (isHDR && !averageChannels.empty()) {
        averageChannels.clear();
    }

    // This is a per block averaging done in mipgen, may only make sense for
    // compressed foramts and not the explicit formats.  Averaging only done on
    // LDR and not sRGB data.  This is to pack more data into normal 4 channel
    // textures without throwing off xy reconstruct.
    if (!averageChannels.empty()) {
        bool isValid = false;

        // averaging all the values in 8-bit space, so only apply to lin. rgbs
        switch (pixelFormat) {
            case MyMTLPixelFormatETC2_RGB8:  // 3 channel
            case MyMTLPixelFormatEAC_RGBA8:  // 4 channel

            case MyMTLPixelFormatASTC_4x4_LDR:
            case MyMTLPixelFormatASTC_5x5_LDR:
            case MyMTLPixelFormatASTC_6x6_LDR:
            case MyMTLPixelFormatASTC_8x8_LDR:

            case MyMTLPixelFormatBC1_RGBA:  // 3 channel RGB only
            case MyMTLPixelFormatBC3_RGBA:
            // case MyMTLPixelFormatBC6H_RGBFloat:
            // case MyMTLPixelFormatBC6H_RGBUfloat:
            case MyMTLPixelFormatBC7_RGBAUnorm:
                isValid = true;
                break;

            default:
                break;
        }

        if (!isValid) {
            KLOGE("ImageInfo", "Averaging only works on specific formats\n");
            averageChannels.clear();
        }
    }
}

}  // namespace kram
