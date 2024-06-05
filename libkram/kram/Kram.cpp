// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "Kram.h"

#include <sys/stat.h>

//#include <atomic>
#include <inttypes.h>

#include <cmath>
#include <ctime>
//#include <algorithm>  // for max
//#include <string>
//#include <vector>

#include "KTXImage.h"
#include "KramDDSHelper.h"
#include "KramFileHelper.h"
#include "KramImage.h"  // has config defines, move them out
#include "KramMmapHelper.h"
#include "KramTimer.h"
#define KRAM_VERSION "1.0"
//#include "KramVersion.h"
#include "TaskSystem.h"
#include "lodepng.h"
#include "miniz.h"

#ifndef USE_LIBCOMPRESSION
#define USE_LIBCOMPRESSION 0 // (KRAM_MAC || KRAM_IOS)
#endif

#if USE_LIBCOMPRESSION
#include <compression.h>
#endif

// one .cpp must supply these new overrides
#if USE_EASTL
void* __cdecl operator new[](size_t size, const char* name, int flags, unsigned debugFlags, const char* file, int line)
{
    return new uint8_t[size];
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return new uint8_t[size];  // TODO: honor alignment
}

#endif

//--------------------------------------

// name change on Win
#if KRAM_WIN
#define strtok_r strtok_s
#endif

namespace kram {

using namespace NAMESPACE_STL;

// This fails with libCompression (see inter-a.png)
// and with miniZ for the ICCP block (see inter-a.png)
// lodepng passes 16K to the custom zlib decompress, but
// the data read isn't that big.
static bool useMiniZ = false;

template <typename T>
void releaseVector(vector<T>& v)
{
    v.clear();
    v.shrink_to_fit();
}

bool isKTXFilename(const char* filename)
{
    // should really look at first 4 bytes of data
    return endsWithExtension(filename, ".ktx");
}
bool isKTX2Filename(const char* filename)
{
    // should really look at first 4 bytes of data
    return endsWithExtension(filename, ".ktx2");
}
bool isDDSFilename(const char* filename)
{
    // should really look at first 4 bytes of data
    return endsWithExtension(filename, ".dds");
}
bool isPNGFilename(const char* filename)
{
    // should really look at first 4 bytes of data
    return endsWithExtension(filename, ".png");
}

static bool isDDSFile(const uint8_t* data, size_t dataSize)
{
    // read the 4 chars at the beginning of the file
    const uint32_t numChars = 4;
    if (dataSize < numChars)
        return false;

    const uint8_t kDdsSignature[numChars] = {'D', 'D', 'S', ' '};
    if (memcmp(data, kDdsSignature, sizeof(kDdsSignature)) != 0) {
        return false;
    }

    return true;
}

static bool isPNGFile(const uint8_t* data, size_t dataSize)
{
    // read the 4 chars at the beginning of the file
    const uint32_t numChars = 8;
    if (dataSize < numChars)
        return false;

    const uint8_t kPngSignature[numChars] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (memcmp(data, kPngSignature, sizeof(kPngSignature)) != 0) {
        return false;
    }

    return true;
}

// this aliases the existing string, so can't chop extension
inline const char* toFilenameShort(const char* filename)
{
    const char* filenameShort = strrchr(filename, '/');
    if (filenameShort == nullptr) {
        filenameShort = filename;
    }
    else {
        filenameShort += 1;
    }
    return filenameShort;
}

bool KTXImageData::open(const char* filename, KTXImage& image, bool isInfoOnly_)
{
    isInfoOnly = isInfoOnly_;

    close();

    // set name from filename
    _name = toFilenameShort(filename);

    if (isPNGFilename(filename)) {
       bool success = openPNG(filename, image);
        
        if (success)
            fixPixelFormat(image, filename);
        
        return success;
    }

    isMmap = true;
    if (!mmapHelper.open(filename)) {
        isMmap = false;

        // open file, copy it to memory, then close it
        FileHelper fileHelper;
        if (!fileHelper.open(filename, "rb")) {
            return false;
        }

        // read the file into memory
        size_t size = fileHelper.size();
        if (size == (size_t)-1) {
            return false;
        }

        fileData.resize(size);
        if (!fileHelper.read(fileData.data(), size)) {
            return false;
        }
    }

    const uint8_t* data;
    size_t dataSize;
    if (isMmap) {
        data = mmapHelper.data();
        dataSize = mmapHelper.dataLength();
    }
    else {
        data = fileData.data();
        dataSize = fileData.size();
    }

    bool isLoaded = true;

    if (isDDSFile(data, dataSize)) {
        DDSHelper ddsHelper;
        isLoaded = ddsHelper.load(data, dataSize, image, isInfoOnly);
    }
    else {
        // read the KTXImage in from the data, it will alias mmap or fileData
        isLoaded = image.open(data, dataSize, isInfoOnly);
    }

    // this means KTXImage is using it's own storage
    if (!isLoaded || image.fileData != data) {
        close();
    }

    if (!isLoaded) {
        return false;
    }

    return true;
}

void KTXImageData::close()
{
    // don't need these anymore, singleImage holds the data
    mmapHelper.close();
    releaseVector(fileData);
    isMmap = false;
}

bool KTXImageData::openPNG(const char* filename, KTXImage& image)
{
    //close();

    isMmap = true;
    if (!mmapHelper.open(filename)) {
        isMmap = false;

        // open file, copy it to memory, then close it
        FileHelper fileHelper;
        if (!fileHelper.open(filename, "rb")) {
            return false;
        }

        // read the file into memory
        size_t size = fileHelper.size();
        if (size == (size_t)-1) {
            return false;
        }

        fileData.resize(size);
        if (!fileHelper.read(fileData.data(), size)) {
            return false;
        }
    }

    const uint8_t* data;
    size_t dataSize;
    if (isMmap) {
        data = mmapHelper.data();
        dataSize = mmapHelper.dataLength();
    }
    else {
        data = fileData.data();
        dataSize = fileData.size();
    }

    return openPNG(data, dataSize, image);
}

bool KTXImageData::openPNG(const uint8_t* data, size_t dataSize, KTXImage& image)
{
    // This is returned by LoadPng.  Note that many png have this set
    // by default and not controllable by artists.
    bool isSrgb = false;
    
    Image singleImage;
    bool isLoaded = LoadPng(data, dataSize, false, false, isSrgb, singleImage);
    
    // don't need png data anymore
    close();

    if (!isLoaded) {
        return false;
    }

    // now move the png pixels into the KTXImage

    image.width = singleImage.width();
    image.height = singleImage.height();
    image.depth = 0;

    image.header.pixelWidth = image.width;
    image.header.pixelHeight = image.height;
    image.header.pixelDepth = image.depth;
    image.header.numberOfArrayElements = 0;
    image.header.numberOfMipmapLevels = 1;

    image.textureType = MyMTLTextureType2D;
    image.pixelFormat = isSrgb ? MyMTLPixelFormatRGBA8Unorm_sRGB : MyMTLPixelFormatRGBA8Unorm;

    // TODO: support mips with blitEncoder but that confuses mipCount in KTXImage
    //     Mipper can also generate on cpu side.  Mipped can do premul conversion though.

    // TODO: support chunks and striped png, but may need to copy horizontal to vertical

    // TODO: png has 16u format useful for heights

    image.initMipLevels(sizeof(KTXHeader));  // TODO: could also make this ktx2 with zstd compress
    image.reserveImageData();
    memcpy((uint8_t*)image.fileData, &image.header, sizeof(KTXHeader));

    memcpy((uint8_t*)image.fileData + image.mipLevels[0].offset, singleImage.pixels().data(), image.levelLength(0));

    return true;
}

bool KTXImageData::open(const uint8_t* data, size_t dataSize, KTXImage& image, bool isInfoOnly_)
{
    isInfoOnly = isInfoOnly_;
    close();

    if (isPNGFile(data, dataSize)) {
        // data stored in image
        return openPNG(data, dataSize, image);  // TODO: pass isInfoOnly
    }
    else if (isDDSFile(data, dataSize)) {
        // converts dds to ktx, data stored in image
        // Note: unlike png, this data may already be block encoded
        DDSHelper ddsHelper;
        return ddsHelper.load(data, dataSize, image, isInfoOnly);
    }

    // image will likely alias incoming data, so KTXImageData is unused

    if (!image.open(data, dataSize, isInfoOnly)) {
        return false;
    }
    return true;
}

// decoding reads a ktx file into KTXImage (not Image)
bool SetupSourceKTX(KTXImageData& srcImageData,
                    const string& srcFilename,
                    KTXImage& sourceImage,
                    bool isInfoOnly)
{
    if (!srcImageData.open(srcFilename.c_str(), sourceImage, isInfoOnly)) {
        KLOGE("Kram", "File input \"%s\" could not be opened for read.\n",
              srcFilename.c_str());
        return false;
    }
    return true;
}

// Twiddle pixels or blocks into Morton order.  Usually this is done during the upload of
// linear-order block textures.  But on some platforms may be able to directly use the block
// and pixel data if organized in the exact twiddle order the hw uses.
// Code adapted from KTX doc example.
class MortonOrder {
public:
    MortonOrder(uint32_t width, uint32_t height)
    {
        minDim = (width <= height) ? width : height;

        // Smaller size must be a power of 2
        assert((minDim & (minDim - 1)) == 0);

        // Larger size must be a multiple of the smaller
        assert(width % minDim == 0 && height % minDim == 0);
    }

