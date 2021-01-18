// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#include <string>
#include <vector>

#include "KramConfig.h"

namespace kram {

using namespace std;

// TODO: abstract MyMTLPixelFormat and move to readable/neutral type
enum MyMTLPixelFormat {
    // Note: bc6/7 aren't supported by squish
    // ATE in Xcode 12 added bc1/3/4/5/7.
    MyMTLPixelFormatInvalid = 0,

    //---------
    // bc
    MyMTLPixelFormatBC1_RGBA = 130,
    MyMTLPixelFormatBC1_RGBA_sRGB = 131,

    // MyMTLPixelFormatBC2_RGBA             = 132,
    // MyMTLPixelFormatBC2_RGBA_sRGB        = 133,

    MyMTLPixelFormatBC3_RGBA = 134,
    MyMTLPixelFormatBC3_RGBA_sRGB = 135,

    MyMTLPixelFormatBC4_RUnorm = 140,
    MyMTLPixelFormatBC4_RSnorm = 141,

    MyMTLPixelFormatBC5_RGUnorm = 142,
    MyMTLPixelFormatBC5_RGSnorm = 143,

    MyMTLPixelFormatBC6H_RGBFloat = 150,
    MyMTLPixelFormatBC6H_RGBUfloat = 151,

    MyMTLPixelFormatBC7_RGBAUnorm = 152,
    MyMTLPixelFormatBC7_RGBAUnorm_sRGB = 153,

    //---------
    // astc - 16, 25, 35, 64px blocks
    MyMTLPixelFormatASTC_4x4_sRGB = 186,
    MyMTLPixelFormatASTC_4x4_LDR = 204,
    MyMTLPixelFormatASTC_4x4_HDR = 222,

    MyMTLPixelFormatASTC_5x5_sRGB = 188,
    MyMTLPixelFormatASTC_5x5_LDR = 206,
    MyMTLPixelFormatASTC_5x5_HDR = 224,

    MyMTLPixelFormatASTC_6x6_sRGB = 190,
    MyMTLPixelFormatASTC_6x6_LDR = 208,
    MyMTLPixelFormatASTC_6x6_HDR = 226,

    MyMTLPixelFormatASTC_8x8_sRGB = 194,
    MyMTLPixelFormatASTC_8x8_LDR = 212,
    MyMTLPixelFormatASTC_8x8_HDR = 230,

    //---------
    // etc2
    MyMTLPixelFormatEAC_R11Unorm = 170,
    MyMTLPixelFormatEAC_R11Snorm = 172,
    MyMTLPixelFormatEAC_RG11Unorm = 174,
    MyMTLPixelFormatEAC_RG11Snorm = 176,
    MyMTLPixelFormatETC2_RGB8 = 180,
    MyMTLPixelFormatETC2_RGB8_sRGB = 181,
    MyMTLPixelFormatEAC_RGBA8 = 178,
    MyMTLPixelFormatEAC_RGBA8_sRGB = 179,

    // not supporting
    //    MyMTLPixelFormatETC2_RGB8A1            = 182,
    //    MyMTLPixelFormatETC2_RGB8A1_sRGB       = 183,

    // ------
    // Explicit formats
    MyMTLPixelFormatR8Unorm = 10,
    MyMTLPixelFormatRG8Unorm = 30,
    MyMTLPixelFormatRGBA8Unorm = 70,

    // TODO: support these
    // MyMTLPixelFormatR8Unorm_sRGB = 11,
    // MyMTLPixelFormatRG8Unorm_sRGB= 31,
    MyMTLPixelFormatRGBA8Unorm_sRGB = 71,

    // MyMTLPixelFormatR8Snorm      = 12,
    // MyMTLPixelFormatRG8Snorm     = 32,
    // MyMTLPixelFormatRGBA8Snorm      = 72,

    // TODO: also BGRA8Unorm types?

    MyMTLPixelFormatR16Float = 25,
    MyMTLPixelFormatRG16Float = 65,
    MyMTLPixelFormatRGBA16Float = 115,

    MyMTLPixelFormatR32Float = 55,
    MyMTLPixelFormatRG32Float = 105,
    MyMTLPixelFormatRGBA32Float = 125,

    // TODO: also need rgb9e5 for fallback if ASTC HDR/6H not supported
    // That is Unity's fallback if alpha not needed, otherwise RGBA16F.
};

enum MyMTLTextureType {
    // MyMTLTextureType1D = 0,
    MyMTLTextureType1DArray = 1,
    MyMTLTextureType2D = 2,
    MyMTLTextureType2DArray = 3,
    // MyMTLTextureType2DMultisample = 4,
    MyMTLTextureTypeCube = 5,
    MyMTLTextureTypeCubeArray,
    MyMTLTextureType3D = 7,
    // MyMTLTextureType2DMultisampleArray = 8,
    // MyMTLTextureTypeTextureBuffer = 9
};

struct Int2 {
    int x, y;
};

//---------------------------------------------

class KTXHeader {
public:
    // Don't add any date to this class.  It's typically the top of a file cast to this.
    // As such, this doesn't have much functionality, other than to hold the header.

    // 64-byte header
    uint8_t identifier[12] = { // same is kKTXIdentifier
        0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
        //'«', 'K', 'T', 'X', ' ', '1', '1', '»', '\r', '\n', '\x1A', '\n'
    };

