// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

//#include <string>
//#include <vector>

#include "KramConfig.h"

namespace kram {

using namespace NAMESPACE_STL;

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

#if SUPPORT_RGB
    // Can import files from KTX/KTX2 with RGB data, but convert right away to RGBA.
    // These are not export formats.  Watch alignment on these too.  These
    // have no MTLPixelFormat.
    MyMTLPixelFormatRGB8Unorm_internal = 200,
    MyMTLPixelFormatRGB8Unorm_sRGB_internal = 201,
    MyMTLPixelFormatRGB16Float_internal = 202,
    MyMTLPixelFormatRGB32Float_internal = 203,
#endif
};

enum MyMTLTextureType {
    // MyMTLTextureType1D = 0,   // not twiddled or compressed, more like a buffer but with texture limits
    MyMTLTextureType1DArray = 1,  // not twiddled or compressed, more like a buffer but with texture limits
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

constexpr int32_t kKTXIdentifierSize = 12;
extern const uint8_t kKTXIdentifier[kKTXIdentifierSize];
extern const uint8_t kKTX2Identifier[kKTXIdentifierSize];

class KTXHeader {
public:
    // Don't add any date to this class.  It's typically the top of a file cast to this.
    // As such, this doesn't have much functionality, other than to hold the header.

    // 64-byte header
    uint8_t identifier[kKTXIdentifierSize] = {
        // same is kKTXIdentifier
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

// This is one entire level of mipLevels.
// In KTX, the image levels are assumed from format and size since no compression applied.
//class KTXImageLevel {
//public:
//    uint64_t offset; // numChunks * length
//    uint64_t length; // size of a single mip
//};

//---------------------------------------------

// Mips are reversed from KTX1 (mips are smallest first for streaming),
// and this stores an array of supercompressed levels, and has dfds.
class KTX2Header {
public:
    uint8_t identifier[kKTXIdentifierSize] = {
        // same is kKTX2Identifier
        0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
        // '«', 'K', 'T', 'X', ' ', '2', '0', '»', '\r', '\n', '\x1A', '\n'
    };

    uint32_t vkFormat = 0;  // invalid format
    uint32_t typeSize = 1;

    uint32_t pixelWidth = 1;
    uint32_t pixelHeight = 0;
    uint32_t pixelDepth = 0;

    uint32_t layerCount = 0;
    uint32_t faceCount = 1;
    uint32_t levelCount = 1;
    uint32_t supercompressionScheme = 0;

    // Index

    // dfd block
    uint32_t dfdByteOffset = 0;
    uint32_t dfdByteLength = 0;

    // key-value
    uint32_t kvdByteOffset = 0;
    uint32_t kvdByteLength = 0;

    // supercompress global data
    uint64_t sgdByteOffset = 0;
    uint64_t sgdByteLength = 0;

    // chunks hold levelCount of all mips of the same size
    // KTXImageLevel* chunks; // [levelCount]
};

// Unlike KTX, KTX2 writes an array of level sizes since level compression may be used.
// Level compression is an entire compressed array of chunks at a given mip dimension.
// So then the entire level must be decompressed before a chunk can be accessed.
// This is one entire level of mipLevels.
//
// Use this for KTX, but there length == lengthCompressed, and the array is just a temporary.
// and the offsts include a 4 byte length at the start of each level.
class KTXImageLevel {
public:
    uint64_t offset = 0;            //  differ in ordering - ktx largest first, ktx2 smallest first
    uint64_t lengthCompressed = 0;  // set to 0 if not compresseds
    uint64_t length = 0;            // numChunks * mipSize when written for non cube on KTX1 or all KTX2, internally only stores mipSize
};

enum KTX2Supercompression {
    KTX2SupercompressionNone = 0,
    KTX2SupercompressionBasisLZ = 1,  // can transcode, but can't gen from KTX file using ktxsc, uses sgdByteLength
    KTX2SupercompressionZstd = 2,     // faster deflate, ktxsc support
    KTX2SupercompressionZlib = 3,     // deflate, no ktxsc support (use miniz)
    // TODO: Need LZFSE?
    // TODO: need Kraken for PS4
    // TODO: need Xbox format
};

struct KTX2Compressor {
    KTX2Supercompression compressorType = KTX2SupercompressionNone;
    float compressorLevel = 0.0f;  // 0.0 is default

    bool isCompressed() const { return compressorType != KTX2SupercompressionNone; }
};

//---------------------------------------------

// Since can't add anything to KTXHeader without throwing off KTXHeader size,
// this holds any mutable data for reading/writing KTX images.
class KTXImage {
public:
    // this calls init calls
    bool open(const uint8_t* imageData, size_t imageDataLength, bool isInfoOnly = false);

    void initProps(const uint8_t* propsData, size_t propDataSize);

    void initMipLevels(size_t mipOffset);
    void initMipLevels(bool doMipmaps, int32_t mipMinSize, int32_t mipMaxSize, int32_t mipSkip, uint32_t& numSkippedMips);

    bool validateMipLevels() const;

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
    uint32_t blockCountRows(uint32_t width_) const;

    // this is where KTXImage holds all mip data internally
    void reserveImageData();
    void reserveImageData(size_t totalSize);
    vector<uint8_t>& imageData();

    // for KTX2 files, the mips can be compressed using various encoders
    bool isSupercompressed() const { return isKTX2() && mipLevels[0].lengthCompressed != 0; }

    bool isKTX1() const { return !skipImageLength; }
    bool isKTX2() const { return skipImageLength; }

    // determine if image stores rgb * a
    bool isPremul() const;
    
    // can use on ktx1/2 files, does a decompress if needed
    bool unpackLevel(uint32_t mipNumber, const uint8_t* srcData, uint8_t* dstData) const;

    // helpers to work with the mipLevels array, mipLength and levelLength are important to get right
    // mip data depends on format

    // mip
    void mipDimensions(uint32_t mipNumber, uint32_t& width_, uint32_t& height_, uint32_t& depth_) const;
    uint32_t mipLengthCalc(uint32_t width_, uint32_t height_) const;
    uint32_t mipLengthCalc(uint32_t mipNumber) const;
    size_t mipLengthLargest() const { return mipLevels[0].length; }
    size_t mipLength(uint32_t mipNumber) const { return mipLevels[mipNumber].length; }

    // level
    size_t levelLength(uint32_t mipNumber) const { return mipLevels[mipNumber].length * totalChunks(); }
    size_t levelLengthCompressed(uint32_t mipNumber) const { return mipLevels[mipNumber].lengthCompressed; }

    // chunk
    uint32_t totalChunks() const;
    size_t chunkOffset(uint32_t mipNumber, uint32_t chunkNumber) const { return mipLevels[mipNumber].offset + mipLevels[mipNumber].length * chunkNumber; }

private:
    bool openKTX2(const uint8_t* imageData, size_t imageDataLength, bool isInfoOnly);

    // ktx2 mips are uncompressed to convert back to ktx1, but without the image offset
    vector<uint8_t> _imageData;

public:  // TODO: bury this
    MyMTLTextureType textureType = MyMTLTextureType2D;
    MyMTLPixelFormat pixelFormat = MyMTLPixelFormatInvalid;

    // copied out of header, but also may be 1 instead of 0
    // also these can be modified, and often are non-zero even if header is
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;

    // for ktx2
    bool skipImageLength = false;
    KTX2Supercompression supercompressionType = KTX2SupercompressionNone;

    KTXHeader header;  // copy of KTXHeader, so can be modified and then written back

    // write out only string/string props, for easy of viewing
    vector<pair<string, string> > props;

    vector<KTXImageLevel> mipLevels;  // offsets into fileData

    // this only holds data for mipLevels
    size_t fileDataLength = 0;
    const uint8_t* fileData = nullptr;  // mmap data
};

// GL/D3D hobbled non-pow2 mips by only supporting round down, not round up
// And then Metal followed OpenGL since it's the same hw and drivers.
// Round up adds an extra mip level to the chain, but results in much better filtering.
// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_non_power_of_two.txt
// http://download.nvidia.com/developer/Papers/2005/NP2_Mipmapping/NP2_Mipmap_Creation.pdf
inline void mipDown(int32_t& w, int32_t& h, int32_t& d, uint32_t lod = 1)
{
    // round-down
    w >>= (int32_t)lod;
    h >>= (int32_t)lod;
    d >>= (int32_t)lod;

    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (d < 1) d = 1;
}

inline void mipDown(uint32_t& w, uint32_t& h, uint32_t& d, uint32_t lod = 1)
{
    // round-down
    w >>= lod;
    h >>= lod;
    d >>= lod;

    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (d < 1) d = 1;
}

inline void KTXImage::mipDimensions(uint32_t mipNumber, uint32_t& width_, uint32_t& height_, uint32_t& depth_) const
{
    assert(mipNumber < mipLevels.size());

    width_ = width;
    height_ = height;
    depth_ = depth;

    mipDown(width_, height_, depth_, mipNumber);
}

const char* supercompressionName(KTX2Supercompression type);

// Generic format helpers.  All based on the ubiquitous type.
bool isFloatFormat(MyMTLPixelFormat format);
bool isHalfFormat(MyMTLPixelFormat format);
bool isHdrFormat(MyMTLPixelFormat format);
bool isSrgbFormat(MyMTLPixelFormat format);
bool isColorFormat(MyMTLPixelFormat format);
bool isAlphaFormat(MyMTLPixelFormat format);
bool isSignedFormat(MyMTLPixelFormat format);

bool isBCFormat(MyMTLPixelFormat format);
bool isETCFormat(MyMTLPixelFormat format);
bool isASTCFormat(MyMTLPixelFormat format);
bool isBlockFormat(MyMTLPixelFormat format);
bool isExplicitFormat(MyMTLPixelFormat format);

Int2 blockDimsOfFormat(MyMTLPixelFormat format);
uint32_t blockSizeOfFormat(MyMTLPixelFormat format);
uint32_t numChannelsOfFormat(MyMTLPixelFormat format);

const char* formatTypeName(MyMTLPixelFormat format);

// metal
uint32_t metalType(MyMTLPixelFormat format);  // really MTLPixelFormat
const char* metalTypeName(MyMTLPixelFormat format);

// vuklan
const char* vulkanTypeName(MyMTLPixelFormat format);
uint32_t vulkanType(MyMTLPixelFormat format);           // really VKFormat
MyMTLPixelFormat vulkanToMetalFormat(uint32_t format);  // really VKFormat

// gl
const char* glTypeName(MyMTLPixelFormat format);
uint32_t glType(MyMTLPixelFormat format);
MyMTLPixelFormat glToMetalFormat(uint32_t format);

const char* textureTypeName(MyMTLTextureType textureType);

// find a corresponding srgb/non-srgb format for a given format
MyMTLPixelFormat toggleSrgbFormat(MyMTLPixelFormat format);

}  // namespace kram