    // For a given xy block in a mip level, find the block offset in morton order
    uint32_t mortonOffset(uint32_t x, uint32_t y)
    {
        uint32_t offset = 0, shift = 0;

        for (uint32_t mask = 1; mask < minDim; mask <<= 1) {
            offset |= (((y & mask) << 1) | (x & mask)) << shift;
            shift++;
        }

        // At least one of width and height will have run out of most-significant bits
        offset |= ((x | y) >> shift) << (shift * 2);
        return offset;
    }

private:
    uint32_t minDim = 0;
};

// rec709
// https://en.wikipedia.org/wiki/Grayscale
inline Color toGrayscaleRec709(Color c, const Mipper& mipper)
{
    const float4 kRec709Conversion = float4m(0.2126f, 0.7152f, 0.0722f, 0.0f);  // really a float3

    // convert to linear, do luminance, then back to srgb primary

    float4 clin = mipper.toLinear(c);
    float luminance = dot(clin, kRec709Conversion);
    luminance = std::min(luminance, 1.0f);  // to avoid assert if math goes above 1.0

    c.r = (uint8_t)(roundf(linearToSRGBFunc(luminance) * 255.0f));

    // can just copy into the other 3 terms
    c.g = c.b = c.r;
    return c;
}

bool LoadKtx(const uint8_t* data, size_t dataSize, Image& sourceImage)
{
    KTXImage image;

    // don't decompress pixel data, only going to unpack top level mip on KTX2
    // this stil aliases the incoming pixel data
    bool isInfoOnly = true;

    if (!image.open(data, dataSize, isInfoOnly)) {
        return false;
    }

    // this loads the top level into the sourceImage, caller must set chunkY to totalChunks
    return sourceImage.loadImageFromKTX(image);
}

// wrap miniz decompress, since it ignores crc checksum and is faster than default png
unsigned LodepngDecompressUsingMiniz(
    unsigned char** dstData, size_t* dstDataSize,
    const unsigned char* srcData, size_t srcDataSize,
    const LodePNGDecompressSettings* settings)
{
    // mz_ulong doesn't line up with size_t on Windows, but does on macOS
    KASSERT(*dstDataSize != 0);
    
#if USE_LIBCOMPRESSION
    // this returns 121 dstSize instead of 16448 on 126 srcSize.  
    // Open src dir to see this.  Have to advance by 2 to fix this.
    if (srcDataSize <= 2) {
        return MZ_DATA_ERROR;
    }
    
    char scratchBuffer[compression_decode_scratch_buffer_size(COMPRESSION_ZLIB)];
    size_t bytesDecoded = compression_decode_buffer(
         (uint8_t*)*dstData, *dstDataSize,
         (const uint8_t*)srcData + 2, srcDataSize - 2,
        scratchBuffer, // scratch-buffer that could speed up to pass
         COMPRESSION_ZLIB);
    
    int result = MZ_OK;
    if (bytesDecoded != *dstDataSize) {
        result = MZ_DATA_ERROR;
        *dstDataSize = 0;
    }
#else
    // This works.
    mz_ulong bytesDecoded = *dstDataSize;
    int result = mz_uncompress(*dstData, &bytesDecoded,
                               srcData, srcDataSize);
    
    if (result != MZ_OK || bytesDecoded != *dstDataSize) {
        *dstDataSize = 0;
    }
    else {
        *dstDataSize = bytesDecoded;
    }
#endif

    return result;
}

// wrap miniz compress
unsigned LodepngCompressUsingMiniz(
    unsigned char** dstData, size_t* dstDataSize,
    const unsigned char* srcData, size_t srcDataSize,
    const LodePNGCompressSettings* settings)
{
    // TODO: no setting for compression level in settings?
    // TODO: libCompression can only encode zlib to quality 5
    
    // mz_ulong doesn't line up with size_t on Windows, but does on macOS
    mz_ulong dstDataSizeUL = *dstDataSize;

    int result = mz_compress2(*dstData, &dstDataSizeUL,
                               srcData, srcDataSize, MZ_DEFAULT_COMPRESSION);

    *dstDataSize = dstDataSizeUL;
    
    return result;
}


//-----------------------

// TODO: fix this to identify srgb, otherwise will skip GAMA block
// no docs on how to identify srgb from iccp, ImageMagick might
// have code for this.˜
static const bool doParseIccProfile = false;

struct IccProfileTag
{
    uint32_t type, offset, size;
};

static void swapEndianUint32(uint32_t& x) {
    x =  ((x << 24) & 0xff000000 ) |
         ((x <<  8) & 0x00ff0000 ) |
         ((x >>  8) & 0x0000ff00 ) |
         ((x >> 24) & 0x000000ff );
}

// https://github.com/lvandeve/lodepng/blob/master/pngdetail.cpp
static int getICCInt32(const unsigned char* icc, size_t size, size_t pos) {
  if (pos + 4 > size) return 0;
    
  // this is just swapEndianUint32 in byte form
  return (int)((icc[pos] << 24) | (icc[pos + 1] << 16) | (icc[pos + 2] << 8) | (icc[pos + 3] << 0));
}

static float getICC15Fixed16(const unsigned char* icc, size_t size, size_t pos) {
  return getICCInt32(icc, size, pos) / 65536.0;
}

// this is all big-endian, so needs swapped, 132 bytes total
struct IccProfileHeader
{
    uint32_t size; // 0
    uint32_t cmmType; // 4 - 'appl'
    uint32_t version; // 8
    uint32_t deviceClass; // 12 - 'mntr'
    uint32_t inputSpace; // 16 - 'RGB '
    uint32_t outputSpace; // 20 - 'XYZ '
    uint16_t date[6]; // 24
    uint32_t signature; // 30 - 'ascp'
    uint32_t platform; // 32 - 'APPL'
    uint32_t flags; // 36
    uint32_t deviceManufacturer; // 40 - 'APPL'
    uint32_t deviceModel;
    uint32_t deviceAttributes[2]; // 48
    uint32_t renderingIntent; // 64
    uint32_t psx, psy, psz; // 68 - fixed-point float, illuminant
    uint32_t creator; // 80 - 'appl'
    uint32_t md5[4]; // 84
    uint32_t padding[7]; // 100
    uint32_t numTags; // 128
};
static_assert( sizeof(IccProfileHeader) == 132, "invalid IccProfileHeader");

#define MAKEFOURCC(str)                                                       \
    ((uint32_t)(uint8_t)(str[0]) | ((uint32_t)(uint8_t)(str[1]) << 8) |       \
    ((uint32_t)(uint8_t)(str[2]) << 16) | ((uint32_t)(uint8_t)(str[3]) << 24 ))


// this must be run after deflate if profile is compressed
bool parseIccProfile(const uint8_t* data, uint32_t dataSize, bool& isSrgb)
{
    isSrgb = false;
    
    // should look at other blocks if this is false
    if (dataSize < sizeof(IccProfileHeader)) {
        return false;
    }
    
    // copy header so can endianSwap it
    IccProfileHeader header = *(const IccProfileHeader*)data;
    // convert big to little endian
    swapEndianUint32(header.size);
    swapEndianUint32(header.numTags);
    
    if (header.signature != MAKEFOURCC("acsp")) {
        return false;
    }

    if (header.deviceModel == MAKEFOURCC("sRGB")) {
        isSrgb = true;
        return true;
    }
    
    IccProfileTag* tags = (IccProfileTag*)(data + sizeof(IccProfileHeader));

    for (uint32_t i = 0; i < header.numTags; ++i) {
        IccProfileTag tag = tags[i];
        swapEndianUint32(tag.offset);
        swapEndianUint32(tag.size);
        
        // There's also tag.name which is 'wtpt' and others.
        // Open a .icc profile to see all these names
        
        uint32_t datatype = *(const uint32_t*)(data + tag.offset);
        
        switch(datatype) {
            case MAKEFOURCC("XYZ "): {
                if (tag.type == MAKEFOURCC("wtpt")) {
                    float x = getICC15Fixed16(data, dataSize, tag.offset + 8);
                    float y = getICC15Fixed16(data, dataSize, tag.offset + 12);
                    float z = getICC15Fixed16(data, dataSize, tag.offset + 16);

                    // types include rXYZ, gXYZ, bXYZ, wtpt,
                    // can srgb be based on white-point, seems cheesy
                    // Media hitepoint trisimulus
                    if (x == 0.950454711f && y == 1.0f && z == 1.08905029f) {
                        isSrgb = true;
                        return true;
                    }
                }
                break;
            }
            case MAKEFOURCC("curv"):
                // rTRC, gTRC, bTRC
                break;
            case MAKEFOURCC("para"):
                // aarg, aagg, aabg,
                break;
            case MAKEFOURCC("vcgt"):
                // vcgt,
                break;
            case MAKEFOURCC("vcgp"):
                // vcgp,
                break;
            case MAKEFOURCC("mmod"):
                // mmod,
                break;
            case MAKEFOURCC("ndin"):
                // ndin,
                break;
            case MAKEFOURCC("chrm"):
                break;
            case MAKEFOURCC("sf32"):
                // chad - chromatic adaptation matrix
                break;
                
            case MAKEFOURCC("mAB "):
                // A2B0, A2B1 - Intent-0/1, device to PCS table
            case MAKEFOURCC("mBA "):
                // B2A0, B2A1 - Intent-0/1, PCS to device table
            case MAKEFOURCC("sig "):
                // rig0
                
            case MAKEFOURCC("text"):
            case MAKEFOURCC("mluc"):
                // muti-localizaed description strings
            case MAKEFOURCC("desc"):
                // cprt, dscm
                // desc/desc or mluc/desc
                // copyright and en description
                break;
        }
    }
    
    return true;
}
   
bool isIccProfileSrgb(const uint8_t* data, uint32_t dataSize) {
    bool isSrgb = false;
    parseIccProfile(data, dataSize, isSrgb);
    return isSrgb;
}

//-----------------------

bool LoadPng(const uint8_t* data, size_t dataSize, bool isPremulRgb, bool isGray, bool& isSrgb, Image& sourceImage)
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t errorLode = 0;

