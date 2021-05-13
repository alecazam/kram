// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "Kram.h"

#include <sys/stat.h>

#include <algorithm>  // for max
#include <atomic>
#include <cmath>
#include <ctime>
#include <inttypes.h>
#include <string>
#include <vector>

#include "KTXImage.h"
#include "KramFileHelper.h"
#include "KramImage.h"  // has config defines, move them out
#include "KramMmapHelper.h"
#include "KramTimer.h"
#include "KramVersion.h"
#include "TaskSystem.h"
#include "lodepng/lodepng.h"

//--------------------------------------

// name change on Win
#if KRAM_WIN
#define strtok_r strtok_s
#endif

namespace kram {

using namespace std;

bool LoadKtx(const uint8_t* data, size_t dataSize, Image& sourceImage)
{
    KTXImage image;
    if (!image.open(data, dataSize)) {
        return false;
    }

    // many different types of KTX files, for now only import from 2D type
    // and only pull the first mip, but want to be able to pull custom mips from
    // many types
    return sourceImage.loadImageFromKTX(image);
}

inline Color toPremul(Color c) {
    c.r = ((uint32_t)c.r * (uint32_t)c.a) / 255;
    c.g = ((uint32_t)c.g * (uint32_t)c.a) / 255;
    c.b = ((uint32_t)c.b * (uint32_t)c.a) / 255;
    return c;
}

bool LoadPng(const uint8_t* data, size_t dataSize, bool isPremulRgb, Image& sourceImage)
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t errorLode = 0;

    // can identify 16unorm data for heightmaps via this call
    LodePNGState state;
    lodepng_state_init(&state);

    errorLode = lodepng_inspect(&width, &height, &state, data, dataSize);
    if (errorLode != 0) {
        return false;
    }

    // don't convert png bit depths, but can convert pallete data
    //    if (state.info_png.color.bitdepth != 8) {
    //        return false;
    //    }

    bool hasColor = true;
    bool hasAlpha = true;

    switch (state.info_png.color.colortype) {
        case LCT_GREY:
        case LCT_GREY_ALPHA:
            hasColor = false;
            break;
        case LCT_MAX_OCTET_VALUE:
        case LCT_RGB:
        case LCT_RGBA:
        case LCT_PALETTE:  // ?
            hasColor = true;
            break;
        
            hasColor = false;
            break;;
    }

    switch (state.info_png.color.colortype) {
        case LCT_GREY:
        case LCT_RGB:
            hasAlpha = false;
            break;
        case LCT_MAX_OCTET_VALUE:
        case LCT_RGBA:
        case LCT_GREY_ALPHA:
        case LCT_PALETTE:  // ?
            hasAlpha = true;
            break;
    }

    // this inserts onto end of array, it doesn't resize
    vector<uint8_t> pixels;
    pixels.clear();
    errorLode = lodepng::decode(pixels, width, height, data, dataSize, LCT_RGBA, 8);
    if (errorLode != 0) {
        return false;
    }

    // apply premul srgb right away, don't use with -premul or alpha is applied twice
    // this may throw off the props.  Note this ignores srgb conversion.
    // This is hack to look like Photoshop and Apple Preview, where they process srgb wrong
    // on premul PNG data on load, and colors look much darker.
    
    if (hasAlpha && isPremulRgb) {
        Color* colors = (Color*)pixels.data();
        for (int32_t i = 0, iEnd = width*height; i < iEnd; ++i) {
            colors[i] = toPremul(colors[i]);
        }
    }
    
    return sourceImage.loadImageFromPixels(pixels, width, height, hasColor, hasAlpha);
}

bool SetupTmpFile(FileHelper& tmpFileHelper, const char* suffix)
{
    return tmpFileHelper.openTemporaryFile(suffix, "w+b");
}

bool SetupSourceImage(MmapHelper& mmapHelper, FileHelper& fileHelper,
                      vector<uint8_t>& fileBuffer,
                      const string& srcFilename, Image& sourceImage, bool isPremulSrgb = false)
{
    bool isKTX = endsWith(srcFilename, ".ktx") || endsWith(srcFilename, ".ktx2");
    bool isPNG = endsWith(srcFilename, ".png");

    if (!(isKTX || isPNG)) {
        KLOGE("Kram", "File input \"%s\" isn't a png, ktx, ktx2 file.\n",
              srcFilename.c_str());
        return false;
    }

    // first try mmap, and then use file -> buffer
    bool useMmap = true;
    if (!mmapHelper.open(srcFilename.c_str())) {
        // fallback to opening file if no mmap support or it didn't work
        useMmap = false;
    }

    if (useMmap) {
        if (isKTX) {  // really want endsWidth
            if (!LoadKtx(mmapHelper.data(), mmapHelper.dataLength(),
                         sourceImage)) {
                return false;  // error
            }
        }
        else if (isPNG) {
            if (!LoadPng(mmapHelper.data(), mmapHelper.dataLength(), isPremulSrgb,
                         sourceImage)) {
                return false;  // error
            }
        }
    }
    else {
        if (!fileHelper.open(srcFilename.c_str(), "rb")) {
            KLOGE("Kram", "File input \"%s\" could not be opened for read.\n",
                  srcFilename.c_str());
            return false;
        }

        // read entire png into memory
        size_t size = fileHelper.size();
        fileBuffer.resize(size);

        if (!fileHelper.read(fileBuffer.data(), size)) {
            return false;
        }

        if (isKTX) {
            if (!LoadKtx(fileBuffer.data(), fileBuffer.size(),
                         sourceImage)) {
                return false;  // error
            }
        }
        else if (isPNG) {
            if (!LoadPng(fileBuffer.data(), fileHelper.size(), isPremulSrgb,
                         sourceImage)) {
                return false;  // error
            }
        }
    }

    return true;
}

// decoding reads a ktx file into KTXImage (not Image)
bool SetupSourceKTX(MmapHelper& mmapHelper, FileHelper& fileHelper,
                    vector<uint8_t>& fileBuffer,
                    const string& srcFilename, KTXImage& sourceImage)
{
    // first try mmap, and then use file -> buffer
    bool useMmap = true;
    if (!mmapHelper.open(srcFilename.c_str())) {
        // fallback to file system if no mmap or failed
        useMmap = false;
    }

    if (useMmap) {
        if (!sourceImage.open(mmapHelper.data(), mmapHelper.dataLength())) {
            return false;
        }
    }
    else {
        if (!fileHelper.open(srcFilename.c_str(), "rb")) {
            KLOGE("Kram", "File input \"%s\" could not be opened for read.\n",
                  srcFilename.c_str());
            return false;
        }

        // read entire ktx into memory
        size_t size = fileHelper.size();
        fileBuffer.resize(size);
        if (!fileHelper.read(fileBuffer.data(), size)) {
            return false;
        }

        if (!sourceImage.open(fileBuffer.data(), (int32_t)fileBuffer.size())) {
            return false;
        }
    }

    return true;
}

// better countof in C++11, https://www.g-truc.net/post-0708.html
template <typename T, size_t N>
constexpr size_t countof(T const (&)[N]) noexcept
{
    return N;
}

#define isStringEqual(lhs, rhs) (strcmp(lhs, rhs) == 0)

static int32_t kramAppEncode(vector<const char*>& args);
static int32_t kramAppDecode(vector<const char*>& args);
static int32_t kramAppInfo(vector<const char*>& args);

static int32_t kramAppCommand(vector<const char*>& args);

