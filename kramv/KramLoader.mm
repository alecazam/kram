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

#include "Kram.h"
#include "KramLog.h"
#include "KramImage.h"
#include "KramFileHelper.h"
#include "KramMmapHelper.h"
#include "KTXImage.h"

using namespace kram;
using namespace std;
using namespace simd;


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

- (BOOL)decodeImageIfNeeded:(KTXImage&)image data:(vector<uint8_t>&)data
{
#if DO_DECODE
    MyMTLPixelFormat format = image.pixelFormat;

    // decode to disk, and then load that in place of original
    // MacIntel can only open BC and explicit formats.
    FileHelper decodedTmpFile;

    bool useDecode = false;
    if (isETCFormat(format)) {
        if (!decodedTmpFile.openTemporaryFile(".ktx", "w+")) {
            return NO;
        }
        
        Image imageDecode;
        if (!imageDecode.decode(image, decodedTmpFile.pointer(), kTexEncoderEtcenc, false, "")) {
            return NO;
        }
        useDecode = true;
    }
    else if (isASTCFormat(format)) {
        if (!decodedTmpFile.openTemporaryFile(".ktx", "w+")) {
            return NO;
        }
        
        Image imageDecode;
        if (!imageDecode.decode(image, decodedTmpFile.pointer(), kTexEncoderAstcenc, false, "")) {
            return NO;
        }
    
        useDecode = true;
    }
    
    // TODO: decode BC format on iOS when not supported, but viewer only on macOS for now

    if (useDecode) {
        FILE* fp = decodedTmpFile.pointer();
        
        size_t size = decodedTmpFile.size();
        if (size <= 0) {
            return NO;
        }
        
        data.resize(size);
        
        // have to pull into buffer, this only works with sync load path for now
        rewind(fp);
        
        size_t readBytes = fread(data.data(), 1, size, fp);
        if (readBytes != size) {
            fprintf(stderr, "%s\n", strerror(errno));
            
            return NO;
        }
        
        image.skipImageLength = false;
        if (!image.open(data.data(), (int32_t)size)) { // doesn't fail
            return NO;
        }
    }
    
#endif
    return YES;
}
    

- (nullable id<MTLTexture>)loadTextureFromData:(nonnull const uint8_t *)imageData imageDataLength:(int32_t)imageDataLength originalFormat:(nullable MTLPixelFormat*)originalFormat
{
    KTXImage image;
    if (!image.open(imageData, imageDataLength)) {
        return nil;
    }
    
    if (originalFormat != nullptr) {
        *originalFormat = (MTLPixelFormat)image.pixelFormat;
    }
    
    vector<uint8_t> data;
    if (![self decodeImageIfNeeded:image data:data]) {
        return nil;
    }
    
    return [self loadTextureFromImage:image];
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
    bool isLoaded = LoadPng(data, dataSize, sourceImage);
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
    
    // have to schedule this inside main loop
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

    MmapHelper mmapHelper;
    if (!mmapHelper.open(path)) {
        return nil;
    }
               
    // TODO: could also ignore extension, and look at header/signature intead
    // files can be renamed to the incorrect extensions
    string filename = [[url.absoluteURL.path lowercaseString] UTF8String];

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
        
        // TODO: use a MTLView to enable srgb or not, then can load and guess the srgb status
        // at a higher level, and also toggle on/off srgb conversion in the shader.
        
        return [self loadTextureFromPNGData:mmapHelper.addr dataSize:(int32_t)mmapHelper.length isSRGB:isSRGB originalFormat:originalFormat];
    }
    
    // route all data through the version that copies or does sync upload
    return [self loadTextureFromData:mmapHelper.addr imageDataLength:(int32_t)mmapHelper.length originalFormat:originalFormat];
}

- (id<MTLTexture>)createTexture:(KTXImage&)image {
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

    // only do this for viewer, this disables lossless compression
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
        
        mipDown(w, h);
    }
        
    return texture;
}

@end




