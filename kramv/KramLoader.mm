// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import "KramLoader.h"

#import <TargetConditionals.h>

#if __has_feature(modules)
@import simd;
#else
#import <simd/simd.h>
#endif

#include <vector>
#include <algorithm> // for max
#include <mm_malloc.h>

#include "Kram.h"
#include "KramLog.h"
#include "KramImage.h"
#include "KramFileHelper.h"
#include "KramMmapHelper.h"
#include "KTXImage.h"

using namespace kram;
using namespace std;
using namespace simd;

string kram::toLower(const string& text) {
    return string([[[NSString stringWithUTF8String:text.c_str()] lowercaseString] UTF8String]);
}

//-----------------------------------------------
    
// blit path for ktxa is commented out to simplify loader, will move that to an async load
// and simplify the loader API by making this a loader class.

@implementation KramLoader {
    BOOL _isMipgenNeeded;
}

- (nullable id<MTLTexture>)loadTextureFromData:(nonnull NSData*)imageData originalFormat:(nullable MTLPixelFormat*)originalFormat {
    
    return [self loadTextureFromData:(const uint8_t*)imageData.bytes imageDataLength:(int32_t)imageData.length originalFormat:originalFormat];
}

// for macOS/win Intel need to decode astc/etc
// on macOS/arm, the M1 supports all 3 encode formats
#define DO_DECODE TARGET_CPU_X86_64

- (BOOL)decodeImageIfNeeded:(KTXImage&)image imageDecoded:(KTXImage&)imageDecoded useImageDecoded:(bool&)useImageDecoded
{
#if DO_DECODE
    useImageDecoded = false;
    
    Image imageUnused; // TODO: move to only using KTXImage, decode needs to move there
    
    if (isETCFormat(image.pixelFormat)) {
        if (!imageUnused.decode(image, imageDecoded, kTexEncoderEtcenc, false, "")) {
            return NO;
        }
        useImageDecoded = true;
    }
    else if (isASTCFormat(image.pixelFormat)) {
        if (!imageUnused.decode(image, imageDecoded, kTexEncoderAstcenc, false, "")) {
            return NO;
        }
    
        useImageDecoded = true;
    }
    
    // TODO: decode BC format on iOS when not supported, but viewer only on macOS for now
    
#endif
    
    return YES;
}

#if SUPPORT_RGB
inline bool isInternalRGBFormat(MyMTLPixelFormat format) {
    bool isInternal = false;
    switch(format) {
        case MyMTLPixelFormatRGB8Unorm_internal:
        case MyMTLPixelFormatRGB8Unorm_sRGB_internal:
        case MyMTLPixelFormatRGB16Float_internal:
        case MyMTLPixelFormatRGB32Float_internal:
            isInternal = true;
            break;
        default:
            break;
    }
    return isInternal;
}

inline MyMTLPixelFormat remapInternalRGBFormat(MyMTLPixelFormat format) {
    MyMTLPixelFormat remapFormat = MyMTLPixelFormatInvalid;
    switch(format) {
        case MyMTLPixelFormatRGB8Unorm_internal:
            remapFormat = MyMTLPixelFormatRGBA8Unorm;
            break;
        case MyMTLPixelFormatRGB8Unorm_sRGB_internal:
            remapFormat = MyMTLPixelFormatRGBA8Unorm_sRGB;
            break;
        case MyMTLPixelFormatRGB16Float_internal:
            remapFormat = MyMTLPixelFormatRGBA32Float;
            break;
        case MyMTLPixelFormatRGB32Float_internal:
            remapFormat = MyMTLPixelFormatRGBA32Float;
            break;
        default:
            break;
    }
    return remapFormat;
}

#endif