static const char* formatFormat(MyMTLPixelFormat format)
{
    const char* fmt = "";

    switch (format) {
        case MyMTLPixelFormatBC1_RGBA_sRGB:
            fmt = " -format bc1 -srgb";
            break;
        case MyMTLPixelFormatBC1_RGBA:
            fmt = " -format bc1";
            break;
        case MyMTLPixelFormatBC4_RSnorm:
            fmt = " -format bc4 -signed";
            break;
        case MyMTLPixelFormatBC4_RUnorm:
            fmt = " -format bc4";
            break;

        // case MyMTLPixelFormatBC2_RGBA_sRGB:
        // case MyMTLPixelFormatBC2_RGBA:
        case MyMTLPixelFormatBC3_RGBA_sRGB:
            fmt = " -format bc3 -srgb";
            break;
        case MyMTLPixelFormatBC3_RGBA:
            fmt = " -format bc3";
            break;
        case MyMTLPixelFormatBC5_RGSnorm:
            fmt = " -format bc5 -signed";
            break;
        case MyMTLPixelFormatBC5_RGUnorm:
            fmt = " -format bc5";
            break;
        case MyMTLPixelFormatBC6H_RGBFloat:
            fmt = " -format bc6 -signed -hdr";
            break;
        case MyMTLPixelFormatBC6H_RGBUfloat:
            fmt = " -format bc6 -hdr";
            break;
        case MyMTLPixelFormatBC7_RGBAUnorm_sRGB:
            fmt = " -format bc7 -srgb";
            break;
        case MyMTLPixelFormatBC7_RGBAUnorm:
            fmt = " -format bc7";
            break;

        // etc2
        case MyMTLPixelFormatEAC_R11Unorm:
            fmt = " -format etc2r";
            break;
        case MyMTLPixelFormatEAC_R11Snorm:
            fmt = " -format etc2r -signed";
            break;
        case MyMTLPixelFormatETC2_RGB8:
            fmt = " -format etc2rgb";
            break;
        case MyMTLPixelFormatETC2_RGB8_sRGB:
            fmt = " -format etc2rgb -srgb";
            break;

        case MyMTLPixelFormatEAC_RG11Unorm:
            fmt = " -format etc2rg";
            break;
        case MyMTLPixelFormatEAC_RG11Snorm:
            fmt = " -format etc2rg -signed";
            break;
        case MyMTLPixelFormatEAC_RGBA8:
            fmt = " -format etc2rgba";
            break;
        case MyMTLPixelFormatEAC_RGBA8_sRGB:
            fmt = " -format etc2rgba -srgb";
            break;

        // Note: snapping to block size isn't just +3/4 when it's a 5x4 block
        // size Note: all astc hdr/ldr blocks are 16 bytes, no L+A for hdr
        //   and can only identify HDR format by looking at all blocks (or using
        //   MTLPixelFormat)
        case MyMTLPixelFormatASTC_4x4_LDR:
            fmt = " -format astc4x4";
            break;
        case MyMTLPixelFormatASTC_4x4_sRGB:
            fmt = " -format astc4x4 -srgb";
            break;
        case MyMTLPixelFormatASTC_4x4_HDR:
            fmt = " -format astc4x4 -hdr";
            break;

        case MyMTLPixelFormatASTC_5x5_LDR:
            fmt = " -format astc5x5";
            break;
        case MyMTLPixelFormatASTC_5x5_sRGB:
            fmt = " -format astc5x5 -srgb";
            break;
        case MyMTLPixelFormatASTC_5x5_HDR:
            fmt = " -format astc5x5 -hdr";
            break;

        case MyMTLPixelFormatASTC_6x6_LDR:
            fmt = " -format astc6x6";
            break;
        case MyMTLPixelFormatASTC_6x6_sRGB:
            fmt = " -format astc6x6 -srgb";
            break;
        case MyMTLPixelFormatASTC_6x6_HDR:
            fmt = " -format astc6x6 -hdr";
            break;

        case MyMTLPixelFormatASTC_8x8_LDR:
            fmt = " -format astc8x8";
            break;
        case MyMTLPixelFormatASTC_8x8_sRGB:
            fmt = " -format astc8x8 -srgb";
            break;
        case MyMTLPixelFormatASTC_8x8_HDR:
            fmt = " -format astc8x8 -hdr";
            break;

        // Explicit formats
        // not really blocks
        case MyMTLPixelFormatR8Unorm:
            fmt = " -format r8";
            break;
        case MyMTLPixelFormatRG8Unorm:
            fmt = " -format rg8";
            break;
        case MyMTLPixelFormatRGBA8Unorm:
            fmt = " -format rgba8";
            break;

        case MyMTLPixelFormatR16Float:
            fmt = " -format r16f";
            break;
        case MyMTLPixelFormatRG16Float:
            fmt = " -format rg16f";
            break;
        case MyMTLPixelFormatRGBA16Float:
            fmt = " -format rgba16f";
            break;

        case MyMTLPixelFormatR32Float:
            fmt = " -format r32f";
            break;
        case MyMTLPixelFormatRG32Float:
            fmt = " -format rg32f";
            break;
        case MyMTLPixelFormatRGBA32Float:
            fmt = " -format rgba32f";
            break;

        default:
            assert(false);  // unknown format
            break;
    }

    return fmt;
}

string formatInputAndOutput(int32_t testNumber, const char* srcFilename, MyMTLPixelFormat format, TexEncoder encoder, bool isNotPremul = false)
{
    // TODO: may want to split output, also call Images for OSX to show dimensions
    const char* dstDirBC = "bc-";
    const char* dstDirASTC = "astc-";
    const char* dstDirETC = "etc-";
    const char* dstDirExplicit = "exp-";

    const char* encATE = "ate-";
    const char* encEtcenc = "ete-";
    const char* encSquish = "squ-";
    const char* encAtscenc = "ast-";
    const char* encBcenc = "bce-";
    const char* encExplicit = "exp-";

    string cmd;

    cmd += formatFormat(format);

    // turn on premul alpha, format must have alpha channel
    if (!isNotPremul && isAlphaFormat(format)) {
        cmd += " -premul";
    }

    cmd += " -encoder ";

    switch (encoder) {
        case kTexEncoderExplicit:
            cmd += "explicit";
            break;
        case kTexEncoderATE:
            cmd += "ate";
            break;
        case kTexEncoderSquish:
            cmd += "squish";
            break;
        case kTexEncoderBcenc:
            cmd += "bcenc";
            break;
        case kTexEncoderEtcenc:
            cmd += "etcenc";
            break;
        case kTexEncoderAstcenc:
            cmd += "astcenc";
            break;
        case kTexEncoderUnknown:
            assert(false);
            break;
    }

    string dst;
    switch (encoder) {
        case kTexEncoderExplicit:
            dst += encExplicit;
            dst += dstDirExplicit;
            break;
        case kTexEncoderATE:
            dst += encATE;
            if (isBCFormat(format))
                dst += dstDirBC;
            else
                dst += dstDirASTC;
            break;
        case kTexEncoderSquish:
            dst += encSquish;
            dst += dstDirBC;
            break;
        case kTexEncoderBcenc:
            dst += encBcenc;
            dst += dstDirBC;
            break;
        case kTexEncoderEtcenc:
            dst += encEtcenc;
            dst += dstDirETC;
            break;
        case kTexEncoderAstcenc:
            dst += encAtscenc;
            dst += dstDirASTC;
            break;
        case kTexEncoderUnknown:
            assert(false);
            break;
    }

    char testNumberString[32];
    snprintf(testNumberString, sizeof(testNumberString), "test%d-", testNumber);
    dst += testNumberString;

#define srcDir "../../tests/src/"
#define dstDir "../../tests/out/dst/"

    cmd += " -input " srcDir;
    cmd += srcFilename;

    cmd += " -output " dstDir + dst;

    // replace png with ktx
    dst = srcFilename;
    size_t extSeparator = dst.rfind('.');
    assert(extSeparator != string::npos);
    dst.erase(extSeparator);
    dst.append(".ktx");

    cmd += dst;

    return cmd;
}