    // Point deflate on decoder to faster version in miniz.
    auto& settings = lodepng_default_decompress_settings;
    if (useMiniZ)
        settings.custom_zlib = LodepngDecompressUsingMiniz;

    // can identify 16unorm data for heightmaps via this call
    LodePNGState state;
    lodepng_state_init(&state);
    state.decoder.ignore_crc = 1;

    // this doesn't look at any blocks including srgb, can only get those from decode
    errorLode = lodepng_inspect(&width, &height, &state, data, dataSize);
    if (errorLode != 0) {
        return false;
    }

    isSrgb = false;
   
    // Stop at the idat, or if not present the end of the file
    const uint8_t* end = lodepng_chunk_find_const(data, data + dataSize, "IDAT");
    if (!end)
        end = data + dataSize;
    
    bool hasNonSrgbBlocks = false;
    bool hasSrgbBlock = false;
    {
        // Apps like Photoshop never set sRGB block
        hasNonSrgbBlocks =
            lodepng_chunk_find_const(data, end, "iCCP") != nullptr ||
            lodepng_chunk_find_const(data, end, "gAMA") != nullptr ||
            lodepng_chunk_find_const(data, end, "cHRM") != nullptr;
        
        // Apps like Figma always set this
        hasSrgbBlock = lodepng_chunk_find_const(data, end, "sRGB") != nullptr;
    }
    
    const uint8_t* chunkData = lodepng_chunk_find_const(data, end, "sRGB");
    if (chunkData) {
        lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
        isSrgb = state.info_png.srgb_defined;
        //state.info_png.srgb_intent; // 0-3
    }
    
    if (doParseIccProfile && !chunkData) {
        chunkData = lodepng_chunk_find_const(data, end, "iCCP");
        if (chunkData) {
            lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
            if (state.info_png.iccp_defined) {
                if (!isSrgb)
                    isSrgb = isIccProfileSrgb(state.info_png.iccp_profile, state.info_png.iccp_profile_size);
            }
                
        }
    }
    
    if (!chunkData) {
        chunkData = lodepng_chunk_find_const(data, end, "gAMA");
        if (chunkData) {
            lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
            if (state.info_png.gama_defined) {
                if (!isSrgb)
                    isSrgb = state.info_png.gama_gamma == 45455; // 1/2.2 x 100000
            }
                
        }
    }
    
    if (!chunkData) {
        chunkData = lodepng_chunk_find_const(data, end, "cHRM");
        if (chunkData) {
            lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
            if (state.info_png.chrm_defined) {
                if (!isSrgb)
                    isSrgb =
                        state.info_png.chrm_red_x == 64000 &&
                        state.info_png.chrm_red_y == 33000 &&
                        state.info_png.chrm_green_x == 30000 &&
                        state.info_png.chrm_green_y == 60000 &&
                        state.info_png.chrm_blue_x == 15000 &&
                        state.info_png.chrm_blue_y == 6000 &&
                        state.info_png.chrm_white_x == 31720 &&
                        state.info_png.chrm_white_y == 32900;
            }
        }
    }
    