- (nullable id<MTLTexture>)loadTextureFromData:(nonnull const uint8_t *)imageData imageDataLength:(int32_t)imageDataLength originalFormat:(nullable MTLPixelFormat*)originalFormat
{
    KTXImage image;
    
    // true keeps compressed mips on KTX2 and aliases original mip data
    // but have decode etc2/asct path below that uncompressed mips
    // and the rgb conversion path below as well in the viewer.
    // games would want to decompress directly from aliased mmap ktx2 data into staging
    // or have blocks pre-twiddled in hw morton order.
    
    bool isInfoOnly = false;
    if (!image.open(imageData, imageDataLength, isInfoOnly)) {
        return nil;
    }
    
#if SUPPORT_RGB
    if (isInternalRGBFormat(image.pixelFormat)) {
        // loads and converts image to RGBA version
        Image rbgaImage;
        if (!rbgaImage.loadImageFromKTX(image))
            return nil;
        
        // re-encode it as a KTXImage, even though this is just a copy
        KTXImage rbgaImage2;
       
        ImageInfoArgs dstImageInfoArgs;
        dstImageInfoArgs.pixelFormat = remapInternalRGBFormat(image.pixelFormat);
        dstImageInfoArgs.doMipmaps = false;
        dstImageInfoArgs.textureEncoder = kTexEncoderExplicit;
        dstImageInfoArgs.swizzleText = "rgb1";
        
        ImageInfo dstImageInfo;
        dstImageInfo.initWithArgs(dstImageInfoArgs);
       
        if (!rbgaImage.encode(dstImageInfo, rbgaImage2)) {
            return nil;
        }
        
        if (originalFormat != nullptr) {
            *originalFormat = (MTLPixelFormat)rbgaImage2.pixelFormat;
        }
        
        return [self loadTextureFromImage:rbgaImage2];
    }
#endif
   
    
    if (originalFormat != nullptr) {
        *originalFormat = (MTLPixelFormat)image.pixelFormat;
    }
    
    KTXImage imageDecoded;
    bool useImageDecoded = false;
    if (![self decodeImageIfNeeded:image imageDecoded:imageDecoded useImageDecoded:useImageDecoded]) {
        return nil;
    }
    
    return [self loadTextureFromImage:useImageDecoded ? imageDecoded : image];
}

static int32_t numberOfMipmapLevels(const Image& image) {
    int32_t w = image.width();
    int32_t h = image.height();
    int32_t maxDim = MAX(w,h);
    
    int32_t numberOfMips = 1;
    while (maxDim > 1) {
        numberOfMips++;
        maxDim = maxDim >> 1;
    }
    return numberOfMips;
}
    
- (nullable id<MTLTexture>)loadTextureFromPNGData:(const uint8_t*)data dataSize:(int32_t)dataSize isSRGB:(BOOL)isSRGB originalFormat:(nullable MTLPixelFormat*)originalFormat
{
    // can only load 8u and 16u from png, no hdr formats, no premul either, no props
    Image sourceImage;
    bool isLoaded = LoadPng(data, dataSize, false, false, sourceImage);
    if (!isLoaded) {
        return nil;
    }
    
    KTXImage image;
    image.width = sourceImage.width();
    image.height = sourceImage.height();
    image.depth = 0;
    
    image.header.numberOfArrayElements = 0;
    image.header.numberOfMipmapLevels = numberOfMipmapLevels(sourceImage);
    
    image.textureType = MyMTLTextureType2D;
    image.pixelFormat = isSRGB ? MyMTLPixelFormatRGBA8Unorm_sRGB : MyMTLPixelFormatRGBA8Unorm;
    
    // TODO: replace this with code that gens a KTXImage from png (and cpu mips)
    // instead of needing to use autogenmip that has it's own filters (probably a box)
    
    id<MTLTexture> texture = [self createTexture:image];
    if (!texture) {
        return nil;
    }
    
    if (originalFormat != nullptr) {
        *originalFormat = (MTLPixelFormat)image.pixelFormat;
    }
    
    // cpu copy the bytes from the data object into the texture
    int32_t sliceOrArrayOrFace = 0;
    
    const MTLRegion region = {
        { 0, 0, (NSUInteger)sliceOrArrayOrFace }, // MTLOrigin
        { static_cast<NSUInteger>(image.width), static_cast<NSUInteger>(image.height), 1 }  // MTLSize
    };
    
    int32_t bytesPerRow = 4 * sourceImage.width();
    
    [texture replaceRegion:region
                mipmapLevel:0
                  withBytes:sourceImage.pixels()
                bytesPerRow:bytesPerRow];
    
    // have to schedule autogen inside render using MTLBlitEncoder
    if (image.header.numberOfMipmapLevels > 1) {
        _isMipgenNeeded = YES;
    }
    
    return texture;
}

- (BOOL)isMipgenNeeded {
    return _isMipgenNeeded;
}

- (void)setMipgenNeeded:(BOOL)enabled {
    _isMipgenNeeded = enabled;
}