bool kramTestCommand(int32_t testNumber,
                     vector<const char*>& args, string& cmd)
{
    // wipe out the args, and use the ones below
    // these outlive Xcode losing all project arguments

    cmd.clear();

    TexEncoder encoder = kTexEncoderUnknown;

    // some common swizzles
    //#define SwizzleA " -swizzle 000r"
    //#define SwizzleLA " -swizzle rrrg"

#define ASTCSwizzle2nm " -swizzle gggr"  // store as L+A, decode to snorm with .ag * 2 - 1
#define ASTCSwizzleL1 " -swizzle rrr1"   // store as L
#define ASTCSwizzle2 " -swizzle gggr"    // store as L+A, decode to snorm with .ag

    // TODO: these are all run at default quality
    bool isNotPremul = true;

    switch (testNumber) {
        //--------------
        // astc tests
        case 1:
            // This doesn't us LA or dualplane when content is swizzled to gggr.
            // Encoder may be fast, but want quality.  Just decode this file using astcenc to see.
            testNumber = 1;
            encoder = kTexEncoderATE;
            cmd +=
                " -normal" ASTCSwizzle2nm +
                formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatASTC_4x4_LDR, encoder, isNotPremul);

            break;

        case 2:
            testNumber = 2;
            encoder = kTexEncoderATE;
            cmd +=
                formatInputAndOutput(testNumber, "ColorMap-a.png", MyMTLPixelFormatASTC_4x4_sRGB, encoder);

            break;

        case 3:
            testNumber = 3;
            encoder = kTexEncoderATE;
            cmd +=
                " -normal" +  // " -quality 100"
                formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatBC5_RGUnorm, encoder);

            break;

        // slow tex to enocde
        case 4:
            testNumber = 4;
            encoder = kTexEncoderATE;
            cmd +=
                formatInputAndOutput(testNumber, "color_grid-a.png", MyMTLPixelFormatASTC_4x4_sRGB, encoder);
            break;

        // slow tex to enocde
        case 5:
            testNumber = 5;
            encoder = kTexEncoderAstcenc;
            cmd +=
                formatInputAndOutput(testNumber, "color_grid-a.png", MyMTLPixelFormatASTC_4x4_sRGB, encoder);
            break;

        case 10:
            testNumber = 10;
            encoder = kTexEncoderAstcenc;
            cmd +=
                " -normal" ASTCSwizzle2nm +
                formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatASTC_4x4_LDR, encoder, isNotPremul);

            break;

        case 11:
            testNumber = 11;
            encoder = kTexEncoderAstcenc;
            cmd +=
                formatInputAndOutput(testNumber, "ColorMap-a.png", MyMTLPixelFormatASTC_4x4_sRGB, encoder);

            break;

        case 12:
            testNumber = 12;
            encoder = kTexEncoderSquish;
            cmd +=
                " -normal" +
                formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatBC5_RGUnorm, encoder);

            break;
        case 13:
            testNumber = 13;
            encoder = kTexEncoderBcenc;
            cmd +=
                " -normal" +
                formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatBC5_RGUnorm, encoder);

            break;

        case 1000:
            testNumber = 1000;
            encoder = kTexEncoderATE;
            cmd +=
                formatInputAndOutput(testNumber, "ColorMap-a.png", MyMTLPixelFormatBC7_RGBAUnorm_sRGB, encoder);
            break;

        case 1002:
            testNumber = 1002;
            encoder = kTexEncoderATE;
            cmd +=
                formatInputAndOutput(testNumber, "color_grid-a.png", MyMTLPixelFormatBC7_RGBAUnorm_sRGB, encoder);
            break;

        case 1010:
            testNumber = 1010;
            encoder = kTexEncoderSquish;
            cmd +=
                formatInputAndOutput(testNumber, "ColorMap-a.png", MyMTLPixelFormatBC3_RGBA_sRGB, encoder);
            break;

        case 1020:
            testNumber = 1020;
            // bc7enc with source, also handles other bc formats, way slower than ATE but why?
            encoder = kTexEncoderBcenc;
            cmd += " -optopaque" +
                   formatInputAndOutput(testNumber, "ColorMap-a.png", MyMTLPixelFormatBC7_RGBAUnorm_sRGB, encoder);
            break;

        // this takes 12s to process, may need to adjust quality to settings, but they're low already
        case 1022:
            testNumber = 1022;
            encoder = kTexEncoderBcenc;
            cmd +=
                formatInputAndOutput(testNumber, "color_grid-a.png", MyMTLPixelFormatBC7_RGBAUnorm_sRGB, encoder);
            break;

        //--------------
        // etc tests
        case 2000:
            testNumber = 2000;
            encoder = kTexEncoderEtcenc;
            cmd +=
                formatInputAndOutput(testNumber, "ColorMap-a.png", MyMTLPixelFormatEAC_RGBA8_sRGB, encoder);

            break;

        case 2001:
            testNumber = 2001;
            encoder = kTexEncoderEtcenc;
            cmd +=
                formatInputAndOutput(testNumber, "collectorbarrel-a.png", MyMTLPixelFormatEAC_RGBA8_sRGB, encoder);

            break;

        case 2002:
            testNumber = 2002;
            encoder = kTexEncoderEtcenc;
            cmd +=
                " -normal" +
                formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatEAC_RG11Unorm, encoder);

            break;

        case 2003:
            testNumber = 2003;
            encoder = kTexEncoderEtcenc;
            cmd += " -optopaque" +
                   formatInputAndOutput(testNumber, "color_grid-a.png", MyMTLPixelFormatEAC_RGBA8_sRGB, encoder);
            break;

            //--------------
            // sdf tests

        case 3001:
            testNumber = 3001;
            encoder = kTexEncoderExplicit;
            cmd +=
                " -sdf" +
                formatInputAndOutput(testNumber, "flipper-sdf.png", MyMTLPixelFormatR8Unorm, encoder);
            break;

        case 3002:
            testNumber = 3002;
            encoder = kTexEncoderSquish;
            cmd +=
                " -sdf" +
                formatInputAndOutput(testNumber, "flipper-sdf.png", MyMTLPixelFormatBC4_RUnorm, encoder);

            break;

        case 3003:
            testNumber = 3003;
            encoder = kTexEncoderEtcenc;
            cmd +=
                " -sdf" +
                formatInputAndOutput(testNumber, "flipper-sdf.png", MyMTLPixelFormatEAC_R11Unorm, encoder);

            break;

        case 3004:
            testNumber = 3004;
            encoder = kTexEncoderATE;
            cmd +=
                " -sdf" ASTCSwizzleL1 +
                formatInputAndOutput(testNumber, "flipper-sdf.png", MyMTLPixelFormatASTC_4x4_LDR, encoder, isNotPremul);
            break;

        default:
            return false;
            break;
    }

    KLOGI("Kram", "Testing command:\n encode %s\n", cmd.c_str());

    bool isValidEncoder = isEncoderAvailable(encoder);
    if (!isValidEncoder) {
        KLOGE("Kram", "Skipping test %d.  Encoder not available for test\n", testNumber);
        cmd.clear();
    }

    if (!cmd.empty()) {
        args.clear();

        cmd.insert(0, "encode");

        // string is modified in this case " " replaced with "\0"
        // strtok isn't re-entrant, but strtok_r is
        char* rest = (char*)cmd.c_str();
        char* token;
        while ((token = strtok_r(rest, " ", &rest))) {
            args.push_back(token);
        }

        // turn on verbose output
        args.push_back("-v");
    }

#undef srcDir

    return true;
}

static void setupTestArgs(vector<const char*>& args)
{
    int32_t testNumber = 0;

    int32_t argc = (int32_t)args.size();

    int32_t errorCode = 0;
    bool isTest = false;

    string cmd;
    vector<const char*> argsTest;

    for (int32_t i = 0; i < argc; ++i) {
        char const* word = args[i];

        if (isStringEqual(word, "-test")) {
            isTest = true;

            if (i + 1 >= argc) {
                // error
                errorCode = -1;
                break;
            }

            testNumber = atoi(args[i + 1]);

            if (!kramTestCommand(testNumber, argsTest, cmd)) {
                KLOGE("Kram", "Test %d not found\n", testNumber);
                errorCode = -1;
                break;
            }

            Timer timer;
            errorCode = kramAppCommand(argsTest);

            if (errorCode != 0) {
                KLOGE("Kram", "Test %d failure\n", testNumber);
                break;
            }
            else {
                KLOGI("Kram", "Test %d success in %0.3fs\n", testNumber,
                      timer.timeElapsed());
            }
        }
        else if (isStringEqual(word, "-testall")) {
            isTest = true;

            int32_t allTests[] = {
                // astcenc is slow, etcenc is too
                10,
                11,
                12,
                13,

                1,
                2,
                3,
                1000,
                1002,
                1020,
                1022,
                2000,
                2001,
                2002,
                3001,
                3002,
                3003,
                3004,
            };

            for (int32_t j = 0, jEnd = countof(allTests); j < jEnd; ++j) {
                testNumber = allTests[j];
                if (!kramTestCommand(testNumber, argsTest, cmd)) {
                    KLOGE("Kram", "Test %d not found\n", testNumber);
                    errorCode = -1;
                    break;
                }

                Timer timer;
                errorCode = kramAppCommand(argsTest);
                if (errorCode != 0) {
                    KLOGE("Kram", "Test %d failure\n", testNumber);
                    break;
                }
                else {
                    KLOGI("Kram", "Test %d success in %0.2fs\n", testNumber,
                          timer.timeElapsed());
                }
            }
        }

        // break on first error
        if (errorCode != 0) {
            break;
        }
    }

    if (isTest) {
        exit(errorCode);
    }
}

// display "version build kram"
#if KRAM_RELEASE
#define appName "kram"
#define usageName appName " " KRAM_VERSION
#else
#define appName "kram"
#define usageName appName " DEBUG " KRAM_VERSION
#endif

void kramDecodeUsage(bool showVersion = true)
{
    KLOGI("Kram",
          "%s\n"
          "Usage: kram decode\n"
          "\t [-swizzle rgba01]\n"
          "\t [-e/ncoder (squish | ate | etcenc | bcenc | astcenc | explicit | ..)]\n"
          "\t [-v/erbose]\n"
          "\t -i/nput .ktx\n"
          "\t -o/utput .ktx\n"
          "\n",
          showVersion ? usageName : "" );
}

void kramInfoUsage(bool showVersion = true)
{
    KLOGI("Kram",
          "%s\n"
          "Usage: kram info\n"
          "\t -i/nput <.png | .ktx>\n"
          "\t [-o/utput info.txt]\n"
          "\t [-v/erbose]\n"
          "\n",
          showVersion ? usageName : "");
}

