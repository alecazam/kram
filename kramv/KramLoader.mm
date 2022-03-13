// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import "KramLoader.h"

#import <TargetConditionals.h>

//#include <vector>
//#include <algorithm> // for max
#include <mutex>

#include "KramLib.h"
//#include "KramLog.h"
//#include "KramImage.h"
//#include "KramFileHelper.h"
//#include "KramMmapHelper.h"
//#include "KTXImage.h"

using namespace kram;
using namespace NAMESPACE_STL;
using namespace simd;

using mymutex = std::recursive_mutex;
using mylock = std::unique_lock<mymutex>;
static mymutex gTextureLock;

string kram::toLower(const string &text)
{
    return string(
        [NSString stringWithUTF8String:text.c_str()].lowercaseString.UTF8String);
}

// defer data need to blit staging MTLBuffer to MTLTexture at the start of
// rendering
struct KramBlit {
    uint32_t w;
    uint32_t h;
    uint32_t chunkNum;
    uint32_t mipLevelNumber;

    uint64_t mipStorageSize;
    uint64_t mipOffset;

    uint32_t textureIndex;
    uint32_t bytesPerRow;
    bool is3D;
};

//-----------------------------------------------

@implementation KramLoader {
    // only one of these for now
    id<MTLBuffer> _buffer;
    uint8_t *_data;
    uint32_t _bufferOffset;

    vector<KramBlit> _blits;
    NSMutableArray<id<MTLTexture>> *_blitTextures;
    NSMutableArray<id<MTLTexture>> *_mipgenTextures;
}

- (instancetype)init
{
    self = [super init];

    _blitTextures = [[NSMutableArray alloc] init];
    _mipgenTextures = [[NSMutableArray alloc] init];

    return self;
}

- (nullable id<MTLTexture>)loadTextureFromData:(nonnull NSData *)imageData
                                originalFormat:
                                    (nullable MTLPixelFormat *)originalFormat
{
    return [self loadTextureFromData:(const uint8_t *)imageData.bytes
                     imageDataLength:(int32_t)imageData.length
                      originalFormat:originalFormat];
}

// for macOS/win Intel need to decode astc/etc
// on macOS/arm, the M1 supports all 3 encode formats
#define DO_DECODE TARGET_CPU_X86_64

#if DO_DECODE

// this means format isnt supported on platform, but can be decoded to rgba to
// display
bool isDecodeImageNeeded(MyMTLPixelFormat pixelFormat)
{
    bool needsDecode = false;

    if (isETCFormat(pixelFormat)) {
        needsDecode = true;
    }
    else if (isASTCFormat(pixelFormat)) {
        needsDecode = true;
    }

    return needsDecode;
}

bool decodeImage(const KTXImage &image, KTXImage &imageDecoded)
{
    KramDecoderParams decoderParams;
    KramDecoder decoder;

    if (isETCFormat(image.pixelFormat)) {
        if (!decoder.decode(image, imageDecoded, decoderParams)) {
            return NO;
        }
    }
    else if (isASTCFormat(image.pixelFormat)) {
        if (!decoder.decode(image, imageDecoded, decoderParams)) {
            return NO;
        }
    }
    else {
        assert(false);  // don't call this routine if decode not needed
    }

    // TODO: decode BC format on iOS when not supported, but viewer only on macOS
    // for now

    return YES;
}

#endif

#if SUPPORT_RGB

