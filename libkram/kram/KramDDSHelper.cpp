// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramDDSHelper.h"

#include "KTXImage.h"
#include "KramFileHelper.h"

namespace kram {
using namespace NAMESPACE_STL;

const uint32_t DDS_MAGIC = 0x20534444; // "DDS "

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
    ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
    ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))

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
    //DDSPF_LUMINANCE =   0x00020000,
    //DDSPF_ALPHA =       0x00000002,
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
    
    FOURCC_DX10 = MAKEFOURCC('D', 'X', '1', '0'),
    
    // dx10 misc2 flags
    DDS_ALPHA_MODE_UNKNOWN = 0,
    DDS_ALPHA_MODE_STRAIGHT = 1,
    DDS_ALPHA_MODE_PREMULTIPLIED = 2,
    DDS_ALPHA_MODE_OPAQUE = 3,
    DDS_ALPHA_MODE_CUSTOM = 4,
    
    // Not worth support dx9-style files, these don't even hold srgb state
    //FOURCC_BC1 = MAKEFOURCC('D', 'X', 'T', '1'),
    //FOURCC_BC3 = MAKEFOURCC('D', 'X', 'T', '5'),
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

bool DDSHelper::load(const uint8_t* data, size_t dataSize, KTXImage& image, bool isInfoOnly)
{
    const uint32_t magicSize = sizeof(uint32_t);
    uint32_t mipDataOffset = magicSize + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10);
    
    if (dataSize <= mipDataOffset) {
        KLOGE("kram", "bad dataSize too small %d <= %d", dataSize, mipDataOffset);
        return false;
    }
    
    const uint32_t& magic = *(const uint32_t*)data;
    const DDS_HEADER& hdr = *(const DDS_HEADER*)(data + magicSize);
    const DDS_HEADER_DXT10& hdr10 = *(const DDS_HEADER_DXT10*)(data + magicSize + sizeof(DDS_HEADER));
    const DDS_PIXELFORMAT& format = hdr.ddspf;
    
    if (magic != DDS_MAGIC) {
        KLOGE("kram", "bad magic number 0x%08X", magic);
        return false;
    }

    // only load DX10 formatted DDS for now
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
    if (format.fourCC != FOURCC_DX10) {
        KLOGE("kram", "format.fourCC 0x%08X must be DX10", format.fourCC);
        return false;
    }
    
    // Kram only supports a subset of DDS formats
    auto pixelFormat = directxToMetalFormat(hdr10.dxgiFormat);
    if (pixelFormat == MyMTLPixelFormatInvalid) {
        KLOGE("kram", "bad format.dxgiFormat %d", hdr10.dxgiFormat);
        return false;
    }
    
    // make sure to copy mips/slices from DDS array-ordered to mip-ordered for KTX
    uint32_t width = (hdr.flags & DDSD_WIDTH) ? hdr.width : 1;
    uint32_t height = (hdr.flags & DDSD_HEIGHT) ? hdr.height : 1;
    uint32_t depth = (hdr.flags & DDSD_DEPTH) ? hdr.depth : 1;
    
    uint32_t mipCount = (hdr.flags & DDSD_MIPMAPCOUNT) ? hdr.mipMapCount : 1;
    uint32_t arrayCount = hdr10.arraySize;
    
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
    
    bool isCube = (hdr10.miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE);
    bool isArray = arrayCount > 1;
    
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
            break;
    }
    
    // transfer premul setting, would like to not depend on "info" to carry this
    bool isPremul = (hdr10.miscFlags2 & DDS_ALPHA_MODE_PREMULTIPLIED) != 0;
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
