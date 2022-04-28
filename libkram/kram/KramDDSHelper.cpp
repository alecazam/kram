// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramDDSHelper.h"

#include "KTXImage.h"
#include "KramFileHelper.h"

namespace kram {
using namespace NAMESPACE_STL;

const uint32_t DDS_MAGIC = 0x20534444; // "DDS "

#define MAKEFOURCC(str)                                                       \
    ((uint32_t)(uint8_t)(str[0]) | ((uint32_t)(uint8_t)(str[1]) << 8) |       \
    ((uint32_t)(uint8_t)(str[2]) << 16) | ((uint32_t)(uint8_t)(str[3]) << 24 ))

// DX9 era formats, only for reading dds files in dx9 style
enum D3DFORMAT : uint32_t
{
    D3DFMT_UNKNOWN              =  0,

//    D3DFMT_R8G8B8               = 20,
    D3DFMT_A8R8G8B8             = 21,
//    D3DFMT_X8R8G8B8             = 22,
//    D3DFMT_R5G6B5               = 23,
//    D3DFMT_X1R5G5B5             = 24,
//    D3DFMT_A1R5G5B5             = 25,
//    D3DFMT_A4R4G4B4             = 26,
//    D3DFMT_R3G3B2               = 27,
//    D3DFMT_A8                   = 28,
//    D3DFMT_A8R3G3B2             = 29,
//    D3DFMT_X4R4G4B4             = 30,
//    D3DFMT_A2B10G10R10          = 31,
    D3DFMT_A8B8G8R8             = 32,
//    D3DFMT_X8B8G8R8             = 33,
//    D3DFMT_G16R16               = 34,
//    D3DFMT_A2R10G10B10          = 35,
//    D3DFMT_A16B16G16R16         = 36,

//    D3DFMT_A8P8                 = 40,
//    D3DFMT_P8                   = 41,

//    D3DFMT_L8                   = 50,
//    D3DFMT_A8L8                 = 51,
//    D3DFMT_A4L4                 = 52,

//    D3DFMT_V8U8                 = 60,
//    D3DFMT_L6V5U5               = 61,
//    D3DFMT_X8L8V8U8             = 62,
//    D3DFMT_Q8W8V8U8             = 63,
//    D3DFMT_V16U16               = 64,
//    D3DFMT_A2W10V10U10          = 67,

//    D3DFMT_UYVY                 = MAKEFOURCC("UYVY"),
//    D3DFMT_R8G8_B8G8            = MAKEFOURCC("RGBG"),
//    D3DFMT_YUY2                 = MAKEFOURCC("YUY2"),
//    D3DFMT_G8R8_G8B8            = MAKEFOURCC("GRGB"),
    
    D3DFMT_DXT1                 = MAKEFOURCC("DXT1"),
    D3DFMT_DXT2                 = MAKEFOURCC("DXT2"),
    D3DFMT_DXT3                 = MAKEFOURCC("DXT3"),
    D3DFMT_DXT4                 = MAKEFOURCC("DXT4"),
    D3DFMT_DXT5                 = MAKEFOURCC("DXT5"),

    // Not worth support dx9-style files, these don't even hold srgb state
    D3DFMT_ATI1 = MAKEFOURCC("ATI1"),
    D3DFMT_BC4U = MAKEFOURCC("BC4U"),
    D3DFMT_BC4S = MAKEFOURCC("BC4S"),
    
    D3DFMT_ATI2 = MAKEFOURCC("ATI2"),
    D3DFMT_BC5U = MAKEFOURCC("BC5U"),
    D3DFMT_BC5S = MAKEFOURCC("BC5S"),
    
//    D3DFMT_D16_LOCKABLE         = 70,
//    D3DFMT_D32                  = 71,
//    D3DFMT_D15S1                = 73,
//    D3DFMT_D24S8                = 75,
//    D3DFMT_D24X8                = 77,
//    D3DFMT_D24X4S4              = 79,
//    D3DFMT_D16                  = 80,
//
//    D3DFMT_D32F_LOCKABLE        = 82,
//    D3DFMT_D24FS8               = 83,