void kramScriptUsage(bool showVersion = true)
{
    KLOGI("Kram",
          "%s\n"
          "Usage: kram script\n"
          "\t -i/nput kramscript.txt\n"
          "\t [-v/erbose]\n"
          "\t [-j/obs numJobs]\n"
          "\t [-c/ontinue]\tcontinue on errors\n"
          "\n",
          showVersion ? usageName : "");
}

void kramEncodeUsage(bool showVersion = true)
{
    const char* squishEnabled = "";
    const char* bcencEnabled = "";
    const char* ateEnabled = "";
    const char* etcencEnabled = "";
    const char* astcencEnabled = "";
    
    // prerocessor in MSVC can't handle this
    #if !COMPILE_SQUISH
    squishEnabled = "(DISABLED)";
    #endif
    #if !COMPILE_BCENC
    bcencEnabled = "(DISABLED)";
    #endif
    #if !COMPILE_ATE
    ateEnabled = "(DISABLED)";
    #endif
    #if !COMPILE_ASTCENC
    astcencEnabled = "(DISABLED)";
    #endif
    #if !COMPILE_ETCENC
    etcencEnabled = "(DISABLED)";
    #endif
    
    KLOGI("Kram",
          "%s\n"
          "Usage: kram encode\n"
          "\t -f/ormat (bc1 | astc4x4 | etc2rgba | rgba16f)\n"
          "\t [-srgb] [-signed] [-normal]\n"
          "\t -i/nput <source.png | .ktx>\n"
          "\t -o/utput <target.ktx | .ktxa>\n"
          "\n"
          "\t [-type 2d|3d|..]\n"
          "\t [-e/ncoder (squish | ate | etcenc | bcenc | astcenc | explicit | ..)]\n"
          "\t [-resize (16x32 | pow2)]\n"
          "\n"
          //"\t [-mipalign]\n"
          "\t [-mipnone]\n"
          "\t [-mipmin size] [-mipmax size]\n"
          "\n"
          "\t [-chunks 4x4]\n"
          "\t [-swizzle rg01]\n"
          "\t [-avg rxbx]\n"
          "\t [-sdf]\n"
          "\t [-premul]\n"
          "\t [-prezero]\n"
          "\t [-quality 0-100]\n"
          "\t [-optopaque]\n"
          "\t [-v]\n"
          "\n"
          "\t [-testall]\n"
          "\t [-test 1002]\n"
          "\n"
          "\n"

          "OPTIONS\n"

          "\t-type 2d|3d|cube|1darray|2darray|cubearray\n"
          "\n"

          "\t-format [r|rg|rgba| 8|16f|32f]"
          "\tExplicit format to build mips and for hdr.\n"
          "\t-format bc[1,3,4,5,7]"
          "\tBC compression\n"
          "\t-format etc2[r|rg|rgb|rgba]"
          "\tETC2 compression - r11sn, rg11sn, rgba, rgba\n"
          "\t-format astc[4x4|5x5|6x6|8x8]"
          "\tASTC compression - block size.\n"
          "\n"

          // can force an encoder when there is overlap
          "\t-encoder squish"
          "\tbc[1,3,4,5] %s\n" // can be disabled
          
          "\t-encoder bcenc"
          "\tbc[1,3,4,5,7] %s\n" // can be disabled
          
          "\t-encoder ate"
          "\tbc[1,4,5,7] %s\n" // can be disabled
          
          "\t-encoder ate"
          "\tastc[4x4,8x8] %s\n" // can be disabled

          "\t-encoder astcenc"
          "\tastc[4x4,5x5,6x6,8x8] ldr/hdr support %s\n" // can be disabled

          "\t-encoder etcenc"
          "\tetc2[r,rg,rgb,rgba] %s\n" // can be disabled

          "\t-encoder explicit"
          "\tr|rg|rgba[8|16f|32f]\n"
          "\n"

          "\t-mipalign"
          "\tAlign mip levels with .ktxa output \n"
          "\t-mipnone"
          "\tDon't build mips even if pow2 dimensions\n"

          "\t-mipmin size"
          "\tOnly output mips >= size px\n"
          "\t-mipmax size"
          "\tOnly output mips <= size px\n"
          "\n"

          // tex to normal
          "\t-height"
          "\tConvert height.x to normal.xy\n"
          "\t-heightScale scale"
          "\tScale heights up down to adjust normal map\n"
          "\t-wrap"
          "\tWrap texture at edges (height only for now)\n"
          "\n"

          "\t-srgb"
          "\tsRGB for rgb/rgba formats\n"
          "\t-signed"
          "\tSigned r or rg for etc/bc formats, astc doesn't have signed format.\n"
          "\t-normal"
          "\tNormal map rg storage signed for etc/bc (rg01), only unsigned astc L+A (gggr).\n"
          "\t-sdf"
          "\tGenerate single-channel SDF from a bitmap, can mip and drop large mips. Encode to r8, bc4, etc2r, astc4x4 (Unorm LLL1) to encode\n"

          // premul is not on by default, but really should be or textures aren't sampled correctly
          // but this really only applies to color channel textures, so off by default.
          "\t-premul"
          "\tPremultiplied alpha to src pixels before output\n"
          "\n"

          // This is meant to work with shaders that (incorrectly) premul after sampling.
          // limits the rgb bleed in regions that should not display colors.  Can stil have black color halos.
          "\t-prezero"
          "\tPremultiplied alpha to src pixels before output but only where a=0\n"
          "\n"
          
          "\t-premulrgb"
          "\tPremultiplied alpha to src pixels at load to emulate Photoshop, don't use with -premul\n"
          "\n"
          
          "\t-optopaque"
          "\tChange format from bc7/3 to bc1, or etc2rgba to rgba if opaque\n"
          "\n"

          "\t-chunks 4x4"
          "\tSpecifies how many chunks to split up texture into 2darray\n"
          
          "\t-swizzle [rgba01 x4]"
          "\tSpecifies pre-encode swizzle pattern\n"
          "\t-avg [rgba]"
          "\tPost-swizzle, average channels per block (f.e. normals) lrgb astc/bc3/etc2rgba\n"

          "\t-v"
          "\tVerbose encoding output\n"
          "\n",
          showVersion ? usageName : "",
          
          squishEnabled,
          bcencEnabled,
          ateEnabled,
          ateEnabled,
          astcencEnabled,
          etcencEnabled
          );
}

void kramUsage()
{
    KLOGI("Kram",
          usageName "\n"
          "SYNTAX\nkram [encode | decode | info | script | ...]\n"
          );

    kramEncodeUsage(false);
    kramInfoUsage(false);
    kramDecodeUsage(false);
    kramScriptUsage(false);
}

static int32_t kramAppInfo(vector<const char*>& args)
{
    // this is help
    int32_t argc = (int32_t)args.size();
    if (argc == 0) {
        kramInfoUsage();
        return 0;
    }
    
    string srcFilename;
    string dstFilename;

    bool isVerbose = false;

    bool error = false;
    for (int32_t i = 0; i < argc; ++i) {
        const char* word = args[i];
        if (word[0] != '-') {
            KLOGE("Kram", "unexpected argument \"%s\"\n",
                  word);
            error = true;
            break;
        }

        if (isStringEqual(word, "-output") ||
            isStringEqual(word, "-o")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "no output file defined");
                error = true;
                break;
            }

            dstFilename = args[i];
            continue;
        }
        else if (isStringEqual(word, "-input") ||
                 isStringEqual(word, "-i")) {
            ++i;
            if (i >= argc) {
                error = true;
                KLOGE("Kram", "no input file defined");
                break;
            }

            srcFilename = args[i];
            continue;
        }
        else if (isStringEqual(word, "-v") ||
                 isStringEqual(word, "-verbose")) {
            isVerbose = true;
        }
        else {
            KLOGE("Kram", "unexpected argument \"%s\"\n",
                  word);
            error = true;
            break;
        }
    }

    if (srcFilename.empty()) {
        KLOGE("Kram", "no input file supplied");
        error = true;
    }

    bool isPNG = endsWith(srcFilename, ".png");
    bool isKTX = endsWith(srcFilename, ".ktx") || endsWith(srcFilename, ".ktx2");

    if (!(isPNG || isKTX)) {
        KLOGE("Kram", "info only supports png, ktx, ktx2 inputs");
        error = true;
    }

    if (error) {
        kramInfoUsage();
        return -1;
    }

    string info = kramInfoToString(srcFilename, isVerbose);
    if (info.empty()) {
        return -1;
    }

    // now write the string to output (always appends for scripting purposes, so caller must wipe output file)
    FILE* fp = stdout;

    FileHelper dstFileHelper;
    if (!dstFilename.empty()) {
        if (!dstFileHelper.open(dstFilename.c_str(), "w+")) {
            KLOGE("Kram", "info couldn't open output file");
            return -1;
        }

        fp = dstFileHelper.pointer();
    }

    fprintf(fp, "%s", info.c_str());

    return 0;
}