    // because Apple finder thumbnails can't be overridden with custom thumbanailer
    // and defaults to white bkgd (making white icons impossible to see).
    // track the bkgd block, and set/re-define as all black.  Maybe will honor that.
    bool hasBackground = false;
    bool hasBlackBackground = false;
    chunkData = lodepng_chunk_find_const(data, data + dataSize, "bKGD");
    if (chunkData) {
        lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
        if (state.info_png.background_defined) {
            hasBackground = true;
            hasBlackBackground =
                state.info_png.background_r == 0 && // gray/pallete uses this only
                state.info_png.background_g == 0 &&
                state.info_png.background_b == 0;
        }
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
    vector<uint8_t> pixelsPNG;
    pixelsPNG.clear();
    errorLode = lodepng::decode(pixelsPNG, width, height, data, dataSize, LCT_RGBA, 8);
    if (errorLode != 0) {
        return false;
    }

    // Note: could probably do a cast of vector<uint8_t> to vector<Color>, but do a copy here instead
    vector<Color> pixels;
    pixels.resize(width * height);
    memcpy(pixels.data(), pixelsPNG.data(), vsizeof(pixelsPNG));

    // convert to grasycale on load
    // better if could do this later in pipeline to stay in linear fp16 color
    if (hasColor && isGray) {
        Mipper mipper;

        Color* colors = pixels.data();
        for (int32_t i = 0, iEnd = width * height; i < iEnd; ++i) {
            colors[i] = toGrayscaleRec709(colors[i], mipper);
        }

        hasColor = false;
    }

    // apply premul srgb right away, don't use with -premul or alpha is applied twice
    // this may throw off the props.  Note this ignores srgb conversion.
    // This is hack to look like Photoshop and Apple Preview, where they process srgb wrong
    // on premul PNG data on load, and colors look much darker.

    if (hasAlpha && isPremulRgb) {
        Color* colors = pixels.data();
        for (int32_t i = 0, iEnd = width * height; i < iEnd; ++i) {
            colors[i] = toPremul(colors[i]);
        }
    }

    sourceImage.setSrgbState(isSrgb, hasSrgbBlock, hasNonSrgbBlocks);
    sourceImage.setBackgroundState(hasBlackBackground);
    
    return sourceImage.loadImageFromPixels(pixels, width, height, hasColor, hasAlpha);
}

// Use this to fix the src png, will only have a single block with srgb or not.
// Can then run ImageOptim on it, with block preservation set.
// Need this since Photoshop refuses to save the srgb flag, and stuff a giant
// ICCP block which isn't easy to parse, and a Gama/Chrm block which is easier.
// Really just want all png to either specify srgb block or not.  Don't need other
// blocks.
bool SavePNG(Image& image, const char* filename)
{
    // TODO: would be nice to skip this work if the blocks are already
    // removed (could detect no iccp/chrm/gama in src png, and only srgb or no).
    // Then if srgb, see if that matches content type srgb state below.
    TexContentType contentType = findContentTypeFromFilename(filename);
    bool isSrgb = contentType == TexContentTypeAlbedo;
    
    // Skip file if it has srgb block, and none of the other block types.
    // This code will also strip the sRGB block from apps like Figma that always set it.
    if (image.hasBlackBackground()) {
        if (isSrgb == image.isSrgb()) {
            if (isSrgb == image.hasSrgbBlock() && !image.hasNonSrgbBlocks()) {
                KLOGI("Kram", "skipping srgb correction");
                return true;
            }
        }
    }

    // This is the only block written or not
    lodepng::State state;
    if (isSrgb) {
        // this is the only block that gets written
        state.info_png.srgb_defined = 1;
        state.info_png.srgb_intent = 0;
    }
    
    // always redefine background to black, so Finder thumbnails are not white
    // this makes viewing any white icons nearly impossible.  Make suer lodepng
    // ignores this background on import, want the stored pixels not ones composited.
    // Note that _r is only used for grayscale/pallete, and these values are in same
    // color depth as pixels.  But 0 works for all bit-depths.
    state.info_png.background_defined = true;
    state.info_png.background_r = 0;
    state.info_png.background_g = 0;
    state.info_png.background_b = 0;
    
    // TODO: could write other data into Txt block
    // or try to preserve those
    
    // TODO: image converted to 32-bit, so will save out large ?
    // Can we write out L, LA, RGB, RGBA based on image state?
 
    // use miniz as the encoder
    auto& settings = lodepng_default_compress_settings;
    if (useMiniZ)
        settings.custom_zlib = LodepngCompressUsingMiniz;

    // encode to png
    vector<unsigned char> outputData;
    unsigned error = lodepng::encode(outputData, (const uint8_t*)(image.pixels().data()), image.width(), image.height(), state);
    
    if (error) {
        return false;
    }
    
    FileHelper fileHelper;
    if (!fileHelper.open(filename, "wb+")) {
        return false;
    }
    
    // this is overrwriting the source file currently
    // TODO: could use tmp file, and then replace existing
    // this could destroy original png on failure otherwise
    if (!fileHelper.write((const uint8_t*)outputData.data(), outputData.size())) {
        return false;
    }
    
    KLOGI("Kram", "saved %s %s sRGB block", filename, isSrgb ? "with" : "without");
    
    return true;
}

bool SetupTmpFile(FileHelper& tmpFileHelper, const char* suffix)
{
    return tmpFileHelper.openTemporaryFile(suffix, "w+b");
}

bool SetupSourceImage(const string& srcFilename, Image& sourceImage,
                      bool isPremulSrgb = false, bool isGray = false)
{
    bool isKTX = isKTXFilename(srcFilename);
    bool isKTX2 = isKTX2Filename(srcFilename);
    bool isPNG = isPNGFilename(srcFilename);

    if (!(isKTX || isKTX2 || isPNG)) {
        KLOGE("Kram", "File input \"%s\" isn't a png, ktx, ktx2 file.\n",
              srcFilename.c_str());
        return false;
    }

    // TODO: basically KTXImageData, but the encode can't take in a KTXImage yet
    // so here it's generate a single Image.  Also here the LoadKTX converts
    // 1/2/3/4 channel formats to 4.

    MmapHelper mmapHelper;
    vector<uint8_t> fileData;

    // first try mmap, and then use file -> buffer
    bool isMmap = true;
    if (!mmapHelper.open(srcFilename.c_str())) {
        isMmap = false;

        FileHelper fileHelper;

        // fallback to opening file if no mmap support or it didn't work
        if (!fileHelper.open(srcFilename.c_str(), "rb")) {
            KLOGE("Kram", "File input \"%s\" could not be opened for read.\n",
                  srcFilename.c_str());
            return false;
        }

        // read entire png into memory
        size_t size = fileHelper.size();
        if (size == (size_t)-1) {
            return false;
        }

        fileData.resize(size);

        if (!fileHelper.read(fileData.data(), size)) {
            return false;
        }
    }

    const uint8_t* data;
    size_t dataSize;
    if (isMmap) {
        data = mmapHelper.data();
        dataSize = mmapHelper.dataLength();
    }
    else {
        data = fileData.data();
        dataSize = fileData.size();
    }

    //-----------------------

    if (isPNG) {
        bool isSrgb = false;
        if (!LoadPng(data, dataSize, isPremulSrgb, isGray, isSrgb, sourceImage)) {
            return false;  // error
        }
    }
    else {
        if (!LoadKtx(data, dataSize, sourceImage)) {
            return false;  // error
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

    cmd += " -output " dstDir;
    cmd += dst;

    // replace png with ktx
    dst = srcFilename;
    const char* extSeparatorStr = strrchr(dst.c_str(), '.');
    assert(extSeparatorStr != nullptr);
    size_t extSeparator = extSeparatorStr - dst.c_str();
    dst.erase(extSeparator);
    dst.append(".ktx");  // TODO: test ktx2 too

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
            cmd += " -normal" ASTCSwizzle2nm;
            cmd += formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatASTC_4x4_LDR, encoder, isNotPremul);

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
            cmd += " -normal"; // " -quality 100"
            cmd += formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatBC5_RGUnorm, encoder);

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
            cmd += " -normal" ASTCSwizzle2nm;
            cmd += formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatASTC_4x4_LDR, encoder, isNotPremul);

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
            cmd += " -normal";
            cmd += formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatBC5_RGUnorm, encoder);

            break;
        case 13:
            testNumber = 13;
            encoder = kTexEncoderBcenc;
            cmd += " -normal";
            cmd += formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatBC5_RGUnorm, encoder);

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
            cmd += " -optopaque";
            cmd += formatInputAndOutput(testNumber, "ColorMap-a.png", MyMTLPixelFormatBC7_RGBAUnorm_sRGB, encoder);
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
            cmd += " -normal";
            cmd += formatInputAndOutput(testNumber, "collectorbarrel-n.png", MyMTLPixelFormatEAC_RG11Unorm, encoder);

            break;

        case 2003:
            testNumber = 2003;
            encoder = kTexEncoderEtcenc;
            cmd += " -optopaque";
            cmd += formatInputAndOutput(testNumber, "color_grid-a.png", MyMTLPixelFormatEAC_RGBA8_sRGB, encoder);
            break;

            //--------------
            // sdf tests

        case 3001:
            testNumber = 3001;
            encoder = kTexEncoderExplicit;
            cmd += " -sdf";
            cmd += formatInputAndOutput(testNumber, "flipper-sdf.png", MyMTLPixelFormatR8Unorm, encoder);
            break;

        case 3002:
            testNumber = 3002;
            encoder = kTexEncoderSquish;
            cmd += " -sdf";
            cmd += formatInputAndOutput(testNumber, "flipper-sdf.png", MyMTLPixelFormatBC4_RUnorm, encoder);

            break;

        case 3003:
            testNumber = 3003;
            encoder = kTexEncoderEtcenc;
            cmd += " -sdf";
            cmd +=   formatInputAndOutput(testNumber, "flipper-sdf.png", MyMTLPixelFormatEAC_R11Unorm, encoder);

            break;

        case 3004:
            testNumber = 3004;
            encoder = kTexEncoderATE;
            cmd += " -sdf" ASTCSwizzleL1;
            cmd +=     formatInputAndOutput(testNumber, "flipper-sdf.png", MyMTLPixelFormatASTC_4x4_LDR, encoder, isNotPremul);
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

            testNumber = StringToInt32(args[i + 1]);

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
          // TODO: does this support .ktx2, .dds?
          "\t -i/nput <.ktx | .ktx2 | .dds>\n"
          "\t -o/utput .ktx\n"
          "\n",
          showVersion ? usageName : "");
}

void kramFixupUsage(bool showVersion = true)
{
    KLOGI("Kram",
          "%s\n"
          "Usage: kram fixup\n"
          "\t -i/nput <.png>\n"
          "\t -srgb\n"
          "\n",
          showVersion ? usageName : "");
}