    //D3DFMT_D32_LOCKABLE         = 84,
    //D3DFMT_S8_LOCKABLE          = 85,

//    D3DFMT_L16                  = 81,
//
//    D3DFMT_VERTEXDATA           =100,
//    D3DFMT_INDEX16              =101,
//    D3DFMT_INDEX32              =102,

    //D3DFMT_Q16W16V16U16         =110,

    //D3DFMT_MULTI2_ARGB8         = MAKEFOURCC("MET1"),

    D3DFMT_R16F                 = 111,
    D3DFMT_G16R16F              = 112,
    D3DFMT_A16B16G16R16F        = 113,

    D3DFMT_R32F                 = 114,
    D3DFMT_G32R32F              = 115,
    D3DFMT_A32B32G32R32F        = 116,

//    D3DFMT_CxV8U8               = 117,

    //D3DFMT_A1                   = 118,
    //D3DFMT_A2B10G10R10_XR_BIAS  = 119,
    //D3DFMT_BINARYBUFFER         = 199,

    D3DFMT_FORCE_DWORD          =0x7fffffff
};

enum DDS_FLAGS : uint32_t
{
    
    DDSD_HEIGHT = 0x00000002,
    DDSD_DEPTH  = 0x00800000,

    DDSD_WIDTH = 0x00000004,
    DDSD_LINEARSIZE = 0x00080000,
    DDSD_PITCH = 0x00000008,

    DDSD_CAPS        = 0x00000001,
    DDSD_PIXELFORMAT = 0x00001000,
    DDSD_MIPMAPCOUNT = 0x00020000,

    // ddspf
    DDSPF_ALPHAPIXELS = 0x00000001,
    DDSPF_FOURCC =      0x00000004,
    DDSPF_RGB =         0x00000040,
    DDSPF_LUMINANCE =   0x00020000, // dx9
    DDSPF_ALPHA =       0x00000002, // dx9
    //DDSPF_BUMPDUDV =    0x00080000,
    
    // caps
    DDSCAPS_TEXTURE = 0x00001000,
    DDSCAPS_MIPMAP  = 0x00400000,
    DDSCAPS_COMPLEX = 0x00000008,
    
    // caps2
    DDSCAPS2_VOLUME = 0x200000,
    DDSCAPS2_CUBEMAP_ALLFACES = 0x0000FA00, // DDSCAPS2_CUBEMAP | all faces
    DDSCAPS2_CUBEMAP = 0x00000200, // DDSCAPS2_CUBEMAP
    
    DDS_RESOURCE_MISC_TEXTURECUBE = 0x4,
    
    // resourceDimension
    DDS_DIMENSION_TEXTURE1D = 2,
    DDS_DIMENSION_TEXTURE2D = 3,
    DDS_DIMENSION_TEXTURE3D = 4,
    
    FOURCC_DX10 = MAKEFOURCC("DX10"),
    