inline bool isInternalRGBFormat(MyMTLPixelFormat format)
{
    bool isInternal = false;
    switch (format) {
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

inline MyMTLPixelFormat remapInternalRGBFormat(MyMTLPixelFormat format)
{
    MyMTLPixelFormat remapFormat = MyMTLPixelFormatInvalid;
    switch (format) {
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

- (nullable id<MTLTexture>)
    loadTextureFromData:(nonnull const uint8_t *)imageData
        imageDataLength:(int32_t)imageDataLength
         originalFormat:(nullable MTLPixelFormat *)originalFormat
{
    KTXImage image;

    if (imageDataLength > 3 &&
        imageData[0] == 0xff && imageData[1] == 0xd8 && imageData[2] == 0xff )
    {
        KLOGE("kramv", "loader does not support jpg files");
        return nil;
    }
        
    // if png, then need to load from KTXImageData which uses loadpng
    // \x89, P, N, G
    if (imageDataLength > 4 &&
        imageData[0] == 137 && imageData[1] == 'P' && imageData[2] == 'N' && imageData[3] == 'G')
    {
        KTXImageData imageDataReader;
        if (!imageDataReader.open(imageData, imageDataLength, image)) {
            return nil;
        }
        
        return [self loadTextureFromImage:image originalFormat:originalFormat name:""];
    }
    else
    {
    
        // isInfoOnly = true keeps compressed mips on KTX2 and aliases original mip
        // data but have decode etc2/astc path below that uncompressed mips and the
        // rgb conversion path below as well in the viewer. games would want to
        // decompress directly from aliased mmap ktx2 data into staging or have blocks
        // pre-twiddled in hw morton order.

        bool isInfoOnly = true;
        if (!image.open(imageData, imageDataLength, isInfoOnly)) {
            return nil;
        }

        return [self loadTextureFromImage:image originalFormat:originalFormat name:""];
    }
}

- (nullable id<MTLTexture>)loadTextureFromImage:(const KTXImage &)image
                                 originalFormat:
                                     (nullable MTLPixelFormat *)originalFormat
                                           name:(const char*)name
{
#if SUPPORT_RGB
    if (isInternalRGBFormat(image.pixelFormat)) {
        // loads and converts top level mip from RGB to RGBA (RGB0)
        // handles all texture types
        Image rgbaImage;
        if (!rgbaImage.loadImageFromKTX(image))
            return nil;

        // re-encode it as a KTXImage, even though this is just a copy
        KTXImage rbgaImage2;

        ImageInfoArgs dstImageInfoArgs;
        dstImageInfoArgs.textureType = image.textureType;
        dstImageInfoArgs.pixelFormat = remapInternalRGBFormat(image.pixelFormat);
        dstImageInfoArgs.doMipmaps =
            image.mipCount() > 1;  // ignore 0
        dstImageInfoArgs.textureEncoder = kTexEncoderExplicit;

        // set chunk count, so it's explicit
        // the chunks are loaded into a vertical strip
        dstImageInfoArgs.chunksX = 1;
        dstImageInfoArgs.chunksY = dstImageInfoArgs.chunksCount =
            image.totalChunks();

        ImageInfo dstImageInfo;
        dstImageInfo.initWithArgs(dstImageInfoArgs);

        // this will build mips if needed
        KramEncoder encoder;
        if (!encoder.encode(dstImageInfo, rgbaImage, rbgaImage2)) {
            return nil;
        }

        if (originalFormat != nullptr) {
            *originalFormat =
                (MTLPixelFormat)rbgaImage2
                    .pixelFormat;  // TODO: should this return rgbaImage.pixelFormat ?
        }

        return [self blitTextureFromImage:rbgaImage2 name:name];
    }
#endif

    if (originalFormat != nullptr) {
        *originalFormat = (MTLPixelFormat)image.pixelFormat;
    }

#if DO_DECODE
    if (isDecodeImageNeeded(image.pixelFormat)) {
        KTXImage imageDecoded;
        if (!decodeImage(image, imageDecoded)) {
            return nil;
        }

        return [self blitTextureFromImage:imageDecoded name:name];
    }
    else
#endif
    {
        // fast load path directly from mmap'ed data, decompress direct to staging
        return [self blitTextureFromImage:image name:name];
    }
}

/*

static uint32_t numberOfMipmapLevels(const Image& image) {
    uint32_t w = image.width();
    uint32_t h = image.height();
    uint32_t maxDim = MAX(w,h);

    uint32_t numberOfMips = 1;
    while (maxDim > 1) {
        numberOfMips++;
        maxDim = maxDim >> 1;
    }
    return numberOfMips;
}

- (nullable id<MTLTexture>)_loadTextureFromPNGData:(const uint8_t*)data
dataSize:(int32_t)dataSize isSRGB:(BOOL)isSRGB originalFormat:(nullable
MTLPixelFormat*)originalFormat
{
    // can only load 8u and 16u from png, no hdr formats, no premul either, no
props
    // this also doesn't handle strips like done in libkram.

   // Image sourceImage;
    bool isLoaded = LoadPng(data, dataSize, false, false, image);
    if (!isLoaded) {
        return nil;
    }


    // TODO: replace this with code that gens a KTXImage from png (and cpu mips)
    // instead of needing to use autogenmip that has it's own filters (probably
a box)

    id<MTLTexture> texture = [self createTexture:image isPrivate:true];
    if (!texture) {
        return nil;
    }

    if (originalFormat != nullptr) {
        *originalFormat = (MTLPixelFormat)image.pixelFormat;
    }

    // this means KTXImage must hold data
    [self blitTextureFromImage:image];


    // cpu copy the bytes from the data object into the texture
    const MTLRegion region = {
        { 0, 0, 0 }, // MTLOrigin
        { static_cast<NSUInteger>(image.width),
static_cast<NSUInteger>(image.height), 1 }  // MTLSize
    };

    size_t bytesPerRow = 4 * sourceImage.width();

    [texture replaceRegion:region
                mipmapLevel:0
                  withBytes:sourceImage.pixels().data()
                bytesPerRow:bytesPerRow];


    // have to schedule autogen inside render using MTLBlitEncoder
    if (image.mipCount() > 1) {
        [_mipgenTextures addObject: texture];
    }

    return texture;
}
*/

- (BOOL)loadImageFromURL:(nonnull NSURL *)url
                   image:(KTXImage &)image
               imageData:(KTXImageData &)imageData
{
    const char *path = url.absoluteURL.path.UTF8String;

    // TODO: could also ignore extension, and look at header/signature instead
    // files can be renamed to the incorrect extensions
    string filename = toLower(path);

    if (isPNGFilename(filename)) {
        // set title to filename, chop this to just file+ext, not directory
        string filenameShort = filename;
        const char *filenameSlash = strrchr(filenameShort.c_str(), '/');
        if (filenameSlash != nullptr) {
            filenameShort = filenameSlash + 1;
        }

        // now chop off the extension
        filenameShort = filenameShort.substr(0, filenameShort.find_last_of("."));

        // dealing with png means fabricating the format, texture type, and other
        // data
        bool isNormal = false;
        bool isSDF = false;
        if (endsWith(filenameShort, "-n") || endsWith(filenameShort, "_normal")) {
            isNormal = true;
        }
        else if (endsWith(filenameShort, "-sdf")) {
            isSDF = true;
        }

        
        if (!imageData.open(path, image)) {
            return NO;
        }

        // have to adjust the format if srgb
        // PS2022 finally added sRGB gama/iccp blocks to "Save As",
        // but there are a lot of older files where this is not set
        // or Figma always sets sRGB.  So set based on identified type.
        bool doReplaceSrgbFromType = true;
        if (doReplaceSrgbFromType) {
            bool isSRGB = (!isNormal && !isSDF);
            image.pixelFormat = isSRGB ?  MyMTLPixelFormatRGBA8Unorm_sRGB : MyMTLPixelFormatRGBA8Unorm;
        }
    }
    else {
        if (!imageData.open(path, image)) {
            return NO;
        }
    }

    return YES;
}

- (nullable id<MTLTexture>)loadTextureFromURL:(nonnull NSURL *)url
                               originalFormat:
                                   (nullable MTLPixelFormat *)originalFormat
{
    KTXImage image;
    KTXImageData imageData;

    if (![self loadImageFromURL:url image:image imageData:imageData]) {
        return nil;
    }

    return [self loadTextureFromImage:image originalFormat:originalFormat name:imageData.name()];
}

- (nullable id<MTLTexture>)createTexture:(const KTXImage &)image
                               isPrivate:(bool)isPrivate
{
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];

    // Indicate that each pixel has a blue, green, red, and alpha channel, where
    // each channel is an 8-bit unsigned normalized value (i.e. 0 maps to 0.0 and
    // 255 maps to 1.0)
    textureDescriptor.textureType = (MTLTextureType)image.textureType;
    textureDescriptor.pixelFormat = (MTLPixelFormat)image.pixelFormat;

    // Set the pixel dimensions of the texture
    textureDescriptor.width = image.width;
    textureDescriptor.height = MAX(1, image.height);
    textureDescriptor.depth = MAX(1, image.depth);

    textureDescriptor.arrayLength = MAX(1, image.arrayCount());

    // ignoring 0 (auto mip), but might need to support for explicit formats
    // must have hw filtering support for format, and 32f filtering only first
    // appeared on A14/M1 and only get box filtering in API-level filters.  But
    // would cut storage.
    textureDescriptor.mipmapLevelCount =
        MAX(1, image.mipCount());

    // this is needed for blit
    if (isPrivate)
        textureDescriptor.storageMode = MTLStorageModePrivate;

    // Create the texture from the device by using the descriptor
    id<MTLTexture> texture =
        [self.device newTextureWithDescriptor:textureDescriptor];
    if (!texture) {
        KLOGE("kramv", "could not allocate texture");
        return nil;
    }

    return texture;
}

/* just for reference now

// Has a synchronous upload via replaceRegion that only works for shared/managed
(f.e. ktx),
// and another path for private that uses a blitEncoder and must have block
aligned data (f.e. ktxa, ktx2).
// Could repack ktx data into ktxa before writing to temporary file, or when
copying NSData into MTLBuffer.
- (nullable id<MTLTexture>)loadTextureFromImage:(KTXImage &)image
{
    // TODO: about aligning to 4k for base + length
    // http://metalkit.org/2017/05/26/working-with-memory-in-metal-part-2.html

    int32_t w = image.width;
    int32_t h = image.height;
    int32_t d = image.depth;

    int32_t numMips     = MAX(1, image.mipCount());
    int32_t numArrays   = MAX(1, image.arrayCount());
    int32_t numFaces    = MAX(1, image.faceCount());
    int32_t numSlices   = MAX(1, image.depth);

    Int2 blockDims = image.blockDims();

    uint32_t numChunks = image.totalChunks();

    // TODO: reuse staging _buffer and _bufferOffset here, these large
allocations take time vector<uint8_t> mipStorage;
    mipStorage.resize(image.mipLengthLargest() * numChunks); // enough to hold
biggest mip

    //-----------------

    id<MTLTexture> texture = [self createTexture:image isPrivate:false];
    if (!texture) {
        return nil;
    }

    const uint8_t* srcLevelData = image.fileData;

    for (int mipLevelNumber = 0; mipLevelNumber < numMips; ++mipLevelNumber) {
        // there's a 4 byte levelSize for each mipLevel
        // the mipLevel.offset is immediately after this

        const KTXImageLevel& mipLevel = image.mipLevels[mipLevelNumber];

        // this is offset to a given level
        uint64_t mipBaseOffset = mipLevel.offset;

        // unpack the whole level in-place
        if (image.isSupercompressed()) {
            if (!image.unpackLevel(mipLevelNumber, image.fileData +
mipLevel.offset, mipStorage.data())) { return nil;
            }
            srcLevelData = mipStorage.data();

            // going to upload from mipStorage temp array
            mipBaseOffset = 0;
        }

        // only have face, face+array, or slice but this handles all cases
        for (int array = 0; array < numArrays; ++array) {
            for (int face = 0; face < numFaces; ++face) {
                for (int slice = 0; slice < numSlices; ++slice) {

                    uint32_t bytesPerRow = 0;

                    // 1D/1DArray textures set bytesPerRow to 0
                    if (//image.textureType != MyMTLTextureType1D &&
                        image.textureType != MyMTLTextureType1DArray)
                    {
                        // for compressed, bytesPerRow needs to be multiple of
block size
                        // so divide by the number of blocks making up the
height
                        //int xBlocks = ((w + blockDims.x - 1) / blockDims.x);
                        uint32_t yBlocks = ((h + blockDims.y - 1) /
blockDims.y);

                        // Calculate the number of bytes per row in the image.
                        // for compressed images this is xBlocks * blockSize
                        bytesPerRow = (uint32_t)mipLevel.length / yBlocks;
                    }

                    int32_t chunkNum = 0;

                    if (image.header.numberOfArrayElements > 0) {
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

                    // this is size of one face/slice/texture, not the levels
size uint64_t mipStorageSize = mipLevel.length;

                    uint64_t mipOffset = mipBaseOffset + chunkNum *
mipStorageSize;

                    // offset into the level
                    const uint8_t *srcBytes = srcLevelData + mipOffset;

                    {
                        // Note: this only works for managed/shared textures.
                        // For private upload to buffer and then use blitEncoder
to copy to texture.
                        // See KramBlitLoader for that.  This is all synchronous
upload too.
                        //
                        // Note: due to API limit we can only copy one chunk at
a time.  With KramBlitLoader
                        // can copy the whole level to buffer, and then
reference chunks within.

                        bool is3D = image.textureType == MyMTLTextureType3D;

                        // sync cpu copy the bytes from the data object into the
texture MTLRegion region = { { 0, 0, 0 }, // MTLOrigin { (NSUInteger)w,
(NSUInteger)h, 1 }  // MTLSize
                        };

                        size_t bytesPerImage = 0;
                        if (is3D) {
                            region.origin.z = chunkNum;
                            chunkNum = 0;
                            bytesPerImage = mipStorageSize;
                        }

                        [texture replaceRegion:region
                                    mipmapLevel:mipLevelNumber
                                         slice:chunkNum
                                      withBytes:srcBytes
                                    bytesPerRow:bytesPerRow
                                 bytesPerImage:bytesPerImage];

                    }
                }
            }
        }

        mipDown(w, h, d);
    }

    return texture;
}
*/

//--------------------------

- (void)createStagingBufffer:(uint64_t)dataSize
{
    // must be aligned to pagesize() or can't use with newBufferWithBytesNoCopy
    // enough to upload 4k x 4k @ 4 bytes no mips, careful with array and cube
    // that get too big

    // allocate system memory for bufffer, can memcopy to this
    posix_memalign((void **)&_data, getpagesize(), dataSize);

    // allocate memory for circular staging buffer, only need to memcpy to this
    // but need a rolling buffer atop to track current begin/end.

    _buffer =
        [_device newBufferWithBytesNoCopy:_data
                                   length:dataSize
                                  options:MTLResourceStorageModeShared
                              deallocator:^(void *macroUnusedArg(pointer),
                                            NSUInteger macroUnusedArg(length)) {
                                  delete self->_data;
                                  self->_data = nullptr;
                              }];
}

- (void)uploadTexturesIfNeeded:(id<MTLBlitCommandEncoder>)blitEncoder
                 commandBuffer:(id<MTLCommandBuffer>)commandBuffer
{
    mylock lock(gTextureLock);
    
    if (!_blits.empty()) {
        // now upload from staging MTLBuffer to private MTLTexture
        for (const auto &blit : _blits) {
            MTLRegion region = {
                {0, 0, 0},                                   // MTLOrigin
                {(NSUInteger)blit.w, (NSUInteger)blit.h, 1}  // MTLSize
            };

            uint32_t chunkNum = blit.chunkNum;
            if (blit.is3D) {
                region.origin.z = chunkNum;
                chunkNum = 0;
            }

            // assert(blit.textureIndex < _blitTextures.count);
            id<MTLTexture> texture = _blitTextures[blit.textureIndex];

            [blitEncoder copyFromBuffer:_buffer
                           sourceOffset:blit.mipOffset
                      sourceBytesPerRow:blit.bytesPerRow
                    sourceBytesPerImage:blit.mipStorageSize
                             sourceSize:region.size

                              toTexture:texture
                       destinationSlice:chunkNum
                       destinationLevel:blit.mipLevelNumber
                      destinationOrigin:region.origin
                                options:MTLBlitOptionNone];
        }

        // reset the array and buffer offset, so can upload more textures
        _blits.clear();
        [_blitTextures removeAllObjects];

        // TODO: use atomic on this, but have lock now
        uint32_t bufferOffsetCopy = _bufferOffset;
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> /* buffer */) {
            // can only reset this once gpu completes the blits above
            // also guard against addding to this in blitTextureFromImage when
            // completion handler will reset to 0
            if (self->_bufferOffset == bufferOffsetCopy)
                self->_bufferOffset = 0;
        }];
    }
    
    // mipgen after possible initial blit above
    if (_mipgenTextures.count > 0) {
        for (id<MTLTexture> texture in _mipgenTextures) {
            // autogen mips will include srgb conversions, so toggling srgb on/off
            // isn't quite correct
            [blitEncoder generateMipmapsForTexture:texture];
        }

        // reset the arra
        [_mipgenTextures removeAllObjects];
    }
}

- (void)releaseAllPendingTextures
{
    mylock lock(gTextureLock);
    
    _bufferOffset = 0;
    
    _blits.clear();
    
    [_mipgenTextures removeAllObjects];
    [_blitTextures removeAllObjects];
}

inline uint64_t alignOffset(uint64_t offset, uint64_t alignment)
{
    return offset + (alignment - offset % alignment) % alignment;
}

// Has a synchronous upload via replaceRegion that only works for shared/managed
// (f.e. ktx), and another path for private that uses a blitEncoder and must
// have block aligned data (f.e. ktxa, ktx2). Could repack ktx data into ktxa
// before writing to temporary file, or when copying NSData into MTLBuffer.
- (nullable id<MTLTexture>)blitTextureFromImage:(const KTXImage &)image name:(const char*)name
{
    mylock lock(gTextureLock);
    
    if (_buffer == nil) {
        // Was set to 128, but models like FlightHelmet.gltf exceeded that buffer
        static const size_t kStagingBufferSize = 256 * 1024 * 1024;
        
        // this is enough to upload 4k x 4x @ RGBA8u with mips, 8k x 8k compressed
        // with mips @96MB
        [self createStagingBufffer:kStagingBufferSize];
    }

    // TODO: first make sure have enough buffer to upload, otherwise need to queue
    // this image try not to load much until that's established queue would need
    // KTXImage and mmap to stay alive long enough for queue to be completed
    //    if (_bufferOffset != 0) {
    //        return nil;
    //    }

    id<MTLTexture> texture = [self createTexture:image isPrivate:true];
    if (!texture)
        return nil;
    
    // set a label so can identify in captures
    texture.label = [NSString stringWithUTF8String:name];
    
    // this is index where texture will be added
    uint32_t textureIndex = (uint32_t)_blitTextures.count;

    //--------------------------------
    // upload mip levels

    // TODO: about aligning to 4k for base + length
    // http://metalkit.org/2017/05/26/working-with-memory-in-metal-part-2.html

    uint32_t w = image.width;
    uint32_t h = image.height;
    uint32_t d = image.depth;

    uint32_t numMips = MAX(1, image.header.numberOfMipmapLevels);
    uint32_t numArrays = MAX(1, image.header.numberOfArrayElements);
    uint32_t numFaces = MAX(1, image.header.numberOfFaces);
    uint32_t numSlices = MAX(1, image.depth);

    Int2 blockDims = image.blockDims();

    // Note: copy entire decompressed level from KTX, but then upload
    // each chunk of that with separate blit calls below.
    size_t blockSize = image.blockSize();

    vector<uint64_t> bufferOffsets;
    uint8_t *bufferData = (uint8_t *)_buffer.contents;
    const uint8_t *mipData = (const uint8_t *)image.fileData;
    bufferOffsets.resize(image.mipLevels.size());

    uint32_t numChunks = image.totalChunks();

    // This offset, needs to keep incrementing.  Cleared in blit code.
    uint32_t bufferOffset = _bufferOffset;

    for (uint32_t i = 0; i < numMips; ++i) {
        const KTXImageLevel &mipLevel = image.mipLevels[i];

        // pad buffer offset to a multiple of the blockSize
        bufferOffset = alignOffset(bufferOffset, blockSize);

        if ((bufferOffset + mipLevel.length * numChunks) > _buffer.allocatedSize) {
            KLOGE("kramv", "Ran out of buffer space to upload images");
            return nil;
        }
        
        
        // this may have to decompress the level data
        if (!image.unpackLevel(i, mipData + mipLevel.offset,
                               bufferData + bufferOffset)) {
            return nil;
        }

        bufferOffsets[i] = bufferOffset;
        bufferOffset += mipLevel.length * numChunks;
    }

    // everything succeded, so advance the offset
    _bufferOffset = bufferOffset;
    [_blitTextures addObject:texture];

    // defer the blits from buffer until start of render thread when BlitEncoder
    // is available

    for (uint32_t mipLevelNumber = 0; mipLevelNumber < numMips;
         ++mipLevelNumber) {
        // there's a 4 byte levelSize for each mipLevel
        // the mipLevel.offset is immediately after this

        // this is offset to a given level
        const KTXImageLevel &mipLevel = image.mipLevels[mipLevelNumber];

        // only have face, face+array, or slice but this handles all cases
        for (uint32_t array = 0; array < numArrays; ++array) {
            for (uint32_t face = 0; face < numFaces; ++face) {
                for (uint32_t slice = 0; slice < numSlices; ++slice) {
                    uint32_t bytesPerRow = 0;

                    // 1D/1DArray textures set bytesPerRow to 0
                    if (  // image.textureType != MyMTLTextureType1D &&
                        image.textureType != MyMTLTextureType1DArray) {
                        // for compressed, bytesPerRow needs to be multiple of block size
                        // so divide by the number of blocks making up the height
                        // int xBlocks = ((w + blockDims.x - 1) / blockDims.x);
                        uint32_t yBlocks = ((h + blockDims.y - 1) / blockDims.y);

                        // Calculate the number of bytes per row in the image.
                        // for compressed images this is xBlocks * blockSize
                        bytesPerRow = mipLevel.length / yBlocks;
                    }

                    uint32_t chunkNum = 0;

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
                    // Note: no call on MTLBlitEncoder to copy entire level of mips like
                    // glTexImage3D
                    uint64_t mipOffset =
                        bufferOffsets[mipLevelNumber] + chunkNum * mipStorageSize;

                    {
                        bool is3D = image.textureType == MyMTLTextureType3D;

                        _blits.push_back({
                            // use named inits here
                            w, h, chunkNum,

                            mipLevelNumber, mipStorageSize, mipOffset,

                            textureIndex, bytesPerRow,
                            is3D  // could derive from textureIndex lookup
                        });
                    }
                }
            }
        }

        mipDown(w, h, d);
    }

    // this texture cannot be used until buffer uploads complete
    // but those happen at beginning of frame, so can attach to shaders, etc
    return texture;
}

@end