void kramInfoUsage(bool showVersion = true)
{
    KLOGI("Kram",
          "%s\n"
          "Usage: kram info\n"
          "\t -i/nput <.png | .ktx | .ktx2 | .dds>\n"
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
          "\t -f/ormat (bc1 | astc4x4 | etc2rgba | rgba16f) [-quality 0-100]\n"
          "\t [-zstd 0] or [-zlib 0] (for .ktx2 output)\n"
          "\t [-srgb] [-srcsrgb] [-srclin] [-srcsrgbflag]\n"
          "\t [-signed] [-normal]\n"
          "\t -i/nput <source.png | .ktx | .ktx2 | .dds>\n"
          "\t -o/utput <target.ktx | .ktx | .ktx2 | .dds>\n"
          "\n"
          "\t [-type 2d|3d|..]\n"
          "\t [-e/ncoder (squish | ate | etcenc | bcenc | astcenc | explicit | ..)]\n"
          "\t [-resize (16x32 | pow2)]\n"
          "\n"
          "\t [-mipnone] [-mipflood]\n"
          "\t [-mipmin size] [-mipmax size] [-mipskip count]\n"
          "\n"
          "\t [-chunks 4x4]\n"
          "\t [-swizzle rg01]\n"
          "\t [-avg rxbx]\n"
          "\t [-sdf]\n"
          "\t [-sdfThreshold 120]\n"
          "\t [-premul] [-prezero] [-premulrgb]\n"
          "\t [-gray]\n"
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
          "\tbc[1,3,4,5] %s\n"  // can be disabled

          "\t-encoder bcenc"
          "\tbc[1,3,4,5,7] %s\n"  // can be disabled

          "\t-encoder ate"
          "\tbc[1,4,5,7] %s\n"  // can be disabled

          "\t-encoder ate"
          "\tastc[4x4,8x8] %s\n"  // can be disabled

          "\t-encoder astcenc"
          "\tastc[4x4,5x5,6x6,8x8] ldr/hdr support %s\n"  // can be disabled

          "\t-encoder etcenc"
          "\tetc2[r,rg,rgb,rgba] %s\n"  // can be disabled

          "\t-encoder explicit"
          "\tr|rg|rgba[8|16f|32f]\n"
          "\n"

          // Mips
          "\t-mipnone"
          "\tDon't build mips even if pow2 dimensions\n"

          "\t-mipflood"
          "\tDilate color by upscaling smaller mips to higher\n"

          "\t-mipmin size"
          "\tOnly output mips >= size px\n"

          "\t-mipmax size"
          "\tOnly output mips <= size px\n"

          "\t-mipskip count"
          "\tOnly output largest mips >= count, similar to mipmax but with count instead of size px\n"
          "\n"

          // tex to normal
          "\t-height"
          "\tConvert height.x to normal.xy\n"
          "\t-heightScale scale"
          "\tScale heights up down to adjust normal map\n"
          "\t-wrap"
          "\tWrap texture at edges (height only for now)\n"
          "\n"

          // srgb
          "\t-srgb"
          "\tsRGB for rgb/rgba formats - applied to src/dst\n"
          "\t-srcsrgb"
          "\tsrc set to sRGB\n"
          "\t-srclin"
          "\tsrc set to linear\n"
          "\t-srcsrgbimage"
          "\tsrc set to png flag (unreliable) or container format\n"
          
          // normals and snorm data
          "\t-signed"
          "\tSigned r or rg for etc/bc formats, astc doesn't have signed format.\n"
          "\t-normal"
          "\tNormal map rg storage signed for etc/bc (rg01), only unsigned astc L+A (gggr).\n"
          
          // sdf
          "\t-sdf"
          "\tGenerate single-channel SDF from a bitmap, can mip and drop large mips. Encode to r8, bc4, etc2r, astc4x4 (Unorm LLL1) to encode\n"
          "\t-sdfThreshold 120"
          "\tSDF generation uses bitmap converted from 8-bit red channel\n"
          
          "\t-gray"
          "\tConvert to grayscale before premul\n"

          // premul is not on by default, but really should be or textures aren't sampled correctly
          // but this really only applies to color channel textures, so off by default.
          "\t-premul"
          "\tPremultiplied alpha to src pixels before output\n"

          // This is meant to work with shaders that (incorrectly) premul after sampling.
          // limits the rgb bleed in regions that should not display colors.  Can stil have black color halos.
          "\t-prezero"
          "\tPremultiplied alpha to src pixels before output but only where a=0\n"

          // This emulates Photoshop premul only on png files.  Multiplies  srgbColor.rgb * a.
          "\t-premulrgb"
          "\tPremultiplied alpha to src pixels at load to emulate Photoshop srgbColor.rgb * a, don't use with -premul\n"
          "\n"

          "\t-optopaque"
          "\tChange format from bc7/3 to bc1, or etc2rgba to rgba if opaque\n"
          "\n"

          "\t-chunks 4x4"
          "\tSpecifies how many chunks to split up texture into 2darray\n"

          // ktx2 specific settings
          "\tktx2 mip compression, if not present then no compresion used\n"
          "\t-zstd 0"
          "\tktx2 with zstd mip compressor, 0 for default, 0 to 100\n"
          "\t-zlib 0"
          "\tktx2 with zlib mip compressor, 0 for default, 0 to 11\n"

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
          etcencEnabled);
}

