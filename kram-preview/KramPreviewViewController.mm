// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import "KramPreviewViewController.h"
#import <Quartz/Quartz.h>

#include <CoreGraphics/CoreGraphics.h>
#import <Accelerate/Accelerate.h>

#include "KramLib.h"

using namespace kram;

// Same code in Preview and Thumbnail
inline NSError* KLOGF(uint32_t code, const char* format, ...) {
    string str;
    
    va_list args;
    va_start(args, format);
    /* int32_t len = */ append_vsprintf(str, format, args);
    va_end(args);
    
    // log here, so it can see it in Console.  But this never appears.
    // How are you supposed to debug failures?  Resorted to passing a unique code into this call.
    // It wasn't originally supposed to generate an NSError
    //NSLog(@"%s", str.c_str());
    
    // Console prints this as <private>, so what's the point of producing a localizedString ?
    // This doesn't seem to work to Console app, but maybe if logs are to terminal
    // sudo log config --mode "level:debug" --subsystem com.ba.kramv
    
    NSString* errorText = [NSString stringWithUTF8String:str.c_str()];
    return [NSError errorWithDomain:@"com.ba.kramv" code:code userInfo:@{NSLocalizedDescriptionKey: errorText}];
}

@interface KramPreviewViewController () <QLPreviewingController>
@end

@implementation KramPreviewViewController

- (NSString *)nibName {
    return @"KramPreviewViewController";
}

- (void)loadView {
    [super loadView];
    // Do any additional setup after loading the view.
}

/*
 * Implement this method and set QLSupportsSearchableItems to YES in the Info.plist of the extension if you support CoreSpotlight.
 *
- (void)preparePreviewOfSearchableItemWithIdentifier:(NSString *)identifier queryString:(NSString *)queryString completionHandler:(void (^)(NSError * _Nullable))handler {
    
    // Perform any setup necessary in order to prepare the view.
    
    // Call the completion handler so Quick Look knows that the preview is fully loaded.
    // Quick Look will display a loading spinner while the completion handler is not called.

    handler(nil);
}
*/

- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable))handler {
    
    // Add the supported content types to the QLSupportedContentTypes array in the Info.plist of the extension.
    // Perform any setup necessary in order to prepare the view.
    
    // The follwing is copied out of Thumbnailer
    
    // No request here, may need to use view size
    uint32_t maxWidth = 128; // request.maximumSize.width
    uint32_t maxHeight = 128; // request.maximumSize.height
    
    const char* filename = [url fileSystemRepresentation];

    NSError* error = nil;
    
    // ignore upper case extensions
    if (!(endsWith(filename, ".ktx") || endsWith(filename, ".ktx2"))) {
        error = KLOGF(1, "kramv %s only supports ktx/ktx2 files\n", filename);
        handler(error);
        return;
    }
         
    KTXImage image;
    KTXImageData imageData;
    TexEncoder decoderType = kTexEncoderUnknown;
   
    if (!imageData.open(filename, image)) {
        error = KLOGF(2, "kramv %s coould not open file\n", filename);
        handler(error);
        return;
    }
    
    // This will set decoder
    auto textureType = MyMTLTextureType2D; // image.textureType
    if (!validateFormatAndDecoder(textureType, image.pixelFormat, decoderType)) {
        error = KLOGF(3, "format decode only supports ktx and ktx2 output");
        handler(error);
        return;
    }
    
    bool isPremul = image.isPremul();
    bool isSrgb = isSrgbFormat(image.pixelFormat);
    
    // unpack a level to get the blocks
    uint32_t mipNumber = 0;
    
    uint32_t mipCount = image.mipCount();
    uint32_t w, h, d;
    for (uint32_t i = 0; i < mipCount; ++i) {
        image.mipDimensions(i, w, h, d);
        if (w > maxWidth || h > maxHeight) {
            mipNumber++;
        }
    }
    
    // clamp to smallest
    mipNumber = std::min(mipNumber, mipCount - 1);
    image.mipDimensions(mipNumber, w, h, d);
    
    uint32_t chunkNum = 0; // TODO: could embed chunk(s) to gen thumbnail from, cube/array?
    uint32_t numChunks = image.totalChunks();
    
    vector<uint8_t> mipData;

    // new decode the blocks in that chunk
    if (isBlockFormat(image.pixelFormat)) {
        
        uint64_t mipLength = image.mipLevels[mipNumber].length;
        
         // then decode any blocks to rgba8u, not dealing with HDR formats yet
        if (image.isSupercompressed()) {
            const uint8_t* srcData = image.fileData + image.mipLevels[mipNumber].offset;
            
            mipData.resize(mipLength * numChunks);
            uint8_t* dstData = mipData.data();
            if (!image.unpackLevel(mipNumber, srcData, dstData)) {
                error = KLOGF(5, "kramv %s failed to unpack mip\n", filename);
                handler(error);
                return;
            }
            
            // now extract the chunk for the thumbnail out of that level
            if (numChunks > 1) {
                macroUnusedVar(chunkNum);
                assert(chunkNum == 0);

                // this just truncate to chunk 0 instead of copying chunkNum first
                mipData.resize(mipLength);
            }
        }
        else
        {
            // this just truncate to chunk 0 instead of copying chunkNum first
            mipData.resize(mipLength);
            
            const uint8_t* srcData = image.fileData + image.mipLevels[mipNumber].offset;
            
            memcpy(mipData.data(), srcData, mipLength);
        }
        
        KramDecoder decoder;
        KramDecoderParams params;
        
        vector<uint8_t> dstMipData;
        
        // only space for one chunk for now
        dstMipData.resize(numChunks * h * w * sizeof(Color));
        
        // want to just decode one chunk of the level that was unpacked abovve
        if (!decoder.decodeBlocks(w, h, mipData.data(), (int32_t)mipData.size(), image.pixelFormat, dstMipData, params)) {
            error = KLOGF(6, "kramv %s failed to decode blocks\n", filename);
            handler(error);
            return;
        }
        
        mipData = dstMipData;
    }
    else if (isExplicitFormat(image.pixelFormat))
    {
        Image image2D;
        if (!image2D.loadThumbnailFromKTX(image, mipNumber)) {
            error = KLOGF(7, "kramv %s failed to convert image to 4 channels\n", filename);
            handler(error);
            return;
        }
        
        // copy from Color back to uint8_t
        uint32_t mipSize = h * w * sizeof(Color);
        mipData.resize(mipSize);
        memcpy(mipData.data(), image2D.pixels().data(), mipSize);
    }
    
    // https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/drawingwithquartz2d/dq_images/dq_images.html#//apple_ref/doc/uid/TP30001066-CH212-TPXREF101

    uint32_t rowBytes = w * sizeof(Color);

    // use vimage in the Accelerate.framework
    // https://developer.apple.com/library/archive/releasenotes/Performance/RN-vecLib/index.html#//apple_ref/doc/uid/TP40001049

    vImage_Buffer buf = { mipData.data(), h, w, rowBytes };

    // Declare the pixel format for the vImage_Buffer
    vImage_CGImageFormat format = {
        .bitsPerComponent   = 8,
        .bitsPerPixel       = 32,
    };
    
    format.bitmapInfo = kCGBitmapByteOrderDefault | (isPremul ? kCGImageAlphaPremultipliedLast : kCGImageAlphaLast);
    format.colorSpace = isSrgb ? CGColorSpaceCreateWithName(kCGColorSpaceSRGB) : CGColorSpaceCreateDeviceRGB();
    
    // don't need to allocate, can requse memory from mip

    // TODO: might want to convert to PNG, but maybe thumbnail system does that automatically?
    // see how big thumbs.db is after running this
    
    vImage_Error err = 0;
    CGImageRef cgImage = vImageCreateCGImageFromBuffer(&buf, &format, NULL, NULL, kvImageNoAllocate, &err);
    if (err) {
        error = KLOGF(8, "kramv %s failed create cgimage\n", filename);
        handler(error);
        return;
    }
    CGRect rect = CGRectMake(0, 0, w, h);

    NSImage* nsImage = [[NSImage alloc] initWithCGImage:cgImage size:rect.size];
    
    NSImageView* nsImageView = (NSImageView*)self.view;
    
    // Change default white to transparent
    [nsImageView.layer setBackgroundColor: [NSColor clearColor].CGColor];
    nsImageView.image = nsImage;

    // This seems to cause plugin to fail
    //CGImageRelease(cgImage);
    
    // TODO: could add description with info from texture (format, etc)
    // self.textView.text = ...
    
    // Call the completion handler so Quick Look knows that the preview is fully loaded.
    // Quick Look will display a loading spinner while the completion handler is not called.
    
    handler(nil);
}

@end

