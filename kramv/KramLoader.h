// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#if __has_feature(modules)
@import Foundation;
@import Metal;
#else

#import <Foundation/Foundation.h>

// protocol requires imports
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLTexture.h>
#import <Metal/MTLBlitCommandEncoder.h>
#import <Metal/MTLDevice.h>

#endif


// This loads KTX and PNG data synchronously.  Will likely move to only loading KTX files, with a png -> ktx conversion.
// The underlying KTXImage is not yet returned to the caller, but would be useful for prop queries.
@interface KramLoader : NSObject

// from mem, caller must keep data alive
- (nullable id<MTLTexture>)loadTextureFromData:(nonnull const uint8_t *)imageData imageDataLength:(int32_t)imageDataLength originalFormat:(nullable MTLPixelFormat*)originalFormat;

// from mem, if NSData then caller must keep data alive until blit
- (nullable id<MTLTexture>)loadTextureFromData:(nonnull NSData*)imageData originalFormat:(nullable MTLPixelFormat*)originalFormat;

// from url (mmap)
- (nullable id<MTLTexture>)loadTextureFromURL:(nonnull NSURL *)url originalFormat:(nullable MTLPixelFormat*)originalFormat;

@property (retain, nonatomic, readwrite, nonnull) id<MTLDevice> device;

// test this after load, and use a MTLBlitEncoder to autogen mips
@property (nonatomic, readwrite, getter=isMipgenNeeded) BOOL mipgenNeeded;

@end

//-------------------------------------

// This loads KTX and PNG data synchronously.  Will likely move to only loading KTX files, with a png -> ktx conversion.
// The underlying KTXImage is not yet returned to the caller, but would be useful for prop queries.
@interface KramBlitLoader : NSObject

@property (retain, nonatomic, readwrite, nonnull) id<MTLDevice> device;

@end

//-------------------------------------

// for toLower
#include <string>

namespace kram {
    using namespace std;

    // provide access to lowercase strings
    string toLower(const string& text);
}