// this is the main chunk of info generation, can be called without writing result to stdio
string kramInfoToString(const string& srcFilename, bool isVerbose)
{
    bool isPNG = endsWith(srcFilename, ".png");
    bool isKTX = endsWith(srcFilename, ".ktx") || endsWith(srcFilename, ".ktx2");

    MmapHelper srcMmapHelper;
    FileHelper srcFileHelper;
    vector<uint8_t> srcFileBuffer;

    string info;

    // handle png and ktx
    if (isPNG) {
        // This was taken out of SetupSourceImage, dont want to decode PNG yet
        // just peek tha the header.
        const uint8_t* data = nullptr;
        int32_t dataSize = 0;

        // first try mmap, and then use file -> buffer
        bool useMmap = true;
        if (!srcMmapHelper.open(srcFilename.c_str())) {
            // fallback to file system if no mmap or it failed
            useMmap = false;
        }

        if (useMmap) {
            data = srcMmapHelper.data();
            dataSize = (int32_t)srcMmapHelper.dataLength();
        }
        else {
            if (!srcFileHelper.open(srcFilename.c_str(), "rb")) {
                KLOGE("Kram", "File input \"%s\" could not be opened for read.\n",
                      srcFilename.c_str());
                return "";
            }

            // read entire png into memory
            // even though really just want to peek at header
            int64_t size = srcFileHelper.size();
            if (size < 0) return "";
            
            srcFileBuffer.resize(size);
            if (!srcFileHelper.read(srcFileBuffer.data(), size)) {
                return "";
            }

            data = srcFileBuffer.data();
            dataSize = (int32_t)srcFileBuffer.size();
        }
        info = kramInfoPNGToString(srcFilename, data, dataSize, isVerbose);
        
    }
    else if (isKTX) {
        KTXImage srcImage;

        // Note: could change to not read any mips
        bool success = SetupSourceKTX(srcMmapHelper, srcFileHelper, srcFileBuffer,
                                      srcFilename, srcImage);
        if (!success) {
            KLOGE("Kram", "info couldn't open ktx file");
            return "";
        }
        
        info = kramInfoKTXToString(srcFilename, srcImage, isVerbose);
    }
    
    return info;
}


string kramInfoPNGToString(const string& srcFilename, const uint8_t* data, uint64_t dataSize, bool /* isVerbose */)
{
    // vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t errorLode = 0;

    // can identify 16unorm data for heightmaps via this call
    LodePNGState state;
    lodepng_state_init(&state);

    errorLode = lodepng_inspect(&width, &height, &state, data, dataSize);
    if (errorLode != 0) {
        KLOGE("Kram", "info couldn't open png file");
        return "";
    }

    string info;
    
    bool hasColor = true;
    bool hasAlpha = true;
    bool hasPalette = state.info_png.color.colortype == LCT_PALETTE;

    switch (state.info_png.color.colortype) {
        case LCT_GREY:
        case LCT_GREY_ALPHA:
            hasColor = false;
            break;
        case LCT_MAX_OCTET_VALUE:
        case LCT_RGB:
        case LCT_RGBA:
        case LCT_PALETTE:  // ?
            hasColor = true;
            break;
    }

    switch (state.info_png.color.colortype) {
        case LCT_GREY:
        case LCT_RGB:
            hasAlpha = false;
            break;
        case LCT_MAX_OCTET_VALUE:
        case LCT_RGBA:
        case LCT_GREY_ALPHA:
        case LCT_PALETTE:  // ?
            hasAlpha = true;
            break;
    }

    string tmp;
    bool isMB = (dataSize > (512 * 1024));
    sprintf(tmp,
            "file: %s\n"
            "size: %" PRIu64 "\n"
            "sizm: %0.3f %s\n",
            srcFilename.c_str(),
            dataSize,
            isMB ? dataSize / (1024.0f * 1024.0f) : dataSize / 1024.0f,
            isMB ? "MB" : "KB");
    info += tmp;

    sprintf(tmp,
            "type: %s\n"
            "dims: %dx%d\n"
            "dimm: %0.3f MP\n"
            "bitd: %d\n"
            "colr: %s\n"
            "alph: %s\n"
            "palt: %s\n",
            textureTypeName(MyMTLTextureType2D),
            width, height,
            width * height / (1000.0f * 1000.0f),
            state.info_png.color.bitdepth,
            hasColor ? "y" : "n",
            hasAlpha ? "y" : "n",
            hasPalette ? "y" : "n");
    info += tmp;

    // optional block with ppi
    // TODO: inspect doesn't parse this block, only decode call does
//        if (state.info_png.phys_defined && state.info_png.phys_unit == 1) {
//            float metersToInches = 39.37;
//            sprintf(tmp,
//                    "ppix: %d\n"
//                    "ppiy: %d\n",
//                    (int32_t)(state.info_png.phys_x * metersToInches),
//                    (int32_t)(state.info_png.phys_y * metersToInches));
//            info += tmp;
//        }

    // TODO: also bkgd blocks.
    // TODO: sRGB, cHRM, gAMA and other colorspace blocks aren't supported by lodepng,
    // so can't report those would need to walk those blocks manually.
    return info;
}

string kramInfoKTXToString(const string& srcFilename, const KTXImage& srcImage, bool isVerbose)
{
    string info;
    
    // for now driving everything off metal type, but should switch to neutral
    MyMTLPixelFormat metalFormat = srcImage.pixelFormat;

    int32_t dataSize = srcImage.fileDataLength;
    
    string tmp;
    bool isMB = (dataSize > (512 * 1024));
    sprintf(tmp,
            "file: %s\n"
            "size: %d\n"
            "sizm: %0.3f %s\n",
            srcFilename.c_str(),
            dataSize,
            isMB ? dataSize / (1024.0f * 1024.0f) : dataSize / 1024.0f,
            isMB ? "MB" : "KB");
    info += tmp;

    int32_t pixelMultiplier = srcImage.totalChunks();
    
    float numPixels = srcImage.width * srcImage.height;
    numPixels *= (float)pixelMultiplier;
    
    if (srcImage.header.numberOfMipmapLevels > 1) {
        numPixels *= 4.0 / 3.0f; // estimate for now
    }
    
    numPixels /= (1000.0f * 1000.0f);
    
    auto textureType = srcImage.header.metalTextureType();
    switch (textureType) {
        case MyMTLTextureType1DArray:
        case MyMTLTextureType2D:
        case MyMTLTextureTypeCube:
        case MyMTLTextureTypeCubeArray:
        case MyMTLTextureType2DArray:
            sprintf(tmp,
                    "type: %s\n"
                    "dims: %dx%d\n"
                    "dimm: %0.3f MP\n"
                    "mips: %d\n",
                    textureTypeName(srcImage.header.metalTextureType()),
                    srcImage.width, srcImage.height,
                    numPixels,
                    srcImage.header.numberOfMipmapLevels);
            break;
        case MyMTLTextureType3D:
            sprintf(tmp,
                    "type: %s\n"
                    "dims: %dx%dx%d\n"
                    "dimm: %0.3f MP\n"
                    "mips: %d\n",
                    textureTypeName(srcImage.header.metalTextureType()),
                    srcImage.width, srcImage.height, srcImage.depth,
                    numPixels,
                    srcImage.header.numberOfMipmapLevels);
            break;
    }
    info += tmp;

    // print out the array
    if (srcImage.header.numberOfArrayElements > 1) {
        sprintf(tmp,
                "arry: %d\n",
                srcImage.header.numberOfArrayElements);

        info += tmp;
    }

    sprintf(tmp,
            "fmtk: %s\n"
            "fmtm: %s (%d)\n"
            "fmtv: %s (%d)\n"
            "fmtg: %s (0x%04X)\n",
            formatTypeName(metalFormat),
            metalTypeName(metalFormat), metalFormat,
            vulkanTypeName(metalFormat), vulkanType(metalFormat),
            glTypeName(metalFormat), glType(metalFormat));
    info += tmp;

    // report any props
    string propText;
    for (const auto& prop : srcImage.props) {
        sprintf(propText, "prop: %s %s\n", prop.first.c_str(), prop.second.c_str());
        info += propText;
    }

    // TODO: handle zstd compressed KTX2 too, they have a length and compressed length field
    // also Basis + zstd
    
    if (isVerbose) {
        // dump mips/dims, but this can be a lot of data on arrays
        int32_t mipLevel = 0;
        int32_t w = srcImage.width;
        int32_t h = srcImage.height;

        for (const auto& mip : srcImage.mipLevels) {
            sprintf(tmp,
                    "mipn: %d\n"
                    "mipd: %dx%d\n"
                    "mips: %" PRIu64 "\n"
                    "mipc: %dx\n"
                    "mipo: %" PRIu64 "\n",
                    w, h, mipLevel++, mip.length, srcImage.totalChunks(), mip.offset);
            info += tmp;

            // drop a mip level
            mipDown(w, h);
        }
    }

    return info;
}

