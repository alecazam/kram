// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KramLib.h"

#import <Foundation/Foundation.h>

// protocol requires imports
#import <Metal/MTLBlitCommandEncoder.h>
#import <Metal/MTLCommandBuffer.h>
#import <Metal/MTLDevice.h>
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLTexture.h>

namespace kram {
class KTXImage;
class KTXImageData;
}

// This loads KTX/2 and PNG data.  Moving towards KTX/2 files only, with a PNG
// to KTX/2 conversion.
@interface KramLoader : NSObject

// from mem,  copied to MTLBuffer if available, if not caller must keep mem
// alive
- (nullable id<MTLTexture>)
    loadTextureFromData:(nonnull const uint8_t *)imageData
        imageDataLength:(int32_t)imageDataLength
         originalFormat:(nullable MTLPixelFormat *)originalFormat;

// from mem, copied to MTLBuffer if available, if not caller must keep mem alive
- (nullable id<MTLTexture>)loadTextureFromData:(nonnull NSData *)imageData
                                originalFormat:
                                    (nullable MTLPixelFormat *)originalFormat;

// load from a KTXImage
- (nullable id<MTLTexture>)loadTextureFromImage:(const kram::KTXImage &)image
                                 originalFormat:
                                     (nullable MTLPixelFormat *)originalFormat;

// load into KTXImage and KTXImageData, can use with loadTextureFromImage
- (BOOL)loadImageFromURL:(nonnull NSURL *)url
                   image:(kram::KTXImage &)image
               imageData:(kram::KTXImageData &)imageData;

// from url (mmap)
- (nullable id<MTLTexture>)loadTextureFromURL:(nonnull NSURL *)url
                               originalFormat:
                                   (nullable MTLPixelFormat *)originalFormat;

// handle auto-mipgen and upload mips from staging MTLBuffer to mips of various
// private MTLTexture
- (void)uploadTexturesIfNeeded:(nonnull id<MTLBlitCommandEncoder>)blitEncoder
                 commandBuffer:(nonnull id<MTLCommandBuffer>)commandBuffer;

@property(retain, nonatomic, readwrite, nonnull) id<MTLDevice> device;

@end

//-------------------------------------

// for toLower
//#include <string>

namespace kram {
using namespace NAMESPACE_STL;

// provide access to lowercase strings
string toLower(const string &text);
}