    uint32_t endianness = 0x04030201;
    uint32_t glType = 0;      // compressed = 0
    uint32_t glTypeSize = 1;  // doesn't depend on endianness

    uint32_t glFormat = 0;
    uint32_t glInternalFormat = 0;      // must be same as glFormat
    uint32_t glBaseInternalFormat = 0;  // GL_RED, RG, RGB, RGBA, SRGB, SRGBA

    uint32_t pixelWidth = 1;
    uint32_t pixelHeight = 0;  // >0 for 2d
    uint32_t pixelDepth = 0;   // >0 for 3d

    uint32_t numberOfArrayElements = 0;
    uint32_t numberOfFaces = 1;
    uint32_t numberOfMipmapLevels = 1;  // 0 means auto mip

    uint32_t bytesOfKeyValueData = 0;

public:
    // depth * faces * array
    uint32_t totalChunks() const;

    // this has gaps in the mapping for ASTC HDR
    void initFormatGL(MyMTLPixelFormat pixelFormat);

    MyMTLTextureType metalTextureType() const;

    // only use this if prop KTXmetalFormat not written out
    MyMTLPixelFormat metalFormat() const;
};

//---------------------------------------------

// This is one entire level of mipLevels.
class KTXImageLevel {
public:
    uint64_t offset; // numChunks * length
    uint64_t length; // size of a single mip
};

// Since can't add anything to KTXHeader without throwing off KTXHeader size,
// this holds any mutable data for reading/writing KTX images.
class KTXImage {
public:
    // this calls init calls
    bool open(const uint8_t* imageData, size_t imageDataLength);
    
    void initProps();
    bool initMipLevels(bool validateLevelSizeFromRead);

    // props handling
    void toPropsData(vector<uint8_t>& propsData);

    string getProp(const char* name) const;
    void addProp(const char* name, const char* value);

    void addFormatProps();
    void addSwizzleProps(const char* swizzleTextPre, const char* swizzleTexPost);
    void addSourceHashProps(uint32_t sourceHash);
    void addChannelProps(const char* channelContent);
    void addAddressProps(const char* addressContent);
    void addFilterProps(const char* filterContent);

    // block data depends on format
    uint32_t blockSize() const;
    Int2 blockDims() const;
    uint32_t blockCount(uint32_t width_, uint32_t height_) const;

    // mip data depends on format
    uint32_t mipLevelSize(uint32_t width_, uint32_t height_) const;
    //int totalMipLevels() const;
    uint32_t totalChunks() const;

private:
    bool openKTX2(const uint8_t* imageData, size_t imageDataLength);

    // ktx2 mips are uncompressed to convert back to ktx1, but without the image offset
    vector<uint8_t> imageDataFromKTX2;
    
public:  // TODO: bury this
    MyMTLTextureType textureType = MyMTLTextureType2D;
    MyMTLPixelFormat pixelFormat = MyMTLPixelFormatInvalid;

    // copied out of header, but also may be 1 instead of 0
    // also these can be modified, and often are non-zero even if header is
    uint32_t width;
    uint32_t height;
    uint32_t depth;

    // for ktxa and ktx2
    bool skipImageLength = false;

    KTXHeader header;  // copy of KTXHeader, so can be modified and then written back

    // write out only string/string props, for easy of viewing
    vector<pair<string, string>> props;

    vector<KTXImageLevel> mipLevels;  // offsets into fileData

    // this only holds data for mipLevels
    size_t fileDataLength;
    const uint8_t* fileData;  // mmap data
};

// Generic format helpers.  All based on the ubiquitous type.
bool isFloatFormat(MyMTLPixelFormat format);
bool isHdrFormat(MyMTLPixelFormat format);
bool isSrgbFormat(MyMTLPixelFormat format);
bool isColorFormat(MyMTLPixelFormat format);
bool isAlphaFormat(MyMTLPixelFormat format);
bool isSignedFormat(MyMTLPixelFormat format);

bool isBCFormat(MyMTLPixelFormat format);
bool isETCFormat(MyMTLPixelFormat format);
bool isASTCFormat(MyMTLPixelFormat format);

Int2 blockDimsOfFormat(MyMTLPixelFormat format);
uint32_t blockSizeOfFormat(MyMTLPixelFormat format);
uint32_t numChannelsOfFormat(MyMTLPixelFormat format);

// metal
uint32_t metalType(MyMTLPixelFormat format);  // really MTLPixelFormat
const char* metalTypeName(MyMTLPixelFormat format);

// vuklan
const char* vulkanTypeName(MyMTLPixelFormat format);
uint32_t vulkanType(MyMTLPixelFormat format);  // really VKFormat
MyMTLPixelFormat vulkanToMetalFormat(uint32_t format); // really VKFormat

// gl
const char* glTypeName(MyMTLPixelFormat format);
uint32_t glType(MyMTLPixelFormat format);
MyMTLPixelFormat glToMetalFormat(uint32_t format);

const char* textureTypeName(MyMTLTextureType textureType);

// find a corresponding srgb/non-srgb format for a given format
MyMTLPixelFormat toggleSrgbFormat(MyMTLPixelFormat format);

}  // namespace kram