static int32_t kramAppDecode(vector<const char*>& args)
{
    // this is help
    int32_t argc = (int32_t)args.size();
    if (argc == 0) {
        kramDecodeUsage();
        return 0;
    }
    
    // decode and write out to ktx file for now
    // all mips, or no mips, can preserve name-value pairs in original

    // parse the command-line
    string srcFilename;
    string dstFilename;

    bool error = false;
    bool isVerbose = false;
    string swizzleText;
    TexEncoder textureDecoder = kTexEncoderUnknown;

    for (int32_t i = 0; i < argc; ++i) {
        const char* word = args[i];
        if (word[0] != '-') {
            KLOGE("Kram", "unexpected argument \"%s\"\n",
                  word);
            error = true;
            break;
        }

        if (isStringEqual(word, "-output") ||
            isStringEqual(word, "-o")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "decode output missing");
                error = true;
                break;
            }

            // TODO: if args ends with /, then output to that dir
            dstFilename = args[i];
            continue;
        }
        else if (isStringEqual(word, "-input") ||
                 isStringEqual(word, "-i")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "decode input missing");
                error = true;
                break;
            }

            srcFilename = args[i];
            continue;
        }

        else if (isStringEqual(word, "-swizzle")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "decode swizzle missing");
                error = true;
                break;
            }

            const char* swizzleString = args[i];
            if (!isSwizzleValid(swizzleString)) {
                KLOGE("Kram", "decode swizzle invalid");
                error = true;
                break;
            }
            swizzleText = swizzleString;
            continue;
        }
        // this is really decoder, but keep same argument as encoder
        else if (isStringEqual(word, "-e") ||
                 isStringEqual(word, "-encoder")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "encoder arg invalid");
                error = true;
                break;
            }

            textureDecoder = parseEncoder(args[i]);
            continue;
        }

        // probably should be per-command and global verbose
        else if (isStringEqual(word, "-v") ||
                 isStringEqual(word, "-verbose")) {
            isVerbose = true;
            continue;
        }
        else {
            KLOGE("Kram", "unexpected argument \"%s\"\n",
                  word);
            error = true;
            break;
        }
    }

    if (srcFilename.empty()) {
        KLOGE("Kram", "decode needs ktx input");
        error = true;
    }
    if (dstFilename.empty()) {
        KLOGE("Kram", "decode needs ktx output");
        error = true;
    }

    bool isKTX = endsWith(srcFilename, ".ktx") || endsWith(srcFilename, ".ktx2");

    if (!isKTX) {
        KLOGE("Kram", "decode only supports ktx and ktx2 input");
        error = true;
    }

    isKTX = endsWith(dstFilename, ".ktx");

    if (!isKTX) {
        KLOGE("Kram", "decode only supports ktx output");
        error = true;
    }

    if (error) {
        kramDecodeUsage();
        return -1;
    }

    KTXImage srcImage;
    MmapHelper srcMmapHelper;
    FileHelper srcFileHelper;
    FileHelper tmpFileHelper;
    vector<uint8_t> srcFileBuffer;

    bool success = SetupSourceKTX(srcMmapHelper, srcFileHelper, srcFileBuffer,
                                  srcFilename, srcImage);

    // TODO: for hdr decode, may need to walk blocks or ask caller to pass -hdr flag
    if (!validateFormatAndDecoder(srcImage.textureType, srcImage.pixelFormat, textureDecoder)) {
        KLOGE("Kram", "format decode only supports ktx output");
        return -1;
    }

    success = success && SetupTmpFile(tmpFileHelper, ".ktx");

    if (success && isVerbose) {
        KLOGI("Kram", "Decoding %s to %s with %s\n",
              textureTypeName(srcImage.textureType),
              metalTypeName(srcImage.pixelFormat),
              encoderName(textureDecoder));
    }
    
    Image tmpImage;  // just to call decode
    success = success && tmpImage.decode(srcImage, tmpFileHelper.pointer(), textureDecoder, isVerbose, swizzleText);

    // rename to dest filepath, note this only occurs if above succeeded
    // so any existing files are left alone on failure.
    success = success && tmpFileHelper.copyTemporaryFileTo(dstFilename.c_str());

    return success ? 0 : -1;
}

static int32_t kramAppEncode(vector<const char*>& args)
{
    // this is help
    int32_t argc = (int32_t)args.size();
    if (argc == 0) {
        kramEncodeUsage();
        return 0;
    }
    
    // parse the command-line
    string srcFilename;
    string dstFilename;
    string resizeString;

    ImageInfoArgs infoArgs;

    bool isPremulRgb = false;
    
    bool error = false;
    for (int32_t i = 0; i < argc; ++i) {
        // check for options
        const char* word = args[i];
        if (word[0] != '-') {
            KLOGE("Kram", "unexpected argument \"%s\"\n",
                  word);
            error = true;
            break;
        }

        if (isStringEqual(word, "-sdf")) {
            infoArgs.doSDF = true;
            continue;
        }
        else if (isStringEqual(word, "-optopaque")) {
            infoArgs.optimizeFormatForOpaque = true;
            continue;
        }

        // mip setting
        else if (isStringEqual(word, "-mipmax")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "mipmax arg invalid");
                error = true;
                break;
            }

            infoArgs.mipMaxSize = atoi(args[i]);
            continue;
        }
        else if (isStringEqual(word, "-mipmin")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "mipmin arg invalid");
                error = true;
                break;
            }

            infoArgs.mipMinSize = atoi(args[i]);
            continue;
        }
        else if (isStringEqual(word, "-mipnone")) {
            // disable mips even if pow2
            infoArgs.doMipmaps = false;
            continue;
        }
//        else if (isStringEqual(word, "-mipalign")) {
//            // pad start of each mip to pixel/block size of format
//            infoArgs.skipImageLength = true;
//            continue;
//        }

        else if (isStringEqual(word, "-heightScale")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "heightScale arg invalid");
                error = true;
                break;
            }
            
            infoArgs.isHeight = true;
            infoArgs.heightScale = atof(args[i]);
            
            // Note: caller can negate scale, but don't allow scale 0.
            if (infoArgs.heightScale == 0.0f) {
                KLOGE("Kram", "heightScale arg cannot be 0");
                error = true;
            }
            continue;
        }
        else if (isStringEqual(word, "-height")) {
            // converted to a normal map
            infoArgs.isHeight = true;
            continue;
        }
        else if (isStringEqual(word, "-wrap")) {
            // whether texture is clamp or wrap
            infoArgs.isWrap = true;
            continue;
        }
        
        
        else if (isStringEqual(word, "-e") ||
                 isStringEqual(word, "-encoder")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "encoder arg invalid");
                error = true;
                break;
            }

            infoArgs.textureEncoder = parseEncoder(args[i]);
            continue;
        }

        else if (isStringEqual(word, "-swizzle")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "swizzle arg invalid");
                error = true;
                break;
            }

            const char* swizzleString = args[i];
            if (!isSwizzleValid(swizzleString)) {
                KLOGE("Kram", "swizzle arg invalid");
                error = true;
                break;
            }
            infoArgs.swizzleText = swizzleString;
            continue;
        }

        else if (isStringEqual(word, "-chunks")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "chunks count missing");
                error = true;
                break;
            }

            const char* chunksString = args[i];
            int32_t chunksX = 0;
            int32_t chunksY = 0;
            if (sscanf(chunksString, "%dx%d", &chunksX, &chunksY) != 2) {
                KLOGE("Kram", "chunks count arg invalid");
                error = true;
                break;
            }
            
            // this is a count of how many chunks (tiles) across and down
            // currently assuming all slots contain data, but may need a total count
            // also an assumption that texture dimensions evenly divisible by count
            infoArgs.chunksX = chunksX;
            infoArgs.chunksY = chunksY;
            infoArgs.chunksCount = chunksX * chunksY;
            
            continue;
        }
        
        else if (isStringEqual(word, "-avg")) {
            ++i;
            const char* channelString = args[i];
            if (!isChannelValid(channelString)) {
                KLOGE("Kram", "avg channel arg invalid");
                error = true;
                break;
            }
            infoArgs.averageChannels = channelString;
            continue;
        }
        else if (isStringEqual(word, "-type")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "type arg invalid");
                error = true;
                break;
            }

            infoArgs.textureType = parseTextureType(args[i]);
            continue;
        }
        else if (isStringEqual(word, "-quality")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "quality arg invalid");
                error = true;
                break;
            }

            infoArgs.quality = atoi(args[i]);
            continue;
        }

        else if (isStringEqual(word, "-output") ||
                 isStringEqual(word, "-o")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "no output file defined");
                error = true;
                break;
            }

            // TODO: if args ends with /, then output to that dir
            dstFilename = args[i];