void kramUsage()
{
    KLOGI("Kram",
          usageName
          "\n"
          "SYNTAX\nkram [encode | decode | info | script | fixup | ...]\n");

    kramEncodeUsage(false);
    kramInfoUsage(false);
    kramDecodeUsage(false);
    kramScriptUsage(false);
    kramFixupUsage(false);
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

    bool isPNG = isPNGFilename(srcFilename);
    bool isKTX = isKTXFilename(srcFilename);
    bool isKTX2 = isKTX2Filename(srcFilename);
    bool isDDS = isDDSFilename(srcFilename);

    if (!(isPNG || isKTX || isKTX2 || isDDS)) {
        KLOGE("Kram", "info only supports png, ktx, ktx2, dds inputs");
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
        if (!dstFileHelper.open(dstFilename.c_str(), "wb+")) {
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
    bool isPNG = isPNGFilename(srcFilename);
    bool isKTX = isKTXFilename(srcFilename);
    bool isKTX2 = isKTX2Filename(srcFilename);
    bool isDDS = isDDSFilename(srcFilename);

    string info;

    // handle png and ktx and dds
    if (isPNG) {
        MmapHelper srcMmapHelper;
        vector<uint8_t> srcFileBuffer;

        // This was taken out of SetupSourceImage, dont want to decode PNG yet
        // just peek at the header.

        // first try mmap, and then use file -> buffer
        bool useMmap = true;
        if (!srcMmapHelper.open(srcFilename.c_str())) {
            // fallback to file system if no mmap or it failed
            useMmap = false;

            FileHelper srcFileHelper;
            if (!srcFileHelper.open(srcFilename.c_str(), "rb")) {
                KLOGE("Kram", "File input \"%s\" could not be opened for info read.\n",
                      srcFilename.c_str());
                return "";
            }

            // read entire png into memory
            // even though really just want to peek at header
            uint64_t size = srcFileHelper.size();
            if (size == (size_t)-1) {
                return "";
            }

            srcFileBuffer.resize(size);
            if (!srcFileHelper.read(srcFileBuffer.data(), size)) {
                return "";
            }
        }

        const uint8_t* data = nullptr;
        size_t dataSize = 0;

        if (useMmap) {
            data = srcMmapHelper.data();
            dataSize = srcMmapHelper.dataLength();
        }
        else {
            data = srcFileBuffer.data();
            dataSize = srcFileBuffer.size();
        }

        info = kramInfoPNGToString(srcFilename, data, dataSize, isVerbose);
    }
    else if (isKTX || isKTX2 || isDDS) {
        KTXImage srcImage;
        KTXImageData srcImageData;

        // TODO: pass isInfoOnly = true to this
        bool success = SetupSourceKTX(srcImageData, srcFilename, srcImage, true);
        if (!success) {
            KLOGE("Kram", "File input \"%s\" could not be opened for info read.\n",
                  srcFilename.c_str());
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

    auto& settings = lodepng_default_decompress_settings;
    if (useMiniZ)
        settings.custom_zlib = LodepngDecompressUsingMiniz;

    // can identify 16unorm data for heightmaps via this call
    LodePNGState state;
    lodepng_state_init(&state);
    state.decoder.ignore_crc = 1;

    errorLode = lodepng_inspect(&width, &height, &state, data, dataSize);
    if (errorLode != 0) {
        KLOGE("Kram", "info couldn't open png file");
        return "";
    }
    
    // TODO: also gama 2.2 block sometimes used in older files
    bool isSrgb = false;
    
    const uint8_t* end = lodepng_chunk_find_const(data, data + dataSize, "IDAT");
    if (!end)
        end = data + dataSize;
    
    const uint8_t* chunkData = lodepng_chunk_find_const(data, end, "sRGB");
    if (chunkData) {
        lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
        isSrgb = state.info_png.srgb_defined;
        //state.info_png.srgb_intent; // 0-3
    }
    
    // Adobe Photoshop 2022 only sets iccp + gama instead of sRGB flag, but iccp takes
    // priority to gama block.
    if (doParseIccProfile && !chunkData) {
        chunkData = lodepng_chunk_find_const(data, end, "iCCP");
        if (chunkData) {
            lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
            if (state.info_png.iccp_defined) {
                if (!isSrgb)
                    isSrgb = isIccProfileSrgb(state.info_png.iccp_profile, state.info_png.iccp_profile_size);
            }
                
        }
    }
    
    if (!chunkData) {
        chunkData = lodepng_chunk_find_const(data, end, "gAMA");
        if (chunkData) {
            lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
            if (state.info_png.gama_defined) {
                if (!isSrgb)
                    isSrgb = state.info_png.gama_gamma == 45455; // 1/2.2 x 100000
            }
                
        }
    }
    
    if (!chunkData) {
        chunkData = lodepng_chunk_find_const(data, data + dataSize, "cHRM");
        if (chunkData) {
            lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
            if (state.info_png.chrm_defined) {
                if (!isSrgb)
                    isSrgb =
                        state.info_png.chrm_red_x == 64000 &&
                        state.info_png.chrm_red_y == 33000 &&
                        state.info_png.chrm_green_x == 30000 &&
                        state.info_png.chrm_green_y == 60000 &&
                        state.info_png.chrm_blue_x == 15000 &&
                        state.info_png.chrm_blue_y == 6000 &&
                        state.info_png.chrm_white_x == 31720 &&
                        state.info_png.chrm_white_y == 32900;
            }
        }
    }
    
    // because Apple finder thumbnails can't be overridden with custom thumbanailer
    // and defaults to white bkgd (making white icons impossible to see).
    // track the bkgd block, and set/re-define as all black.  Maybe will honor that.
    bool hasBackground = false;
    bool hasBlackBackground = false;
    chunkData = lodepng_chunk_find_const(data, data + dataSize, "bKGD");
    if (chunkData) {
        lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
        if (state.info_png.background_defined) {
            hasBackground = true;
            hasBlackBackground =
                state.info_png.background_r == 0 && // gray/pallete uses this only
                state.info_png.background_g == 0 &&
                state.info_png.background_b == 0;
        }
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
            "size: %" PRIu64
            "\n"
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
            "palt: %s\n"
            "srgb: %s\n"
            "bkgd: %s\n",
            textureTypeName(MyMTLTextureType2D),
            width, height,
            width * height / (1000.0f * 1000.0f),
            state.info_png.color.bitdepth,
            hasColor ? "y" : "n",
            hasAlpha ? "y" : "n",
            hasPalette ? "y" : "n",
            isSrgb ? "y" : "n",
            hasBackground ? "y" : "n"
            );
    info += tmp;

    // optional block with ppi
    chunkData = lodepng_chunk_find_const(data, end, "pHYs");
    if (chunkData) {
        lodepng_inspect_chunk(&state, chunkData - data, data, end-data);
    
        if (state.info_png.phys_defined && state.info_png.phys_unit == 1) {
            float metersToInches = 39.37;
            // TODO: there is info_pgn.phys_unit (0 - unknown, 1 - meters)
            sprintf(tmp,
                    "ppix: %d\n"
                    "ppiy: %d\n",
                    (int32_t)(state.info_png.phys_x / metersToInches),
                    (int32_t)(state.info_png.phys_y / metersToInches));
            info += tmp;
        }
    }

    return info;
}

string kramInfoKTXToString(const string& srcFilename, const KTXImage& srcImage, bool isVerbose)
{
    string info;

    // for now driving everything off metal type, but should switch to neutral
    MyMTLPixelFormat metalFormat = srcImage.pixelFormat;

    int32_t dataSize = (int32_t)srcImage.fileDataLength;

    //string tmp;
    bool isMB = (dataSize > (512 * 1024));
    append_sprintf(info,
                   "file: %s\n"
                   "size: %d\n"
                   "sizm: %0.3f %s\n",
                   srcFilename.c_str(),
                   dataSize,
                   isMB ? dataSize / (1024.0f * 1024.0f) : dataSize / 1024.0f,
                   isMB ? "MB" : "KB");

    int32_t numChunks = srcImage.totalChunks();

    // add up lengths and lengthCompressed
    if (srcImage.isSupercompressed()) {
        uint64_t length = 0;
        uint64_t lengthCompressed = 0;

        for (const auto& level : srcImage.mipLevels) {
            length += level.length;
            lengthCompressed += level.lengthCompressed;
        }

        length *= numChunks;
        uint64_t percent = (100 * lengthCompressed) / length;

        isMB = (length > (512 * 1024));
        double lengthF = isMB ? length / (1024.0f * 1024.0f) : length / 1024.0f;
        double lengthCompressedF = isMB ? lengthCompressed / (1024.0f * 1024.0f) : lengthCompressed / 1024.0f;

        append_sprintf(info,
                       "sizc: %0.3f,%0.3f %s %d%%\n"
                       "comp: %s\n",
                       lengthF, lengthCompressedF,
                       isMB ? "MB" : "KB",
                       (int)percent,
                       supercompressionName(srcImage.supercompressionType));
    }

    float numPixels = srcImage.width * srcImage.height;
    numPixels *= (float)numChunks;

    if (srcImage.mipCount() > 1) {
        numPixels *= 4.0 / 3.0f;  // TODO: estimate for now
    }

    // to megapixels
    numPixels /= (1000.0f * 1000.0f);

    auto textureType = srcImage.textureType;
    switch (textureType) {
        case MyMTLTextureType1DArray:
        case MyMTLTextureType2D:
        case MyMTLTextureTypeCube:
        case MyMTLTextureTypeCubeArray:
        case MyMTLTextureType2DArray:
            append_sprintf(info,
                           "type: %s\n"
                           "dims: %dx%d\n"
                           "dimm: %0.3f MP\n"
                           "mips: %d\n",
                           textureTypeName(srcImage.textureType),
                           srcImage.width, srcImage.height,
                           numPixels,
                           srcImage.mipCount());
            break;
        case MyMTLTextureType3D:
            append_sprintf(info,
                           "type: %s\n"
                           "dims: %dx%dx%d\n"
                           "dimm: %0.3f MP\n"
                           "mips: %d\n",
                           textureTypeName(srcImage.textureType),
                           srcImage.width, srcImage.height, srcImage.depth,
                           numPixels,
                           srcImage.mipCount());
            break;
    }

    // print out the array
    if (srcImage.header.numberOfArrayElements > 1) {
        append_sprintf(info,
                       "arry: %d\n",
                       srcImage.header.numberOfArrayElements);
    }

    if (isVerbose) {
        append_sprintf(info,
                       "fmtk: %s\n"
                       "fmtm: %s (%d)\n"
                       "fmtv: %s (%d)\n"
                       "fmtd: %s (%d)\n"
                       "fmtg: %s (0x%04X)\n",
                       formatTypeName(metalFormat),
                       metalTypeName(metalFormat), metalFormat,
                       vulkanTypeName(metalFormat), vulkanType(metalFormat),
                       directxTypeName(metalFormat), directxType(metalFormat),
                       glTypeName(metalFormat), glType(metalFormat));
    }
    else {
        append_sprintf(info,
                       "fmtk: %s\n",
                       formatTypeName(metalFormat));
    }

    // report any props
    for (const auto& prop : srcImage.props) {
        append_sprintf(info, "prop: %s %s\n", prop.first.c_str(), prop.second.c_str());
    }

    if (isVerbose) {
        // dump mips/dims, but this can be a lot of data on arrays
        int32_t mipLevel = 0;

        // num chunks
        append_sprintf(info, "chun: %d\n", numChunks);

        for (const auto& mip : srcImage.mipLevels) {
            uint32_t w, h, d;
            srcImage.mipDimensions(mipLevel, w, h, d);

            switch (textureType) {
                case MyMTLTextureType3D:
                    append_sprintf(info,
                                   "mipl: %d %dx%dx%d ",
                                   mipLevel++,
                                   w, h, d);
                    break;
                default:
                    append_sprintf(info,
                                   "mipl: %d %dx%d ",
                                   mipLevel++,
                                   w, h);
                    break;
            }

            if (mip.lengthCompressed != 0) {
                uint64_t levelSize = mip.length * numChunks;
                uint64_t percent = (100 * mip.lengthCompressed) / levelSize;

                append_sprintf(info,
                               "%" PRIu64 ",%" PRIu64 ",%" PRIu64 " %d%%\n",
                               mip.offset,
                               levelSize,
                               mip.lengthCompressed,
                               (int)percent);
            }
            else {
                append_sprintf(info,
                               "%" PRIu64 ",%" PRIu64 "\n",
                               mip.offset,
                               mip.length  // only size of one mip right now, not mip * numChunks
                );
            }
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
        }

        // probably should be per-command and global verbose
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
        KLOGE("Kram", "decode needs ktx input");
        error = true;
    }
    if (dstFilename.empty()) {
        KLOGE("Kram", "decode needs ktx output");
        error = true;
    }

    bool isKTX = isKTXFilename(srcFilename);
    bool isKTX2 = isKTX2Filename(srcFilename);
    bool isDDS = isDDSFilename(srcFilename);

    if (!(isKTX || isKTX2 || isDDS)) {
        KLOGE("Kram", "decode only supports ktx, ktx2, dds input");
        error = true;
    }

    bool isDstKTX = isKTXFilename(dstFilename);
    //bool isDstKTX2 = isKTX2Filename(dstFilename); // TODO:
    //bool isDstDDS = isDDSFilename(dstFilename); // TODO:

    if (!(isDstKTX)) {
        KLOGE("Kram", "decode only supports ktx output");
        error = true;
    }

    if (error) {
        kramDecodeUsage();
        return -1;
    }

    const char* dstExt = ".ktx";
    //    if (isDstKTX2)
    //        dstExt = ".ktx2";
    //    if (isDstDDS)
    //        dstExt = ".dds";

    KTXImage srcImage;
    KTXImageData srcImageData;
    FileHelper tmpFileHelper;

    bool success = SetupSourceKTX(srcImageData, srcFilename, srcImage, false);
    if (!success)
        return -1;

    // TODO: for hdr decode, may need to walk blocks or ask caller to pass -hdr flag
    if (!validateFormatAndDecoder(srcImage.textureType, srcImage.pixelFormat, textureDecoder)) {
        KLOGE("Kram", "format decode only supports ktx output");
        return -1;
    }

    success = SetupTmpFile(tmpFileHelper, dstExt);
    if (!success)
        return -1;

    if (isVerbose) {
        KLOGI("Kram", "Decoding %s to %s with %s\n",
              textureTypeName(srcImage.textureType),
              metalTypeName(srcImage.pixelFormat),
              encoderName(textureDecoder));
    }

    KramDecoderParams params;
    params.isVerbose = isVerbose;
    params.decoder = textureDecoder;
    params.swizzleText = swizzleText;

    KramDecoder decoder;  // just to call decode
    success = decoder.decode(srcImage, tmpFileHelper.pointer(), params);

    // rename to dest filepath, note this only occurs if above succeeded
    // so any existing files are left alone on failure.
    if (success)
        success = tmpFileHelper.copyTemporaryFileTo(dstFilename.c_str());

    return success ? 0 : -1;
}

int32_t kramAppFixup(vector<const char*>& args)
{
    // this is help
    int32_t argc = (int32_t)args.size();
    if (argc == 0) {
        kramFixupUsage();
        return 0;
    }

    string srcFilename;
    bool doFixupSrgb = false;
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
        
        // TDOO: may want to add output command too
        
        if (isStringEqual(word, "-srgb")) {
            doFixupSrgb = true;
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
        }
        else {
            KLOGE("Kram", "unexpected argument \"%s\"\n",
                  word);
            error = true;
            break;
        }
    }
        
    if (srcFilename.empty()) {
        KLOGE("Kram", "no input file given\n");
        error = true;
    }
        
    if (doFixupSrgb) {
        bool isPNG = isPNGFilename(srcFilename);

        if (!isPNG) {
            KLOGE("Kram", "fixup srgb only supports png input");
            error = true;
        }
        
        bool success = !error;
        
        Image srcImage;
        
        // load the png, this doesn't return srgb state of original png
        if (success)
            success = SetupSourceImage(srcFilename, srcImage);

        // stuff srgb block based on filename to content conversion for now
        if (success) {
            success = SavePNG(srcImage, srcFilename.c_str());
            
            if (!success) {
                KLOGE("Kram", "fixup srgb could not save to file");
            }
        }
        
        if (!success)
            error = true;
    }
    
    return error ? -1 : 0;
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
    bool isGray = false;

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
        }
        else if (isStringEqual(word, "-sdfThreshold")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "sdfThreshold arg invalid");
                error = true;
                break;
            }
            
            infoArgs.sdfThreshold = StringToInt32(args[i]);
            if (infoArgs.sdfThreshold < 1 || infoArgs.sdfThreshold > 255) {
                KLOGE("Kram", "sdfThreshold arg invalid");
                error = true;
                break;
            }
        }
        else if (isStringEqual(word, "-optopaque")) {
            infoArgs.optimizeFormatForOpaque = true;
        }
        else if (isStringEqual(word, "-gray")) {
            isGray = true;
        }

        // mip setting
        else if (isStringEqual(word, "-mipmax")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "mipmax arg invalid");
                error = true;
                break;
            }

            infoArgs.mipMaxSize = StringToInt32(args[i]);
            if (infoArgs.mipMaxSize < 1 || infoArgs.mipMaxSize > 65536) {
                KLOGE("Kram", "mipmax arg invalid");
                error = true;
                break;
            }
        }
        else if (isStringEqual(word, "-mipmin")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "mipmin arg invalid");
                error = true;
                break;
            }

            infoArgs.mipMinSize = StringToInt32(args[i]);
            if (infoArgs.mipMinSize < 1 || infoArgs.mipMinSize > 65536) {
                KLOGE("Kram", "mipmin arg invalid");
                error = true;
                break;
            }
        }
        else if (isStringEqual(word, "-mipskip")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "mipskip arg invalid");
                error = true;
                break;
            }

            infoArgs.mipSkip = StringToInt32(args[i]);
            if (infoArgs.mipSkip < 0 || infoArgs.mipSkip > 16) {
                KLOGE("Kram", "mipskip arg invalid");
                error = true;
                break;
            }
        }
        else if (isStringEqual(word, "-mipnone")) {
            // disable mips even if pow2
            infoArgs.doMipmaps = false;
        }
        else if (isStringEqual(word, "-mipflood")) {
            infoArgs.doMipflood = true;
        }

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
        }
        else if (isStringEqual(word, "-height")) {
            // converted to a normal map
            infoArgs.isHeight = true;
        }
        else if (isStringEqual(word, "-wrap")) {
            // whether texture is clamp or wrap
            infoArgs.isWrap = true;
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
        }
        else if (isStringEqual(word, "-type")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "type arg invalid");
                error = true;
                break;
            }

            infoArgs.textureType = parseTextureType(args[i]);
        }
        else if (isStringEqual(word, "-quality")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "quality arg invalid");
                error = true;
                break;
            }

            infoArgs.quality = StringToInt32(args[i]);
        }

        else if (isStringEqual(word, "-output") ||
                 isStringEqual(word, "-o")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "no output file defined");
                error = true;
                break;
            }

            // TODO: if args Ωƒends with /, then output to that dir
            dstFilename = args[i];
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
        }

        // these affect the format
        else if (isStringEqual(word, "-hdr")) {
            // not validating format for whether it's hdr or not
            infoArgs.isHDR = true;
        }
        else if (isStringEqual(word, "-signed")) {
            // not validating format for whether it's signed or not
            infoArgs.isSigned = true;
        }
        else if (isStringEqual(word, "-srgb")) {
            // not validating format for whether it's srgb or not
            infoArgs.isSRGBSrc = true;
            
            // The format may override this setting.  Not all formats
            // have an srgb varient.
            infoArgs.isSRGBDst = true;
        }
        
        // This means ignore the srgb state on the src image
        // This has to be specified after -srgb
        else if (isStringEqual(word, "-srclin")) {
            infoArgs.isSRGBSrc = false;
        }
        else if (isStringEqual(word, "-srcsrgb")) {
            infoArgs.isSRGBSrc = true;
        }
        // This pulls from the format on dds/ktx/ktx2, or png srgb/chrm/iccp flag
        // Make sure image isn't set to isSRGBSrc.
        else if (isStringEqual(word, "-srcsrgbimage")) {
            infoArgs.isSRGBSrcFlag = false;
        }

        else if (isStringEqual(word, "-normal")) {
            infoArgs.isNormal = true;
        }
        else if (isStringEqual(word, "-resize")) {
            ++i;
            if (i >= argc) {
                error = true;
                KLOGE("Kram", "resize arg invalid");
                break;
            }

            resizeString = args[i];
        }

        // This means to post-multiply alpha after loading, not that incoming data in already premul
        // png has the limitation that it's unmul, but tiff/exr can store premul.  With 8-bit images
        // really would prefer to premul them when building the texture.
        else if (isStringEqual(word, "-premul")) {
            infoArgs.isPremultiplied = true;
        }
        else if (isStringEqual(word, "-prezero")) {
            infoArgs.isPrezero = true;
        }
        // this means premul the data at read from srgb, this it to match photoshop
        else if (isStringEqual(word, "-premulrgb")) {
            isPremulRgb = true;
            infoArgs.isSourcePremultiplied = true;
        }

        else if (isStringEqual(word, "-v") ||
                 isStringEqual(word, "-verbose")) {
            infoArgs.isVerbose = true;
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
        }

        // compressor for ktx2 mips
        // TODO: need level control
        else if (isStringEqual(word, "-zstd")) {
            infoArgs.compressor.compressorType = KTX2SupercompressionZstd;
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "zstd level arg invalid");
                error = true;
                break;
            }
            infoArgs.compressor.compressorLevel = StringToInt32(args[i]);
        }
        else if (isStringEqual(word, "-zlib")) {
            infoArgs.compressor.compressorType = KTX2SupercompressionZlib;
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "zlib level arg invalid");
                error = true;
                break;
            }
            infoArgs.compressor.compressorLevel = StringToInt32(args[i]);
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

    // allow input
    bool isDDS = isDDSFilename(srcFilename);
    bool isKTX = isKTXFilename(srcFilename);
    bool isKTX2 = isKTX2Filename(srcFilename);
    bool isPNG = isPNGFilename(srcFilename);

    if (!(isPNG || isKTX || isKTX2 || isDDS)) {
        KLOGE("Kram", "encode only supports png, ktx, ktx2, dds input");
        error = true;
    }

    // allow output
    bool isDstDDS = isDDSFilename(dstFilename);
    bool isDstKTX = isKTXFilename(dstFilename);
    bool isDstKTX2 = isKTX2Filename(dstFilename);

    if (!(isDstKTX || isDstKTX2 || isDstDDS)) {
        KLOGE("Kram", "encode only supports ktx, ktx2, dds output");
        error = true;
    }

    if (error) {
        kramEncodeUsage();
        return -1;
    }

    const char* dstExt = ".ktx";
    if (isDstKTX2)
        dstExt = ".ktx2";
    else if (isDstDDS)
        dstExt = ".dds";

    infoArgs.isKTX2 = isDstKTX2;

    // Any new settings just go into this struct which is passed into encoder
    ImageInfo info;
    info.initWithArgs(infoArgs);

    // load the source image
    // The helper keeps ktx mips in mmap alive in case want to read them
    // incrementally. Fallback to read into fileBuffer if mmap fails.
    Image srcImage;
    KTXImage srcImageKTX;

    FileHelper tmpFileHelper;

    bool canEncodeInput = true;
    bool success = true;
    if (isDDS) {
        // Note: this is type KTXImage, not Image.

        KTXImageData srcImageData;
        success = SetupSourceKTX(srcImageData, srcFilename, srcImageKTX, false);

        if (success) {
            if (isBlockFormat(srcImageKTX.pixelFormat)) {
                // can only export to dds/ktx (KTX2 would need to supercompress in Image encode path)
                canEncodeInput = false;

                if (isDstKTX2) {
                    KLOGE("Kram", "encode can only export dds import to ktx, dds");
                    success = false;
                }
            }
            else {
                success = srcImage.loadImageFromKTX(srcImageKTX);
            }
        }
    }
    else {
        success = SetupSourceImage(srcFilename, srcImage, isPremulRgb, isGray);
    }

    if (success) {
        success = SetupTmpFile(tmpFileHelper, dstExt);

        if (!success) {
            KLOGE("Kram", "encode couldn't generate tmp file for output");
        }
    }

    if (success && !canEncodeInput) {
        // write the image out with mips to the file (no encode is done)

        success = false;

        // Don't allow DDS -> DDS?  This is really the only case for this
        if (isDstDDS && !isDDS) {
            DDSHelper ddsHelper;
            success = ddsHelper.save(srcImageKTX, tmpFileHelper);
        }
        else if (isDstKTX) {
            KramEncoder encoder;
            success = encoder.saveKTX1(srcImageKTX, tmpFileHelper.pointer());
        }
        else if (isDstKTX2) {
            KramEncoder encoder;
            success = encoder.saveKTX2(srcImageKTX, infoArgs.compressor, tmpFileHelper.pointer());
        }

        if (!success) {
            KLOGE("Kram", "save to format failed");
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

    // so now can complete validation knowing hdr vs. ldr input
    // this checks the dst format
    else if (success) {
        bool isHDR = !srcImage.pixelsFloat().empty();

        if (isHDR) {
            MyMTLPixelFormat format = info.pixelFormat;

            // astcecnc and bcenc are only hdr encoder with explicit input from 16f/32f mips.
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

            // TODO: test bc6h and bcenc?

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
            KramEncoder encoder;

            if (isDstDDS) {
                // encode to ktx
                KTXImage dstImage;
                success = encoder.encode(info, srcImage, dstImage);
                if (!success) {
                    KLOGE("Kram", "encode failed");
                }

                // save as dds
                if (success) {
                    DDSHelper ddsHelper;
                    success = ddsHelper.save(dstImage, tmpFileHelper);
                    if (!success) {
                        KLOGE("Kram", "encode dds convert failed");
                    }
                }
            }
            else {
                success = encoder.encode(info, srcImage, tmpFileHelper.pointer());
                if (!success) {
                    KLOGE("Kram", "encode failed");
                }
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

        if (isStringEqual(word, "-input") ||
            isStringEqual(word, "-i")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "no input file defined");
                error = true;
                break;
            }

            srcFilename = args[i];
        }
        else if (isStringEqual(word, "-jobs") ||
                 isStringEqual(word, "-j")) {
            ++i;
            if (i >= argc) {
                KLOGE("Kram", "no job count defined");

                error = true;
                break;
            }

            numJobs = StringToInt32(args[i]);
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
    std::atomic<int32_t> errorCounter(0);  // doesn't initialize to 0 otherwise
    std::atomic<int32_t> skippedCounter(0);
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
    kCommandTypeFixup,
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
    else if (isStringEqual(command, "fixup")) {
        commandType = kCommandTypeFixup;
    }
    return commandType;
}

void PSTest()
{
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

        values2[i] = uint8_t(roundf(value * 255.0f));

        //KLOGI("srgb", "[%d] = %g\n", i, value);
    }

    // convert srgb to linear and then do premul
    for (int32_t i = 0; i < 256; ++i) {
        float value = mipper.toLinear(i);
        value *= alphaF;

        values3[i] = uint8_t(roundf(value * 255.0));
    }

    // log them side-by-side for comparison
    KLOGI("srgb", "premul by %0.3f", 200.0 / 255.0);
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
        case kCommandTypeFixup:
            args.erase(args.begin());
            return kramAppFixup(args);
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

bool isSupportedFilename(const char* filename) {
    if (isPNGFilename(filename) ||
        isKTXFilename(filename) ||
        isKTX2Filename(filename) ||
        isDDSFilename(filename)) {
    return true;
    }
    return false;
}

void fixPixelFormat(KTXImage& image, const char* filename)
{
    // have to adjust the format if srgb png
    // PS2022 finally added sRGB gama/iccp blocks to "Save As",
    // but there are a lot of older files where this is not set
    // or Figma always sets sRGB.  So set based on content type.
    static bool doReplacePixelFormatFromContentType = true;
    if (!doReplacePixelFormatFromContentType)
        return;
    
    bool isPNG = isPNGFilename(filename);
    if (!isPNG)
        return;
    
    TexContentType contentType = findContentTypeFromFilename(filename);
    
    bool isSrgb = contentType == TexContentTypeAlbedo;
    image.pixelFormat = isSrgb ? MyMTLPixelFormatRGBA8Unorm_sRGB : MyMTLPixelFormatRGBA8Unorm;
}

// Use naming convention for PNG/DDS, can pull from props on KTX/KTX2
// This is mostly useful for source content.
TexContentType findContentTypeFromFilename(const char* filename)
{
    string filenameShort = filename;
    
    const char* dotPosStr = strrchr(filenameShort.c_str(), '.');
    if (dotPosStr == nullptr)
        return TexContentTypeUnknown;
    auto dotPos = dotPosStr - filenameShort.c_str();
    
    // now chop off the extension
    filenameShort = filenameShort.substr(0, dotPos);

    // dealing with png means fabricating the format, texture type, and other data
    if (endsWith(filenameShort, "-sdf")) {
        return TexContentTypeSDF;
    }
    else if (endsWith(filenameShort, "-h")) {
        return TexContentTypeHeight;
    }
    else if (endsWith(filenameShort, "-n") ||
             endsWith(filenameShort, "_normal") ||
             endsWith(filenameShort, "_Normal")
             )
    {
        return TexContentTypeNormal;
    }
    else if (endsWith(filenameShort, "-a") ||
             endsWith(filenameShort, "-d") ||
             endsWith(filenameShort, "_baseColor") ||
             endsWith(filenameShort, "_Color")
             )
    {
        return TexContentTypeAlbedo;
    }
    else if (endsWith(filenameShort, "-ao") ||
             endsWith(filenameShort, "_AO")
             )
    {
        return TexContentTypeAO;
    }
    else if (endsWith(filenameShort, "-mr") ||
             endsWith(filenameShort, "_Metallic") ||
             endsWith(filenameShort, "_Roughness") ||
             endsWith(filenameShort, "_MetaliicRoughness")
             )
    {
        return TexContentTypeMetallicRoughness;
    }
    
    // fallback to albedo for now
    return TexContentTypeAlbedo;
}

}  // namespace kram
