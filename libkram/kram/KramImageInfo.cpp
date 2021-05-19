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
using namespace simd;

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
        format = infoArgs.isHDR ? MyMTLPixelFormatASTC_6x6_HDR : infoArgs.isSRGB ? MyMTLPixelFormatASTC_6x6_sRGB : MyMTLPixelFormatASTC_6x6_LDR;
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
        format = infoArgs.isSRGB ? MyMTLPixelFormatRGBA8Unorm_sRGB : MyMTLPixelFormatRGBA8Unorm;
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
    int32_t index[4];
};

int32_t toSwizzleIndex(char swizzle, int32_t xChannel)
{
    int32_t index = -16;
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
    for (int32_t i = 0; i < 4; ++i) {
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
    for (int32_t i = 0; i < 4; ++i) {
        int32_t index = toSwizzleIndex(channelText[i], i);
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
        // Note: This is causing pink constant block artifacts in Toof-a,
        // -optopaque case was hitting this, where BC7 reduced to BC1.
        // Squish BC1 doesn't look much better. BC7 looks perfect on that file.
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

        MyMTLPixelFormatRGBA8Unorm_sRGB,

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
    int32_t tableSize = 0;

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

    for (int32_t i = 0; i < tableSize; ++i) {
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

    // caller an set or this can parse format from the format text
    MyMTLPixelFormat format = infoArgs.pixelFormat;
    if (format == MyMTLPixelFormatInvalid) {
        format = parseFormat(infoArgs);
    }
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

bool validateTextureType(MyMTLTextureType textureType, int32_t& w, int32_t& h,
                         vector<Int2>& chunkOffsets, KTXHeader& header,
                         bool& doMipmaps,
                         int32_t chunksX, int32_t chunksY, int32_t chunksCount)
{
    // TODO: add error messages to false cases to help with generating
    // TODO: support non-pow2 cubemaps and cubemap arrays?
    // TODO: tie chunks into 3d, cubearray so don't have to use long strips

    if (textureType == MyMTLTextureTypeCube) {
        header.numberOfFaces = 6;

        // validate
        if (w > h) {
            // horizontal strip
            if ((w != 6 * h) || !isPow2(h)) {
                return false;
            }
            w = h;

            for (int32_t i = 0; i < (int32_t)header.numberOfFaces; ++i) {
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

            for (int32_t i = 0; i < (int32_t)header.numberOfFaces; ++i) {
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

            if ((w != (int32_t)(6 * h * header.numberOfArrayElements)) ||
                !isPow2(h)) {
                return false;
            }
            w = h;

            for (int32_t i = 0;
                 i < (int32_t)(header.numberOfFaces * header.numberOfArrayElements);
                 ++i) {
                Int2 chunkOffset = {w * i, 0};
                chunkOffsets.push_back(chunkOffset);
            }
        }
        else {
            // vertical strip
            header.numberOfArrayElements = h / (6 * w);

            if ((h != (int32_t)(6 * w * header.numberOfArrayElements)) ||
                !isPow2(w)) {
                return false;
            }
            h = w;

            for (int32_t i = 0;
                 i < (int32_t)(header.numberOfFaces * header.numberOfArrayElements);
                 ++i) {
                Int2 chunkOffset = {0, h * i};
                chunkOffsets.push_back(chunkOffset);
            }
        }
    }
    else if (textureType == MyMTLTextureType3D) {
        if (w > h) {
            int32_t numSlices = w / h;
            header.pixelDepth = numSlices;
            if (w != (int32_t)(h * numSlices)) {
                return false;
            }
            w = h;  // assume square

            for (int32_t i = 0; i < (int32_t)numSlices; ++i) {
                Int2 chunkOffset = {w * i, 0};
                chunkOffsets.push_back(chunkOffset);
            }
        }
        else {
            int32_t numSlices = h / w;
            header.pixelDepth = numSlices;
            if (h != (int32_t)(w * numSlices)) {
                return false;
            }
            h = w;  // assume square

            for (int32_t i = 0; i < (int32_t)numSlices; ++i) {
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
        for (int32_t i = 0; i < (int32_t)header.numberOfArrayElements; ++i) {
            Int2 chunkOffset = {0, h * i};
            chunkOffsets.push_back(chunkOffset);
        }
    }
    else if (textureType == MyMTLTextureType2DArray) {
        
        if (chunksCount > 0) {
            w = w / chunksX;
            h = h / chunksY;
            
            // this can be smaller than chunksX * chunkY, but assume chunks packed up and to left
            header.numberOfArrayElements = chunksCount;
            
            int32_t x = 0;
            int32_t y = 0;
            for (int32_t i = 0; i < (int32_t)header.numberOfArrayElements; ++i) {
                if ((i > 0) && ((i % chunksX) == 0)) {
                    y++;
                    x = 0;
                }
                
                Int2 chunkOffset = {w * x, h * y};
                chunkOffsets.push_back(chunkOffset);
                
                x++;
            }
        }
        else {
            if (w > h) {
                // horizontal strip
                header.numberOfArrayElements = w / h;
                if (w != (int32_t)(h * header.numberOfArrayElements)) {
                    return false;
                }

                w = h;  // assume square
                for (int32_t i = 0; i < (int32_t)header.numberOfArrayElements; ++i) {
                    Int2 chunkOffset = {w * i, 0};
                    chunkOffsets.push_back(chunkOffset);
                }
            }
            else {
                header.numberOfArrayElements = h / w;
                if (h != (int32_t)(w * header.numberOfArrayElements)) {
                    return false;
                }

                h = w;  // assume square
                for (int32_t i = 0; i < (int32_t)header.numberOfArrayElements; ++i) {
                    Int2 chunkOffset = {0, h * i};
                    chunkOffsets.push_back(chunkOffset);
                }
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
void ImageInfo::swizzleTextureHDR(int32_t w, int32_t h, float4* srcPixelsFloat_,
                                  const char* swizzleText)
{
    // set any channels that are constant
    SwizzleIndex swizzle = toSwizzleIndex(swizzleText);

    // this is a noop
    if (swizzle.index[0] == 0 && swizzle.index[1] == 1 && swizzle.index[2] == 2 && swizzle.index[3] == 3) {
        return;
    }

    float4 c = {0, 0, 0, 0};
    for (int32_t i = 0; i < 4; ++i) {
        if (swizzle.index[i] == -1) {
            c[i] = 1.0f;
        }
    }

    float4* srcPixelsFloat = (float4*)srcPixelsFloat_;
    for (int32_t y = 0; y < h; ++y) {
        int32_t y0 = y * w;

        for (int32_t x = 0; x < w; ++x) {
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

void ImageInfo::swizzleTextureLDR(int32_t w, int32_t h, Color* srcPixels_,
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
    for (int32_t i = 0; i < 4; ++i) {
        if (swizzle.index[i] == -1) {
            (&c.r)[i] = 255;
        }
    }

    Color* srcPixels = (Color*)srcPixels_;
    for (int32_t y = 0; y < h; ++y) {
        int32_t y0 = y * w;

        for (int32_t x = 0; x < w; ++x) {
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

void ImageInfo::updateImageTraitsHDR(int32_t w, int32_t h, const float4* srcPixels)
{
    if (srcPixels == nullptr) {
        return;
    }

    // validate that image hasColor and isn't grayscale data
    if (hasColor) {
        hasColor = false;

        // stop on first color pixel
        for (int32_t y = 0; y < h && !hasColor; ++y) {
            int32_t y0 = y * w;

            for (int32_t x = 0; x < w; ++x) {
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
        for (int32_t y = 0; y < h && !hasAlpha; ++y) {
            int32_t y0 = y * w;

            for (int32_t x = 0; x < w; ++x) {
                const float4& c0 = srcPixels[y0 + x];
                if (c0.w != 1.0f) {
                    hasAlpha = true;
                    break;
                }
            }
        }
    }
}

void ImageInfo::updateImageTraitsLDR(int32_t w, int32_t h, const Color* srcPixels)
{
    if (srcPixels == nullptr) {
        return;
    }

    // validate that image hasColor and isn't grayscale data
    if (hasColor) {
        hasColor = false;

        // stop on first color pixel
        for (int32_t y = 0; y < h && !hasColor; ++y) {
            int32_t y0 = y * w;

            for (int32_t x = 0; x < w; ++x) {
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
        for (int32_t y = 0; y < h && !hasAlpha; ++y) {
            int32_t y0 = y * w;

            for (int32_t x = 0; x < w; ++x) {
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

    isKTX2 = args.isKTX2;
    compressor = args.compressor;
    
    isPrezero = args.isPrezero;
    isPremultiplied = args.isPremultiplied;
    if (isPremultiplied)
        isPrezero = false;
    
    isNormal = args.isNormal;

    doSDF = args.doSDF;
    //skipImageLength = args.skipImageLength;

    // mips
    doMipmaps = args.doMipmaps;
    mipMinSize = args.mipMinSize;
    mipMaxSize = args.mipMaxSize;
    mipSkip    = args.mipSkip;
    
    swizzleText = args.swizzleText;
    averageChannels = args.averageChannels;

    isVerbose = args.isVerbose;

    quality = args.quality;

    // this is for height to normal, will convert .r to normal xy
    isHeight = args.isHeight;
    isWrap = args.isWrap;
    heightScale = args.heightScale;
    if (isHeight)
        isNormal = true;
    
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
    
    // copy over chunks
    chunksX = args.chunksX;
    chunksY = args.chunksY;
    chunksCount = args.chunksCount;
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

        else if (pixelFormat == MyMTLPixelFormatBC7_RGBAUnorm_sRGB) {
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
    int32_t w = sourceImage.width();
    int32_t h = sourceImage.height();
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

    // this will only work on 2d textures, since this is all pre-chunk
    if (isHeight) {
        heightToNormals(w, h, srcPixelsFloat, srcPixels, heightScale, isWrap);
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
    // But BC1 565 and 2-bit endpoints are no match for BC7, and bc7enc's BC1 is introducing artifacts into Toof-a.
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



// TODO: tread 16u png into pixelsFlat, then gen an 8-bit normal xy
// from that.  This is more like SDF where a single height is used.

void ImageInfo::heightToNormals(int32_t w, int32_t h,
                            float4* srcPixels,
                            Color* srcPixels8,
                            float scale, bool isWrap)
{
    // see here
    // https://developer.download.nvidia.com/CgTutorial/cg_tutorial_chapter08.html

    // src/dst the same here
    // may need to copy a row/column of pixels for wrap
    float4* dstPixels = srcPixels;
    Color* dstPixels8 = srcPixels8;
    
    bool isFloat = srcPixels;
    
    // copy the texture, or there are too many edge cases in the code below
    vector<Color> srcDataCopy8;
    vector<float4> srcDataCopy;
    if (isFloat) {
        srcDataCopy.resize(w*h);
        memcpy(srcDataCopy.data(), srcPixels, vsizeof(srcDataCopy));
        srcPixels = srcDataCopy.data();
    }
    else {
        srcDataCopy8.resize(w*h);
        memcpy(srcDataCopy8.data(), srcPixels8, vsizeof(srcDataCopy8));
        srcPixels8 = srcDataCopy8.data();
    }
    
    //-----------------------
    
    bool isWrapX = isWrap;
    bool isWrapY = isWrap;
    
    // 2.0 is distance betwen +1 and -1
    // don't scale by this, want caller to be able to pass 1.0 as default scale not 2.0
    float scaleX = scale; // / 2.0;
    float scaleY = scale; // / 2.0;

    if (!isFloat) {
        scaleX /= 255.0f;
        scaleY /= 255.0f;
    }
    
    // TODO: doing this at image level doesn't support chunk conversion
    // so this would only work for 2D images, and not atlas strips to a 2D array.
    
    // TODO: Larger kernel support to 2x2, 3x3, 5x5, 7x7, 9x9
    // This pattern is a 3x3 cross here only 4 cardinal samples are used.
    // Bigger areas have bleed especially if this is run on a chart.
    
    // this recommends generating a few maps, and blending between them
    // https://vrayschool.com/normal-map/
    
    for (int32_t y = 0; y < h; ++y) {
        int32_t y0 = y;
        int32_t ym = y - 1;
        int32_t yp = y + 1;

        if (isWrapY) {
            ym = (ym + h) % h;
            yp = (yp) % h;
        }
        else {
            // clamp
            if (ym < 0) ym = 0;
            if (yp > (h - 1)) yp = h - 1;
        }

        y0 *= w;
        ym *= w;
        yp *= w;

        for (int32_t x = 0; x < w; ++x) {
            //int32_t x0 = x;
            int32_t xm = x - 1;
            int32_t xp = x + 1;

            if (isWrapX) {
                xm = (xm + w) % w;
                xp = (xp) % w;
            }
            else {
                // clamp
                if (xm < 0) xm = 0;
                if (xp > (w - 1)) xp = w - 1;
            }

            
            if (isFloat) {
                
                // cross pattern
                // height channel is in x
                float cN = srcPixels[ym + x].x;
                float cS = srcPixels[yp + x].x;
                float cE = srcPixels[y0 + xp].x;
                float cW = srcPixels[y0 + xm].x;

                // up is N, so this is rhcs
                float dx = (cE - cW) * scaleX;
                float dy = (cN - cS) * scaleY;
           
                float4 normal = float4m(dx, dy, 1.0f, 0.0f);
                normal = normalize(normal);
                
                // convert to unorm
                normal = normal * 0.5 + 0.5f;
                
                // write out the result
                float4& dstPixel = dstPixels[y0 + x];

                dstPixel.x = normal.x;
                dstPixel.y = normal.y;
                
                // TODO: consider storing in z, easier to see data channel, not premul
                // store height in alpha.  Let caller provide the swizzle xyzh01
                //dstPixel.z = normal.z; // can reconstruct from xy
                //dstPixel.w = srcPixels[y0 + x0].x;
                
                dstPixel.z = srcPixels[y0 + x].z;
                dstPixel.w = srcPixels[y0 + x].w;
            }
            else {
                // cross pattern
                // height channel is in x
                uint8_t cN = srcPixels8[ym + x].r; // assumes first elem (.r) is height channel
                uint8_t cS = srcPixels8[yp + x].r;
                uint8_t cE = srcPixels8[y0 + xp].r;
                uint8_t cW = srcPixels8[y0 + xm].r;

                float dx = (cE - cW) * scaleX;
                float dy = (cN - cS) * scaleY;
           
                dx = -dx;
                dy = -dy;
                
                float4 normal = float4m(dx, dy, 1.0f, 0.0f);
                normal = normalize(normal);
                
                // convert to unorm
                normal = normal * 127.0f + 128.0f;
                
                Color& dstPixel8 = dstPixels8[y0 + x];

                dstPixel8.r = normal.x;
                dstPixel8.g = normal.y;
                
                // TODO: consider storing height in z, easier to see data channel, not premul
                // store height in alpha.  Let caller provide the swizzle xyzh01
                //dstPixel8.b = normal.z; // can reconstruct from xy
                //dstPixel8.a = srcPixels8[y0 + x0].r;
                
                dstPixel8.b = srcPixels8[y0 + x].b;
                dstPixel8.a = srcPixels8[y0 + x].a;
            }
        }
    }
}


const char* encoderName(TexEncoder encoder)
{
    switch(encoder) {
        case kTexEncoderExplicit: return "Explicit";
        case kTexEncoderATE: return "ATE";
        case kTexEncoderSquish: return "Squish";
        case kTexEncoderBcenc: return "Bcenc";
        case kTexEncoderEtcenc: return "Etcenc";
        case kTexEncoderAstcenc: return "Astcenc";
        case kTexEncoderUnknown: return "Unknown";
        default: return "Unknown"; // to fix Visual Studio C4715
    }
}

}  // namespace kram