//            // see if it's a ktxa file
//            if (dstFilename.back() == 'a' ||
//                dstFilename.back() == 'A') {
//                infoArgs.skipImageLength = true;
//            }
            continue;
        }
        else if (isStringEqual(word, "-input") ||
                 isStringEqual(word, "-i")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "no input file defined");
                error = true;
                break;
            }

            srcFilename = args[i];
            continue;
        }

        // these affect the format
        else if (isStringEqual(word, "-hdr")) {
            // not validating format for whether it's srgb or not
            infoArgs.isHDR = true;
            continue;
        }
        else if (isStringEqual(word, "-srgb")) {
            // not validating format for whether it's srgb or not
            infoArgs.isSRGB = true;
            continue;
        }
        else if (isStringEqual(word, "-signed")) {
            // not validating format for whether it's signed or not
            infoArgs.isSigned = true;
            continue;
        }

        else if (isStringEqual(word, "-normal")) {
            infoArgs.isNormal = true;
            continue;
        }
        else if (isStringEqual(word, "-resize")) {
            ++i;
            if (i >= argc) {
                error = true;
                KLOGE("Kram", "resize arg invalid");
                break;
            }

            resizeString = args[i];
            continue;
        }

        // This means to post-multiply alpha after loading, not that incoming data in already premul
        // png has the limitation that it's unmul, but tiff/exr can store premul.  With 8-bit images
        // really would prefer to premul them when building the texture.
        else if (isStringEqual(word, "-premul")) {
            infoArgs.isPremultiplied = true;
            continue;
        }
        else if (isStringEqual(word, "-prezero")) {
            infoArgs.isPrezero = true;
            continue;
        }
        // this means premul the data at read from srgb, this it to match photoshop
        else if (isStringEqual(word, "-premulrgb")) {
            isPremulRgb = true;
            continue;
        }
        
        else if (isStringEqual(word, "-v") ||
                 isStringEqual(word, "-verbose")) {
            infoArgs.isVerbose = true;
            continue;
        }
        else if (isStringEqual(word, "-f") ||
                 isStringEqual(word, "-format")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "format arg invalid");
                error = true;
                break;
            }

            infoArgs.formatString = args[i];
            continue;
        }
        else {
            KLOGE("Kram", "unexpected argument \"%s\"\n",
                  word);
            error = true;
            break;
        }
    }

    if (!error &&
        !validateFormatAndEncoder(infoArgs)) {
        KLOGE("Kram", "encoder not validated for format\n");

        error = true;
    }

    // parse and convert resize string
    int32_t wResize = 0;
    int32_t hResize = 0;
    bool resizePow2 = false;

    if (!resizeString.empty()) {
        // TODO: parse filter, pow2+/-, then have code compute that size from w/h
        if (resizeString == "pow2") {
            resizePow2 = true;
        }
        else if (sscanf(resizeString.c_str(), "%dx%d", &wResize, &hResize) != 2) {
            KLOGE("Kram", "resize argument must be pow2 or wxh form\n");
            error = true;
        }
    }

    if (infoArgs.pixelFormat == MyMTLPixelFormatInvalid) {
        KLOGE("Kram", "invalid format given\n");
        error = true;
    }
    if (srcFilename.empty()) {
        KLOGE("Kram", "no input file given\n");
        error = true;
    }
    if (dstFilename.empty()) {
        KLOGE("Kram", "no output file given\n");
        error = true;
    }

    bool isKTX = endsWith(srcFilename, ".ktx") || endsWith(srcFilename, ".ktx2");
    bool isPNG = endsWith(srcFilename, ".png");

    if (!(isPNG || isKTX)) {
        KLOGE("Kram", "encode only supports png, ktx, ktx2 input");
        error = true;
    }

    bool isDstKTX = endsWith(dstFilename, ".ktx");
   
    if (!isDstKTX) {
        KLOGE("Kram", "encode only supports ktx output");
        error = true;
    }

    if (error) {
        kramEncodeUsage();
        return -1;
    }

    // Any new settings just go into this struct which is passed into enoder
    ImageInfo info;
    info.initWithArgs(infoArgs);

    // load the source image
    // The helper keeps ktx mips in mmap alive in case want to read them
    // incrementally. Fallback to read into fileBuffer if mmap fails.
    Image srcImage;
    MmapHelper srcMmapHelper;
    FileHelper srcFileHelper;
    FileHelper tmpFileHelper;
    vector<uint8_t> srcFileBuffer;

    bool success = SetupSourceImage(srcMmapHelper, srcFileHelper, srcFileBuffer,
                                    srcFilename, srcImage, isPremulRgb);

    if (success) {
        success = SetupTmpFile(tmpFileHelper, ".ktx");

        if (!success) {
            KLOGE("Kram", "encode couldn't generate tmp file for output");
        }
    }

    // so now can complete validation knowing hdr vs. ldr input
    // this checks the dst format
    if (success) {
        bool isHDR = srcImage.pixelsFloat() != nullptr;

        if (isHDR) {
            MyMTLPixelFormat format = info.pixelFormat;

            // astcecnc is only hdr encoder currently and explicit output to
            // 16f/32f mips.
            if (!isFloatFormat(format)) {
                KLOGE("Kram", "only explicit and encoded float formats for hdr");
                return -1;
            }

            bool isASTC = isASTCFormat(format);

            // ATE cannot encode, or may not be compiled in
            if (isASTC && info.textureEncoder != kTexEncoderAstcenc) {
                // validation picked wrong encoder for 4x4 and 8x8 (probably
                // ATE), since didn't know hdr state until now just change
                // encoder, note reference to infoArgs and not info here
                if (infoArgs.textureEncoder == kTexEncoderUnknown) {
                    info.textureEncoder = kTexEncoderAstcenc;
                }
                else {
                    // caller picked wrong encoder without hdr support, so fail
                    KLOGE("Kram", "encoder doesn't support astc hdr");
                    return -1;
                }
            }

            // TODO: find an encoder for BC6
            bool isBC = format == MyMTLPixelFormatBC6H_RGBFloat ||
                        format == MyMTLPixelFormatBC6H_RGBUfloat;
            if (isBC) {
                KLOGE("Kram", "don't have a bc6 encoder");
                return -1;
            }

            // allows explicit output
        }

        info.initWithSourceImage(srcImage);

        if (success && ((wResize && hResize) || resizePow2)) {
            success = srcImage.resizeImage(wResize, hResize, resizePow2, kImageResizeFilterPoint);

            if (!success) {
                KLOGE("Kram", "resize failed");
            }
        }

        if (info.isVerbose) {
            KLOGI("Kram", "Encoding %s to %s with %s\n",
                  textureTypeName(info.textureType),
                  formatTypeName(info.pixelFormat),
                  encoderName(info.textureEncoder));
        }
        
        if (success) {
            success = srcImage.encode(info, tmpFileHelper.pointer());
            
            if (!success) {
                KLOGE("Kram", "encode failed");
            }
        }

        // rename to dest filepath, note this only occurs if above succeeded
        // so any existing files are left alone on failure.
        if (success) {
            success = tmpFileHelper.copyTemporaryFileTo(dstFilename.c_str());

            if (!success) {
                KLOGE("Kram", "rename of temp file failed");
            }
        }
    }

    // done
    return success ? 0 : -1;
}