    // dx10 misc2 flags
    DDS_ALPHA_MODE_UNKNOWN = 0,
    DDS_ALPHA_MODE_STRAIGHT = 1,
    DDS_ALPHA_MODE_PREMULTIPLIED = 2,
    DDS_ALPHA_MODE_OPAQUE = 3,
    DDS_ALPHA_MODE_CUSTOM = 4,
};

struct DDS_PIXELFORMAT
{
    uint32_t    size;
    uint32_t    flags;
    uint32_t    fourCC;
    uint32_t    RGBBitCount;
    uint32_t    RBitMask;
    uint32_t    GBitMask;
    uint32_t    BBitMask;
    uint32_t    ABitMask;
};

struct DDS_HEADER
{
    uint32_t        size;
    uint32_t        flags;
    uint32_t        height;
    uint32_t        width;
    uint32_t        pitchOrLinearSize;
    uint32_t        depth; // only if DDS_HEADER_FLAGS_VOLUME is set in flags
    uint32_t        mipMapCount;
    uint32_t        reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t        caps;
    uint32_t        caps2;
    uint32_t        caps3;
    uint32_t        caps4;
    uint32_t        reserved2;
};

struct DDS_HEADER_DXT10
{
    uint32_t /*DXGI_FORMAT*/     dxgiFormat;
    uint32_t        resourceDimension;
    uint32_t        miscFlag; // see D3D11_RESOURCE_MISC_FLAG
    uint32_t        arraySize;
    uint32_t        miscFlags2;
};

// DX9 bitmask parsing adapted from GetPixelFormat() call here https://github.com/microsoft/DirectXTex/blob/main/DDSTextureLoader/DDSTextureLoader12.cpp
static MyMTLPixelFormat getMetalFormatFromDDS9(const DDS_PIXELFORMAT& ddpf)
{
    // Copyright (c) Microsoft Corporation.
    // Licensed under the MIT License.
    #define ISBITMASK( r,g,b,a ) ( ddpf.RBitMask == r && ddpf.GBitMask == g && ddpf.BBitMask == b && ddpf.ABitMask == a )

    if (ddpf.flags & DDSPF_RGB)
    {
        // Note that sRGB formats are written using the "DX10" extended header
        // here would need to force the format to an srgb format from cli
        switch (ddpf.RGBBitCount)
        {
        case 32:
            if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
            {
                return MyMTLPixelFormatRGBA8Unorm;
            }

            if (ISBITMASK(0xffffffff, 0, 0, 0))
            {
                // Only 32-bit color channel format in D3D9 was R32F
                return MyMTLPixelFormatR32Float; // D3DX writes this out as a FourCC of 114
            }
            break;

        case 8:
            // NVTT versions 1.x wrote this as RGB instead of LUMINANCE
            if (ISBITMASK(0xff, 0, 0, 0))
            {
                return MyMTLPixelFormatR8Unorm;
            }

            // No 3:3:2 or paletted DXGI formats aka D3DFMT_R3G3B2, D3DFMT_P8
            break;
        }
    }
    else if (ddpf.flags & DDSPF_LUMINANCE)
    {
        // TODO: need rrrg swizzle on these
        switch (ddpf.RGBBitCount)
        {
        case 16:
            if (ISBITMASK(0x00ff, 0, 0, 0xff00))
            {
                return MyMTLPixelFormatRG8Unorm; // D3DX10/11 writes this out as DX10 extension
            }
            break;

        case 8:
            if (ISBITMASK(0xff, 0, 0, 0))
            {
                return MyMTLPixelFormatR8Unorm; // D3DX10/11 writes this out as DX10 extension
            }

            // No DXGI format maps to ISBITMASK(0x0f,0,0,0xf0) aka D3DFMT_A4L4

            if (ISBITMASK(0x00ff, 0, 0, 0xff00))
            {
                return MyMTLPixelFormatRG8Unorm; // Some DDS writers assume the bitcount should be 8 instead of 16
            }
            break;
        }
    }
    else if (ddpf.flags & DDSPF_ALPHA)
    {
        if (8 == ddpf.RGBBitCount)
        {
            // TODO: need rrrr swizzle
            return MyMTLPixelFormatR8Unorm; // really A8, but use a swizzle
        }
    }
    else if (ddpf.flags & DDSPF_FOURCC)
    {
        switch (ddpf.fourCC)
        {
            case D3DFMT_DXT1: return MyMTLPixelFormatBC1_RGBA;
            //case D3DFMT_DXT2: return MyMTLPixelFormatBC2_RGBA; // isPremul
            //case D3DFMT_DXT3: return MyMTLPixelFormatBC2_RGBA;
            case D3DFMT_DXT4: return MyMTLPixelFormatBC3_RGBA; // isPremul
            case D3DFMT_DXT5: return MyMTLPixelFormatBC3_RGBA;
                
            case D3DFMT_ATI1: return MyMTLPixelFormatBC4_RUnorm;
            case D3DFMT_BC4U: return MyMTLPixelFormatBC4_RUnorm;
            case D3DFMT_BC4S: return MyMTLPixelFormatBC4_RSnorm;
            
            case D3DFMT_ATI2: return MyMTLPixelFormatBC5_RGUnorm;
            case D3DFMT_BC5U: return MyMTLPixelFormatBC5_RGUnorm;
            case D3DFMT_BC5S: return MyMTLPixelFormatBC5_RGSnorm;
            
            case D3DFMT_R16F: return MyMTLPixelFormatR16Float;
            case D3DFMT_G16R16F: return MyMTLPixelFormatRG16Float;
            case D3DFMT_A16B16G16R16F: return MyMTLPixelFormatRGBA16Float;
                
            case D3DFMT_R32F: return MyMTLPixelFormatR32Float;
            case D3DFMT_G32R32F: return MyMTLPixelFormatRG32Float;
            case D3DFMT_A32B32G32R32F: return MyMTLPixelFormatRGBA32Float;
        }
        
    }

    return MyMTLPixelFormatInvalid;
    #undef ISBITMASK
}

bool DDSHelper::load(const uint8_t* data, size_t dataSize, KTXImage& image, bool isInfoOnly)
{
    const uint32_t magicSize = sizeof(uint32_t);
    uint32_t mipDataOffset = magicSize + sizeof(DDS_HEADER);
    
    if (dataSize <= mipDataOffset) {
        KLOGE("kram", "bad dataSize too small %zu <= %d", dataSize, mipDataOffset);
        return false;
    }
    
    const uint32_t& magic = *(const uint32_t*)data;
    const DDS_HEADER& hdr = *(const DDS_HEADER*)(data + magicSize);
    const DDS_PIXELFORMAT& format = hdr.ddspf;
    
    if (magic != DDS_MAGIC) {
        KLOGE("kram", "bad magic number 0x%08X", magic);
        return false;
    }

    if (hdr.size != sizeof(DDS_HEADER)) {
        KLOGE("kram", "bad header size %d", hdr.size);
        return false;
    }
    if (format.size != sizeof(DDS_PIXELFORMAT)) {
        KLOGE("kram", "bad format size %d", format.size);
        return false;
    }
    
    // this flag must be set even though just using fourcc to indicate DX10
    if ((format.flags & DDSPF_FOURCC) == 0) {
        KLOGE("kram", "missing format.fourCC flag");
        return false;
    }
    
    bool isDDS10 = format.fourCC == FOURCC_DX10;
    const DDS_HEADER_DXT10& hdr10 = *(const DDS_HEADER_DXT10*)(data + magicSize + sizeof(DDS_HEADER));
    
    MyMTLPixelFormat pixelFormat = MyMTLPixelFormatInvalid;
    if (isDDS10) {
        mipDataOffset += sizeof(DDS_HEADER_DXT10);
        pixelFormat = directxToMetalFormat(hdr10.dxgiFormat);
    }
    else {
        pixelFormat = getMetalFormatFromDDS9(format);
    }
    
    // Kram only supports a subset of DDS formats
    if (pixelFormat == MyMTLPixelFormatInvalid) {
        KLOGE("kram", "unsupported dds format");
        return false;
    }
    
    // make sure to copy mips/slices from DDS array-ordered to mip-ordered for KTX
    uint32_t width = (hdr.flags & DDSD_WIDTH) ? hdr.width : 1;
    uint32_t height = (hdr.flags & DDSD_HEIGHT) ? hdr.height : 1;
    uint32_t depth = (hdr.flags & DDSD_DEPTH) ? hdr.depth : 1;
    
    uint32_t mipCount = (hdr.flags & DDSD_MIPMAPCOUNT) ? hdr.mipMapCount : 1;
    uint32_t arrayCount = 1;
    
    if (isDDS10) {
        arrayCount = hdr10.arraySize;
    }
    
    // make sure that counts are reasonable
    const uint32_t kMaxMipCount = 16;
    const uint32_t kMaxTextureSize = 1u << (kMaxMipCount - 1); // 32K
    const uint32_t kMaxArrayCount = 2*1024;
   
    if (width > kMaxTextureSize) {
        KLOGE("kram", "bad dimension width %d", width);
        return false;
    }
    if (height > kMaxTextureSize) {
        KLOGE("kram", "bad dimension height %d", height);
        return false;
    }
    if (depth > kMaxTextureSize) {
        KLOGE("kram", "bad dimension depth %d", depth);
        return false;
    }
    if (mipCount > kMaxMipCount) {
        KLOGE("kram", "bad dimension mipCount %d", mipCount);
        return false;
    }
    if (arrayCount > kMaxArrayCount) {
        KLOGE("kram", "bad dimension height %d", arrayCount);
        return false;
    }
    
    // does mipCount = 0 mean automip?
    if (width == 0)
        width = 1;
    if (height == 0)
        height = 1;
    if (depth == 0)
        depth = 1;
    
    if (mipCount == 0)
        mipCount = 1;
    if (arrayCount == 0)
        arrayCount = 1;
    
    bool isCube = false;
    bool isArray = arrayCount > 1;
    bool isPremul = false;
    
    if (isDDS10) {
        isCube = (hdr10.miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE);
        
        switch(hdr10.resourceDimension) {
            case DDS_DIMENSION_TEXTURE1D:
                image.textureType = MyMTLTextureType1DArray;
                isArray = true; // kram doesn't support 1d
                break;
            case DDS_DIMENSION_TEXTURE2D:
                if (isCube) {
                    image.textureType = isArray ? MyMTLTextureTypeCubeArray : MyMTLTextureTypeCube;
                }
                else {
                    image.textureType = isArray ? MyMTLTextureType2DArray : MyMTLTextureType2D;
                }
                break;
            case DDS_DIMENSION_TEXTURE3D:
                image.textureType = MyMTLTextureType3D;
                isArray = false;
                break;
        }
        isPremul = (hdr10.miscFlags2 & DDS_ALPHA_MODE_PREMULTIPLIED) != 0;
    }
    else {
        isArray = false;
        
        if (hdr.flags & DDSD_DEPTH) {
            image.textureType = MyMTLTextureType3D;
        }
        else if (hdr.caps2 & DDSCAPS2_CUBEMAP) {
            image.textureType = MyMTLTextureTypeCube;
        }
    }
    
    // transfer premul setting, would like to not depend on "info" to carry this
    if (isPremul)
        image.addChannelProps("Alb.ra,Alb.ga,Alb.ba,Alb.a");
    
    //-------------
    
    // TODO: may need to fix these to KTX conventions first
    image.width = width;
    image.height = height;
    image.depth = depth;
    
    auto& ktxHdr = image.header;
    ktxHdr.pixelWidth  = image.width;
    ktxHdr.pixelHeight = image.height;
    ktxHdr.pixelDepth  = image.depth;
    
    ktxHdr.initFormatGL(pixelFormat);
    
    ktxHdr.numberOfFaces = isCube ? 6 : 1;
    ktxHdr.numberOfMipmapLevels = mipCount;
    ktxHdr.numberOfArrayElements = arrayCount;
    
    // fix up the values, so that can convert header properly to type in info
    // TODO: this means image and ktx header don't match
    if (!isArray)
        ktxHdr.numberOfArrayElements = 0;
    if (image.textureType != MyMTLTextureType3D)
        ktxHdr.pixelDepth = 0;
    if (image.textureType == MyMTLTextureType1DArray)
        ktxHdr.pixelHeight = 0;
    if (image.textureType == MyMTLTextureTypeCube || image.textureType == MyMTLTextureTypeCubeArray) {
        // DX10+ require all faces to be defined, DX9 could have partial cubemaps
        if ((hdr.caps2 & DDSCAPS2_CUBEMAP_ALLFACES) != DDSCAPS2_CUBEMAP_ALLFACES) {
            KLOGE("kram", "unsupported all faces of cubemap must be specified");
            return false;
        }
    }
        
    // make sure derived type lines up
    if (ktxHdr.metalTextureType() != image.textureType) {
        KLOGE("kram", "unsupported textureType");
        return false;
    }
    
    image.pixelFormat = pixelFormat;
    
    // allocate data
    image.initMipLevels(mipDataOffset);
    
    // Skip allocating the pixels
    if (!isInfoOnly) {
        image.reserveImageData();
        
        uint8_t* dstImageData = image.imageData().data();
        const uint8_t* srcImageData = data + mipDataOffset;
        
        size_t srcOffset = 0;
        for (uint32_t chunkNum = 0; chunkNum < image.totalChunks(); ++chunkNum) {
            for (uint32_t mipNum = 0; mipNum < image.mipCount(); ++mipNum) {
                // memcpy from src to dst
                size_t dstOffset = image.chunkOffset(mipNum, chunkNum);
                size_t mipLength = image.mipLevels[mipNum].length;
        
                memcpy(dstImageData + dstOffset, srcImageData + srcOffset, mipLength);
                
                srcOffset += mipLength;
            }
        }
    }
    
    // Now have a valid KTX or KTX2 file from the DDS
    
    return true;
}

bool DDSHelper::save(const KTXImage& image, FileHelper& fileHelper)
{
    // Need to order mips/slices/arrays according to DDS convention for saving.
    // This ordering is different than KTX/KTX2 ordering.  Mips cannot
    // be compressed when writing to DDS, so KTX conversion is simpler.
    if (image.isSupercompressed())
        return false;
    
    // Can only write out if matching format in DDS
    if (directxType(image.pixelFormat) == MyMTLPixelFormatInvalid)
        return false;
    
    // https://docs.microsoft.com/en-us/windows/win32/direct3ddds/dds-header
    
    // lots of headers, this is newer dx10 style dds
    DDS_HEADER hdr = {};
    DDS_PIXELFORMAT& format = hdr.ddspf;
    DDS_HEADER_DXT10 hdr10 = {};
    
    hdr.size = sizeof(DDS_HEADER);
    format.size = sizeof(DDS_PIXELFORMAT);
    
    hdr.width = image.width;
    hdr.height = image.height;
    hdr.depth = image.depth;
    
    hdr.mipMapCount = image.mipCount();
    
    hdr.caps |= DDSCAPS_TEXTURE;
    if (image.mipCount() > 1) {
        hdr.caps |= DDSCAPS_MIPMAP;
        hdr.flags |= DDSD_MIPMAPCOUNT;
    }
    
    // indicate this is newer dds file with pixelFormat
    // important to set FOURCC flag
    format.fourCC = FOURCC_DX10;
    format.flags |= DDSPF_FOURCC;
    
    hdr.flags |= DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    
    if (hdr.depth > 1)
        hdr.flags |= DDSD_DEPTH;
    
    if (isBlockFormat(image.pixelFormat)) {
        hdr.flags |= DDSD_LINEARSIZE;
        
        // This is assuming BC 4x4 blocks
        hdr.pitchOrLinearSize = image.blockDims().x * blockSizeOfFormat(image.pixelFormat);
    }
    else {
        hdr.flags |= DDSD_PITCH;
        hdr.pitchOrLinearSize = image.blockDims().x * blockSizeOfFormat(image.pixelFormat);
    }
    
    hdr10.arraySize = image.arrayCount();
    hdr10.dxgiFormat = directxType(image.pixelFormat);
    
    switch (image.textureType) {
        case MyMTLTextureType1DArray:
            hdr.caps |= DDSCAPS_COMPLEX;
            
            hdr10.resourceDimension = DDS_DIMENSION_TEXTURE1D;
            break;
        case MyMTLTextureTypeCube:
        case MyMTLTextureTypeCubeArray:
            hdr.caps |= DDSCAPS_COMPLEX;
            hdr.caps2 = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALLFACES;
            
            hdr10.miscFlag = DDS_RESOURCE_MISC_TEXTURECUBE;
            hdr10.resourceDimension = DDS_DIMENSION_TEXTURE2D;
            break;
        case MyMTLTextureType2D:
            if (image.mipCount() > 1)
                hdr.caps |= DDSCAPS_COMPLEX;
            hdr10.resourceDimension = DDS_DIMENSION_TEXTURE2D;
            break;
        case MyMTLTextureType2DArray:
            hdr.caps |= DDSCAPS_COMPLEX;
            hdr10.resourceDimension = DDS_DIMENSION_TEXTURE2D;
            break;
        case MyMTLTextureType3D:
            hdr.caps |= DDSCAPS_COMPLEX;
            hdr.caps2 = DDSCAPS2_VOLUME;
            
            hdr10.resourceDimension = DDS_DIMENSION_TEXTURE3D;
            break;
    }
    
    // fill out in the format fields
    if (!isBlockFormat(image.pixelFormat)) {
        if (isColorFormat(image.pixelFormat)) {
            bool hasG = numChannelsOfFormat(image.pixelFormat) >= 2;
            bool hasB = numChannelsOfFormat(image.pixelFormat) >= 3;
            
            format.flags |= DDSPF_RGB;
            // supposed to include alpha bits too
            format.RGBBitCount = blockSizeOfFormat(image.pixelFormat) * 8;
            format.RBitMask = 0x000000ff;
            format.GBitMask = hasG ? 0x0000ff00 : 0;
            format.BBitMask = hasB ? 0x00ff0000 : 0;
        }
        if (isAlphaFormat(image.pixelFormat)) {
            format.flags |= DDSPF_ALPHAPIXELS;
            format.ABitMask = 0xff000000;
        }
    }
    
    // set premul state
    // The legacy D3DX 10 and D3DX 11 utility libraries will fail to load any .DDS file with miscFlags2 not equal to zero.
    if (image.isPremul()) {
        hdr10.miscFlags2 |= DDS_ALPHA_MODE_PREMULTIPLIED;
    }
    else {
        hdr10.miscFlags2 |= DDS_ALPHA_MODE_STRAIGHT;
    }
    // TODO: also hdr10.miscFlags2 |= DDS_ALPHA_MODE_OPAQUE (alpha full opaque)
    // TODO: also hdr10.miscFlags2 |= DDS_ALPHA_MODE_CUSTOM (raw data in alpha)
        
    bool success = true;
    
    success = success && fileHelper.write((const uint8_t*)&DDS_MAGIC, sizeof(DDS_MAGIC));
    success = success && fileHelper.write((const uint8_t*)&hdr, sizeof(hdr));
    success = success && fileHelper.write((const uint8_t*)&hdr10, sizeof(hdr10));
    
    if (success) {
        // Now write the mip data out in the order dds expects
        // Ugh, dds stores each array item mips, then the next array item mips.
        const uint8_t* imageData = image.fileData;
        for (uint32_t chunkNum = 0; chunkNum < image.totalChunks(); ++chunkNum) {
            
            for (uint32_t mipNum = 0; mipNum < image.mipCount(); ++mipNum) {
                size_t offset = image.chunkOffset(mipNum, chunkNum);
                size_t mipLength = image.mipLevels[mipNum].length;
                
                success = fileHelper.write(imageData + offset, mipLength);
                if (!success) {
                    break;
                }
            }
            
            if (!success) {
                break;
            }
        }
    }
    
    return success;
}



}  // namespace kram