- (nullable id<MTLTexture>)loadTextureFromURL:(nonnull NSURL *)url originalFormat:(nullable MTLPixelFormat*)originalFormat {
    
    const char *path = [url.absoluteURL.path UTF8String];

    // TODO: could also ignore extension, and look at header/signature instead
    // files can be renamed to the incorrect extensions
    string filename = toLower(path);
    
    MmapHelper mmapHelper;
    if (!mmapHelper.open(path)) {
        return nil;
    }
               
    if (endsWithExtension(filename.c_str(), ".png")) {
        // set title to filename, chop this to just file+ext, not directory
        string filenameShort = filename;
        const char* filenameSlash = strrchr(filenameShort.c_str(), '/');
        if (filenameSlash != nullptr) {
            filenameShort = filenameSlash + 1;
        }
        
        // now chop off the extension
        filenameShort = filenameShort.substr(0, filenameShort.find_last_of("."));
        
        // dealing with png means fabricating the format, texture type, and other data
        bool isNormal = false;
        bool isSDF = false;
        if (endsWith(filenameShort, "-n") || endsWith(filenameShort, "_normal")) {
            isNormal = true;
        }
        else if (endsWith(filenameShort, "-sdf")) {
            isSDF = true;
        }
        
        bool isSRGB = (!isNormal && !isSDF);
        
        return [self loadTextureFromPNGData:mmapHelper.data() dataSize:(int32_t)mmapHelper.dataLength() isSRGB:isSRGB originalFormat:originalFormat];
    }
    
    // route all data through the version that copies or does sync upload
    return [self loadTextureFromData:mmapHelper.data() imageDataLength:(int32_t)mmapHelper.dataLength() originalFormat:originalFormat];
}

- (nullable id<MTLTexture>)createTexture:(KTXImage&)image {
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];

    // Indicate that each pixel has a blue, green, red, and alpha channel, where each channel is
    // an 8-bit unsigned normalized value (i.e. 0 maps to 0.0 and 255 maps to 1.0)
    textureDescriptor.textureType = (MTLTextureType)image.textureType;
    textureDescriptor.pixelFormat = (MTLPixelFormat)image.pixelFormat;

    // Set the pixel dimensions of the texture
    textureDescriptor.width = image.width;
    textureDescriptor.height = MAX(1, image.height);
    textureDescriptor.depth = MAX(1, image.depth);

    textureDescriptor.arrayLength = MAX(1, image.header.numberOfArrayElements);

    // ignoring 0 (auto mip), but might need to support for explicit formats
    // must have hw filtering support for format, and 32f filtering only first appeared on A14/M1
    // and only get box filtering in API-level filters.  But would cut storage.
    textureDescriptor.mipmapLevelCount = MAX(1, image.header.numberOfMipmapLevels);

    // only do this for viewer
    // but allows encoded textures to enable/disable their sRGB state.
    // Since the view isn't accurate, will probably pull this out.
    // Keep usageRead set by default.
    //textureDescriptor.usage = MTLTextureUsageShaderRead;
    
    // this was so that could toggle srgb on/off, but mips are built linear and encoded as lin or srgb
    // in the encoded formats so this wouldn't accurately reflect with/without srgb.
    //textureDescriptor.usage |= MTLTextureUsagePixelFormatView;
    
    // Create the texture from the device by using the descriptor
    id<MTLTexture> texture = [self.device newTextureWithDescriptor:textureDescriptor];
    if (!texture) {
        KLOGE("kramv", "could not allocate texture");
        return nil;
    }
    
    return texture;
}