int32_t kramAppScript(vector<const char*>& args)
{
    // this is help
    int32_t argc = (int32_t)args.size();
    if (argc == 0) {
        kramScriptUsage();
        return 0;
    }
    
    string srcFilename;

    bool isVerbose = false;
    bool error = false;
    // this won't stop immediately, but when error occurs, no more tasks will exectue
    bool isHaltedOnError = true;

    int32_t numJobs = 1;

    for (int32_t i = 0; i < argc; ++i) {
        // check for options
        const char* word = args[i];
        if (word[0] != '-') {
            KLOGE("Kram", "unexpected argument \"%s\"\n",
                  word);
            error = true;
            break;
        }

        if (false) {
            // TODO: add jobs count
        }
        else if (isStringEqual(word, "-input") ||
                 isStringEqual(word, "-i")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "no input file defined");
                error = true;
                break;
            }

            srcFilename = args[i];
            continue;
        }
        else if (isStringEqual(word, "-jobs") ||
                 isStringEqual(word, "-j")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "no job count defined");

                error = true;
                break;
            }

            numJobs = atoi(args[i]);
            continue;
        }
        else if (isStringEqual(word, "-v") ||
                 isStringEqual(word, "-verbose")) {
            isVerbose = true;
        }
        else if (isStringEqual(word, "-c") ||
                 isStringEqual(word, "-continue")) {
            isHaltedOnError = false;
        }
        else {
            KLOGE("Kram", "unexpected argument \"%s\"\n",
                  word);
            error = true;
            break;
        }
    }

    if (srcFilename.empty()) {
        KLOGE("Kram", "script needs input file");
        error = true;
    }

    if (error) {
        kramScriptUsage();
        return -1;
    }

    FileHelper fileHelper;
    if (!fileHelper.open(srcFilename.c_str(), "r")) {
        KLOGE("Kram", "script couldn't find src file %s", srcFilename.c_str());
        return -1;
    }

    FILE* fp = fileHelper.pointer();
    char str[4096];

    string commandAndArgs;

    Timer scriptTimer;

    // as a global this auto allocates 16 threads, and don't want that unless actually
    // using scripting.  And even then want control over the number of threads.
    atomic<int32_t> errorCounter(0);  // doesn't initialize to 0 otherwise
    atomic<int32_t> skippedCounter(0);
    int32_t commandCounter = 0;

    {
        task_system system(numJobs);

        // TODO: should really limit threads if less than command count.
        if (isVerbose) {
            KLOGI("Kram", "script system started with %d threads", system.num_threads());
        }

        while (fp) {
            // serially read commands out of the script
            fgets(str, sizeof(str), fp);
            if (feof(fp)) {
                break;
            }

            commandAndArgs = str;
            if (commandAndArgs.empty()) {
                continue;
            }

            commandCounter++;

            if (commandAndArgs.back() == '\n') {
                commandAndArgs.pop_back();
            }

            // async execute the command across the provided threads
            // this works for symmetric an asymmetric cores.  Work
            // stealing will happen on low perf cores that can't keep up.
            // Could peek at src images to determine dimensions and mem
            // usage estimates.  But then would need hard/easy queues.

            system.async_([&, commandAndArgs]() mutable {
                // stop any new work when not "continue on error"
                if (isHaltedOnError && int32_t(errorCounter) > 0) {
                    skippedCounter++;
                    return 0;  // not really success, just skipping command
                }

                Timer commandTimer;
                if (isVerbose) {
                    KLOGI("Kram", "running %s", commandAndArgs.c_str());
                }

                string commandAndArgsCopy = commandAndArgs;

                // tokenize the strings
                vector<const char*> args;
                // string is modified in this case " " replaced with "\0"
                // strtok isn't re-entrant, but strtok_r is
                // https://www.geeksforgeeks.org/strtok-strtok_r-functions-c-examples/
                char* rest = (char*)commandAndArgs.c_str();
                char* token;
                while ((token = strtok_r(rest, " ", &rest))) {
                    args.push_back(token);
                    //token = strtok_r(NULL, " ");
                }
                const char* command = args[0];

                int32_t errorCode = kramAppCommand(args);

                if (isVerbose) {
                    auto timeElapsed = commandTimer.timeElapsed();
                    if (timeElapsed > 1.0) {
                        // TODO: extract output filename
                        // TODO: task sys passes threadIndex into this, so can report which thread completed work
                        KLOGI("Kram", "perf: %s %s took %0.3fs", command, "file", timeElapsed);
                    }
                }

                if (errorCode != 0) {
                    KLOGE("Kram", "cmd: failed %s", commandAndArgsCopy.c_str());
                    errorCounter++;

                    return errorCode;
                }

                return 0;
            });
        }
    }

    // There are joins done at close of scope above before task system shuts down.
    // This makes sure that return value is accurate if there are errors.  Most task
    // systems don't have this, and shutting down the entire task system isn't ideal.
    // There's a future system that we could block on instead.

    if (errorCounter > 0) {
        KLOGE("Kram", "script %d/%d commands failed", int32_t(errorCounter), commandCounter);
        return -1;
    }

    if (isVerbose) {
        KLOGI("Kram", "script completed %d commands in %0.3fs", commandCounter, scriptTimer.timeElapsed());
    }

    return 0;
}

enum CommandType {
    kCommandTypeUnknown,

    kCommandTypeEncode,
    kCommandTypeDecode,
    kCommandTypeInfo,
    kCommandTypeScript,
    // TODO: more commands, but scripting doesn't deal with failure or dependency
    //    kCommandTypeMerge, // combine channels from multiple png/ktx into one ktx
    //    kCommandTypeAtlas, // combine images into a single texture + atlas table (atlas to 2d or 2darray)
    //    kCommandTypeTile, // split up zip of ktx into zip of aligned compressed tiles for SVT, 16k vs 64k tile size
};

CommandType parseCommandType(const char* command)
{
    CommandType commandType = kCommandTypeUnknown;

    if (isStringEqual(command, "encode")) {
        commandType = kCommandTypeEncode;
    }
    else if (isStringEqual(command, "decode")) {
        commandType = kCommandTypeDecode;
    }
    else if (isStringEqual(command, "info")) {
        commandType = kCommandTypeInfo;
    }
    else if (isStringEqual(command, "script")) {
        commandType = kCommandTypeScript;
    }

    return commandType;
}

void PSTest() {
    static bool doTest = false;
    if (!doTest) {
        return;
    }
    
    // So it looks like Photoshop is doing srgb * alpha right away on PNG import. This results in dimmer colors
    // when they are read on the GPU, since then the gpu does srgb to linear conversion.  values2
    // is that case below.  Also note that the Photoshop color picker shows only srgb intensities, not the linear.
    // color value.  This lets it line up with screen color pickers like Apple DCM.  Apple Preview also shows
    // images with the same dim colors, so it's replicating what Photoshop does.
    //
    // Gimp and kramv do what is in values3 resulting in brighter intensities. One question with formats like
    // astc that interpolate the endpoints in srgb space off the selectors is how to encode colors.
    // Almost makes sense to drop srgb when premul alpha is involved and store linear color instead.
    // Figma follows that convention.

    // Here's kramv's srgb flow:
    // PNG unmul alpha -> srbToLinear(rgb) * alpha -> build mips in linear -> linearToSrgb(lin.rgb)
    //   -> encode endpoints/colors -> BC/ASTC/ETC2
    //
    // Here's Photoshop I think:
    // PNG unmul alpha -> srgbToLinear(rgb * alpha) -> linarToSrgb( c ) -> toUnmul( c/alpha ) -> Png
    
    
    Mipper mipper;

    // 1. srgb 8-bit values
    uint8_t alpha = 200;
    float alphaF = mipper.toAlphaFloat(alpha);

    uint8_t values1[256];
    uint8_t values2[256];
    uint8_t values3[256];

    for (int32_t i = 0; i < 256; ++i) {
        // premul and then snap back to store
        values1[i] = ((uint32_t)i * (uint32_t)alpha) / 255;
    }

    // now convert those values to linear color (float)
    for (int32_t i = 0; i < 256; ++i) {
        float value = mipper.toLinear(values1[i]);
        
        values2[i] = uint8_t(value * 255.1);
        
        //KLOGI("srgb", "[%d] = %g\n", i, value);
    }

    // convert srgb to linear and then do premul
    for (int32_t i = 0; i < 256; ++i) {
        float value = mipper.toLinear(i);
        value *= alphaF;
        
        values3[i] = uint8_t(value * 255.1);
    }

    // log them side-by-side for comparison
    KLOGI("srgb", "premul by %0.3f", 200.0/255.0);
    for (int32_t i = 0; i < 256; ++i) {
        KLOGI("srgb", "[%d] = %u, %u, %u",
              i, values1[i], values2[i], values3[i]);
    }
}


int32_t kramAppCommand(vector<const char*>& args)
{
    PSTest();
    
    // make sure next arg is a valid command type
    CommandType commandType = kCommandTypeUnknown;
    if (args.size() >= 1) {
        const char* command = args[0];
        commandType = parseCommandType(command);

        if (commandType == kCommandTypeUnknown) {
            KLOGE("Kram", "Unknown command\n");
        }
    }

    // remove command before parsing it
    switch (commandType) {
        case kCommandTypeEncode:
            args.erase(args.begin());
            return kramAppEncode(args);
        case kCommandTypeDecode:
            args.erase(args.begin());
            return kramAppDecode(args);
        case kCommandTypeInfo:
            args.erase(args.begin());
            return kramAppInfo(args);
        case kCommandTypeScript:
            args.erase(args.begin());
            return kramAppScript(args);
        default:
            break;
    }
    return -1;
}

// processes a test or single command
int32_t kramAppMain(int32_t argc, char* argv[])
{
    // copy into args, so setupTestArgs can completely replace them
    vector<const char*> args;

    for (int32_t i = 0; i < argc; ++i) {
        // skip first arg if contains the app name
        if (i == 0 && (strstr(argv[i], appName) != nullptr)) {
            continue;
        }
        args.push_back(argv[i]);
    }

    // help if not enough args on command line
    if (args.empty()) {
        kramUsage();
        return 0;
    }

    setupTestArgs(args);

    return kramAppCommand(args);
}

}  // namespace kram