// Has a synchronous upload via replaceRegion that only works for shared/managed (f.e. ktx),
// and another path for private that uses a blitEncoder and must have block aligned data (f.e. ktxa, ktx2).
// Could repack ktx data into ktxa before writing to temporary file, or when copying NSData into MTLBuffer.
- (nullable id<MTLTexture>)loadTextureFromImage:(KTXImage &)image
{
    id<MTLTexture> texture = [self createTexture:image];
    
    //--------------------------------
    // upload mip levels
    
    // TODO: about aligning to 4k for base + length
    // http://metalkit.org/2017/05/26/working-with-memory-in-metal-part-2.html
    
    int32_t w = image.width;
    int32_t h = image.height;
    int32_t d = image.depth;
    
    int32_t numMips     = MAX(1, image.header.numberOfMipmapLevels);
    int32_t numArrays   = MAX(1, image.header.numberOfArrayElements);
    int32_t numFaces    = MAX(1, image.header.numberOfFaces);
    int32_t numSlices   = MAX(1, image.depth);
    
    Int2 blockDims = image.blockDims();
    
    for (int mipLevelNumber = 0; mipLevelNumber < numMips; ++mipLevelNumber) {
        // there's a 4 byte levelSize for each mipLevel
        // the mipLevel.offset is immediately after this
        
        // this is offset to a given level
        const KTXImageLevel& mipLevel = image.mipLevels[mipLevelNumber];
        
        // only have face, face+array, or slice but this handles all cases
        for (int array = 0; array < numArrays; ++array) {
            for (int face = 0; face < numFaces; ++face) {
                for (int slice = 0; slice < numSlices; ++slice) {
    
                    int32_t bytesPerRow = 0;
                    
                    // 1D/1DArray textures set bytesPerRow to 0
                    if ((MTLTextureType)image.textureType != MTLTextureType1D &&
                        (MTLTextureType)image.textureType != MTLTextureType1DArray)
                    {
                        // for compressed, bytesPerRow needs to be multiple of block size
                        // so divide by the number of blocks making up the height
                        //int xBlocks = ((w + blockDims.x - 1) / blockDims.x);
                        int32_t yBlocks = ((h + blockDims.y - 1) / blockDims.y);
                        
                        // Calculate the number of bytes per row in the image.
                        // for compressed images this is xBlocks * blockSize
                        bytesPerRow = (int32_t)mipLevel.length / yBlocks;
                    }
                    
                    int32_t sliceOrArrayOrFace;
                                    
                    if (image.header.numberOfArrayElements > 0) {
                        // can be 1d, 2d, or cube array
                        sliceOrArrayOrFace = array;
                        if (numFaces > 1) {
                            sliceOrArrayOrFace = 6 * sliceOrArrayOrFace + face;
                        }
                    }
                    else {
                        // can be 1d, 2d, or 3d
                        sliceOrArrayOrFace = slice;
                        if (numFaces > 1) {
                            sliceOrArrayOrFace = face;
                        }
                    }
                    
                    // this is size of one face/slice/texture, not the levels size
                    int32_t mipStorageSize = (int32_t)mipLevel.length;
                    
                    int32_t mipOffset = (int32_t)mipLevel.offset + sliceOrArrayOrFace * mipStorageSize;
                    // offset into the level
                    const uint8_t *srcBytes = image.fileData + mipOffset;
            
                    // had blitEncoder support here
                    
                    {
                        // Note: this only works for managed/shared textures.
                        // For private upload to buffer and then use blitEncoder to copy to texture.
                        bool isCubemap = image.textureType == MyMTLTextureTypeCube ||
                                         image.textureType == MyMTLTextureTypeCubeArray;
                        bool is3D = image.textureType == MyMTLTextureType3D;
                        bool is2DArray = image.textureType == MyMTLTextureType2DArray;
                        bool is1DArray = image.textureType == MyMTLTextureType1DArray;
                        
                        // cpu copy the bytes from the data object into the texture
                        MTLRegion region = {
                            { 0, 0, 0 }, // MTLOrigin
                            { (NSUInteger)w,  (NSUInteger)h, 1 }  // MTLSize
                        };
                        
                        // TODO: revist how loading is done to load entire levels
                        // otherwise too many replaceRegion calls.   Data is already packed by mip.
                        
                        if (is1DArray) {
                            [texture replaceRegion:region
                                        mipmapLevel:mipLevelNumber
                                            slice:sliceOrArrayOrFace
                                          withBytes:srcBytes
                                        bytesPerRow:bytesPerRow
                                        bytesPerImage:0];
                        }
                        else if (isCubemap) {
                            [texture replaceRegion:region
                                        mipmapLevel:mipLevelNumber
                                            slice:sliceOrArrayOrFace
                                          withBytes:srcBytes
                                        bytesPerRow:bytesPerRow
                                        bytesPerImage:0];
                        }
                        else if (is3D) {
                            region.origin.z = sliceOrArrayOrFace;
                            
                            [texture replaceRegion:region
                                        mipmapLevel:mipLevelNumber
                                             slice:0
                                          withBytes:srcBytes
                                        bytesPerRow:bytesPerRow
                                     bytesPerImage:mipStorageSize]; // only for 3d
                        }
                        else if (is2DArray) {
                            [texture replaceRegion:region
                                        mipmapLevel:mipLevelNumber
                                             slice:array
                                          withBytes:srcBytes
                                        bytesPerRow:bytesPerRow
                                     bytesPerImage:0];
                        }
                        else {

                            [texture replaceRegion:region
                                        mipmapLevel:mipLevelNumber
                                          withBytes:srcBytes
                                        bytesPerRow:bytesPerRow];
                        }
                    }
                }
            }
        }
        
        mipDown(w, h, d);
    }
        
    return texture;
}

@end

//--------------------------




@implementation KramBlitLoader {
    // this must be created in render, and then do blits into this
    id<MTLBlitCommandEncoder> _blitEncoder;
    id<MTLBuffer> _buffer;
    uint8_t* data;
    size_t dataSize;
}

- (nonnull instancetype)init {
    self = [super init];
    
    // must be aligned to pagesize() or can't use with newBufferWithBytesNoCopy
    // enough to upload 4k x 4k @ 4 bytes no mips, careful with array and cube that get too big
    dataSize = 64*1024*1024;
    
    posix_memalign((void**)&data, getpagesize(), dataSize);
    
    // allocate memory for circular staging buffer, only need to memcpy to this
    // but need a rolling buffer atop to track current begin/end.
    
    _buffer = [_device newBufferWithBytesNoCopy:data
                                            length:dataSize
                                           options:MTLResourceStorageModeShared
                                       deallocator: ^(void *macroUnusedArg(pointer), NSUInteger macroUnusedArg(length)) {
                                            delete data;
                                            }
    ];
    return self;
}

- (nullable id<MTLTexture>)createTexture:(KTXImage&)image {
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];

    // Indicate that each pixel has a blue, green, red, and alpha channel, where each channel is
    // an 8-bit unsigned normalized value (i.e. 0 maps to 0.0 and 255 maps to 1.0)
    textureDescriptor.textureType = (MTLTextureType)image.textureType;
    textureDescriptor.pixelFormat = (MTLPixelFormat)image.pixelFormat;

    // Set the pixel dimensions of the texture
    textureDescriptor.width = image.width;
    textureDescriptor.height = MAX(1, image.height);
    textureDescriptor.depth = MAX(1, image.depth);

    textureDescriptor.arrayLength = MAX(1, image.header.numberOfArrayElements);

    // ignoring 0 (auto mip), but might need to support for explicit formats
    // must have hw filtering support for format, and 32f filtering only first appeared on A14/M1
    // and only get box filtering in API-level filters.  But would cut storage.
    textureDescriptor.mipmapLevelCount = MAX(1, image.header.numberOfMipmapLevels);

    // needed for blit,
    textureDescriptor.storageMode = MTLStorageModePrivate;
    
    // only do this for viewer
    // but allows encoded textures to enable/disable their sRGB state.
    // Since the view isn't accurate, will probably pull this out.
    // Keep usageRead set by default.
    //textureDescriptor.usage = MTLTextureUsageShaderRead;
    
    // this was so that could toggle srgb on/off, but mips are built linear and encoded as lin or srgb
    // in the encoded formats so this wouldn't accurately reflect with/without srgb.
    //textureDescriptor.usage |= MTLTextureUsagePixelFormatView;
    
    // Create the texture from the device by using the descriptor
    id<MTLTexture> texture = [self.device newTextureWithDescriptor:textureDescriptor];
    if (!texture) {
        KLOGE("kramv", "could not allocate texture");
        return nil;
    }
    
    return texture;
}

// Has a synchronous upload via replaceRegion that only works for shared/managed (f.e. ktx),
// and another path for private that uses a blitEncoder and must have block aligned data (f.e. ktxa, ktx2).
// Could repack ktx data into ktxa before writing to temporary file, or when copying NSData into MTLBuffer.
- (nullable id<MTLTexture>)blitTextureFromImage:(KTXImage &)image
{
    id<MTLTexture> texture = [self createTexture:image];
    
    // Note: always starting at 0 here, since kramv is only uploading 1 texture
    // but a real uploader would upload until buffer full, and then reset this back to 0
    // A circular buffer if large enough to support multiple uploads over time.
    // This can be a lot of temporary memory and must complete upload before changing.
    
    uint64_t bufferOffset = 0;
    
    //--------------------------------
    // upload mip levels
    
    // TODO: about aligning to 4k for base + length
    // http://metalkit.org/2017/05/26/working-with-memory-in-metal-part-2.html
    
    int32_t w = image.width;
    int32_t h = image.height;
    int32_t d = image.depth;
    
    int32_t numMips     = MAX(1, image.header.numberOfMipmapLevels);
    int32_t numArrays   = MAX(1, image.header.numberOfArrayElements);
    int32_t numFaces    = MAX(1, image.header.numberOfFaces);
    int32_t numSlices   = MAX(1, image.depth);
    
    Int2 blockDims = image.blockDims();
    
    // Note: copy entire decompressed level from KTX, but then upload
    // each chunk of that with separate blit calls below.
    size_t blockSize = image.blockSize();
    
    vector<uint64_t> bufferOffsets;
    uint8_t* bufferData = (uint8_t*)_buffer.contents;
    const uint8_t* mipData = (const uint8_t*)image.fileData;
    bufferOffsets.resize(image.mipLevels.size());
    
    for (int32_t i = 0; i < numMips; ++i) {
        const KTXImageLevel& mipLevel = image.mipLevels[i];
        
        // pad buffer offset to a multiple of the blockSize
        bufferOffset += (blockSize - 1) - (bufferOffset & blockSize);
        bufferOffsets[i] = bufferOffset;
        bufferOffset += mipLevel.length;
        
        // this may have to decompress the level data
        image.unpackLevel(i, mipData + mipLevel.offset, bufferData + bufferOffset);
    }
    
    // blit encode calls must all be submitted to an encoder
    // but may not have to be on the render thrad?
    
    for (int32_t mipLevelNumber = 0; mipLevelNumber < numMips; ++mipLevelNumber) {
        // there's a 4 byte levelSize for each mipLevel
        // the mipLevel.offset is immediately after this
        
        // this is offset to a given level
        const KTXImageLevel& mipLevel = image.mipLevels[mipLevelNumber];
        
        // only have face, face+array, or slice but this handles all cases
        for (int array = 0; array < numArrays; ++array) {
            for (int face = 0; face < numFaces; ++face) {
                for (int slice = 0; slice < numSlices; ++slice) {
    
                    int32_t bytesPerRow = 0;
                    
                    // 1D/1DArray textures set bytesPerRow to 0
                    if ((MTLTextureType)image.textureType != MTLTextureType1D &&
                        (MTLTextureType)image.textureType != MTLTextureType1DArray)
                    {
                        // for compressed, bytesPerRow needs to be multiple of block size
                        // so divide by the number of blocks making up the height
                        //int xBlocks = ((w + blockDims.x - 1) / blockDims.x);
                        int32_t yBlocks = ((h + blockDims.y - 1) / blockDims.y);
                        
                        // Calculate the number of bytes per row in the image.
                        // for compressed images this is xBlocks * blockSize
                        bytesPerRow = (int32_t)mipLevel.length / yBlocks;
                    }
                    
                    int32_t chunkNum;
                                    
                    if (image.header.numberOfArrayElements > 1) {
                        // can be 1d, 2d, or cube array
                        chunkNum = array;
                        if (numFaces > 1) {
                            chunkNum = 6 * chunkNum + face;
                        }
                    }
                    else {
                        // can be 1d, 2d, or 3d
                        chunkNum = slice;
                        if (numFaces > 1) {
                            chunkNum = face;
                        }
                    }
                    
                    // This is size of one chunk
                    uint64_t mipStorageSize = mipLevel.length;
                    
                    // Have uploaded to buffer in same order visiting chunks.
                    // Note: no call on MTLBlitEncoder to copy entire level of mips like glTexImage3D
                    uint64_t mipOffset =  bufferOffsets[mipLevelNumber] + chunkNum * mipStorageSize;
                    
                    {
                        bool is3D = image.textureType == MyMTLTextureType3D;
                        
                        // cpu copy the bytes from the data object into the texture
                        MTLRegion region = {
                            { 0, 0, 0 }, // MTLOrigin
                            { (NSUInteger)w,  (NSUInteger)h, 1 }  // MTLSize
                        };
                        
                        if (is3D) {
                            region.origin.z = chunkNum;
                            chunkNum = 0;
                        }
                        
                        [_blitEncoder copyFromBuffer:_buffer
                                       sourceOffset:mipOffset
                              sourceBytesPerRow:bytesPerRow
                            sourceBytesPerImage:mipStorageSize
                                     sourceSize:region.size
                         
                                      toTexture:texture
                               destinationSlice:chunkNum
                               destinationLevel:mipLevelNumber
                              destinationOrigin:region.origin
                                        options:MTLBlitOptionNone
                         ];
                    }
                }
            }
        }
        
        mipDown(w, h, d);
    }
        
    // this only affect managed textures
    [_blitEncoder optimizeContentsForGPUAccess:texture];
    
    return texture;
}


@end




