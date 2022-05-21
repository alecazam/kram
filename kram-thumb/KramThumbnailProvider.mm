// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import "KramThumbnailProvider.h"
#include "KramLib.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <Accelerate/Accelerate.h> // for vImage

using namespace kram;
using namespace NAMESPACE_STL;

@implementation KramThumbnailProvider

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
    return [NSError errorWithDomain:@"com.ba.kramv" code:code userInfo:@{NSLocalizedDescriptionKey:errorText}];
}

struct ImageToPass
{
    KTXImage image;
    KTXImageData imageData;
};

- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request completionHandler:(void (^)(QLThumbnailReply * _Nullable, NSError * _Nullable))handler {

    //  Draw the thumbnail into a context passed to your block, set up with Core Graphics's coordinate system.
    
    const char* filename = [request.fileURL fileSystemRepresentation];

    // DONE: could return NSError to caller if non-null
    NSError* error = nil;
    string errorText;
    
    // TODO: use first x-many bytes also to validate, open will do that
    if (!isSupportedFilename(filename)) {
        error = KLOGF(1, "kramv %s only supports ktx,ktx2,dds,png files\n", filename);
        handler(nil, error);
        return;
    }
        
    shared_ptr<ImageToPass> imageToPass = make_shared<ImageToPass>();
    TexEncoder decoderType = kTexEncoderUnknown;
    uint32_t imageWidth, imageHeight;
    
    {
        KTXImage& image = imageToPass->image;
        KTXImageData& imageData = imageToPass->imageData;
        
        if (!imageData.open(filename, image)) {
            error = KLOGF(2, "kramv %s coould not open file\n", filename);
            handler(nil, error);
            return;
        }
        
        // This will set decoder
        auto textureType = MyMTLTextureType2D; // image.textureType
        if (!validateFormatAndDecoder(textureType, image.pixelFormat, decoderType)) {
            error = KLOGF(3, "format decode only supports ktx and ktx2 output");
            handler(nil, error);
            return;
        }
        
        imageWidth = NAMESPACE_STL::max(1U, image.width);
        imageHeight = NAMESPACE_STL::max(1U, image.height);
    }

    // This is retina factor
    float requestScale = request.scale;
    
    // One of the sides must match maximumSize, but can have
    // different aspect ratios below that on a given sides.
    NSSize contextSize = request.maximumSize;
   
    // compute w/h from aspect ratio of image
    float requestWidth, requestHeight;
    
    float imageAspect = imageWidth / (float)imageHeight;
    if (imageAspect >= 1.0f)
    {
        requestWidth = contextSize.width;
        requestHeight = NAMESPACE_STL::clamp((contextSize.width / imageAspect), 1.0, contextSize.height);
    }
    else
    {
        requestWidth = NAMESPACE_STL::clamp((contextSize.height * imageAspect), 1.0, contextSize.width);
        requestHeight = contextSize.height;
    }
    
    // will be further scaled by requestScale
    contextSize = CGSizeMake(requestWidth, requestHeight);
    
    handler([QLThumbnailReply replyWithContextSize:contextSize drawingBlock:^BOOL(CGContextRef  _Nonnull context)
    {
        KTXImage& image = imageToPass->image;
        
        bool isPremul = image.isPremul();
        bool isSrgb = isSrgbFormat(image.pixelFormat);
        
        //-----------------
        
        // unpack a level to get the blocks
        uint32_t mipNumber = 0;
        uint32_t mipCount = image.mipCount();
        
        uint32_t w, h, d;
        for (uint32_t i = 0; i < mipCount; ++i) {
            image.mipDimensions(i, w, h, d);
            if (w > request.maximumSize.width || h > request.maximumSize.height) {
                mipNumber++;
            }
        }
        
        // clamp to smallest
        mipNumber = std::min(mipNumber, mipCount - 1);
        image.mipDimensions(mipNumber, w, h, d);
        
        //-----------------
        
        uint32_t chunkNum = 0; // TODO: could embed chunk(s) to gen thumbnail from, cube/array?
        uint32_t numChunks = image.totalChunks();
        
        vector<uint8_t> mipData;
        
        // now decode the blocks in that chunk to Color
        if (isBlockFormat(image.pixelFormat)) {
            
            // then decode any blocks to rgba8u, not dealing with HDR formats yet
            uint64_t mipLength = image.mipLevels[mipNumber].length;

            if (image.isSupercompressed()) {
                const uint8_t* srcData = image.fileData + image.mipLevels[mipNumber].offset;

                mipData.resize(mipLength * numChunks);
                uint8_t* dstData = mipData.data();
                if (!image.unpackLevel(mipNumber, srcData, dstData)) {
                   //KLOGF("kramv %s failed to unpack mip\n", filename);
                   return NO;
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
            params.decoder = decoderType;
            
            // TODO: should honor swizzle in the ktx image
            // TODO: probaby need an snorm rgba format to convert the snorm versions, so they're not all red
            // if sdf, will be signed format and that will stay red
            
            switch(image.pixelFormat)
            {
                // To avoid showing single channel content in red, replicate to rgb
                case MyMTLPixelFormatBC4_RUnorm:
                case MyMTLPixelFormatEAC_R11Unorm:
                    params.swizzleText = "rrr1";
                    break;
                    
                default:
                    break;
            }
            
            vector<uint8_t> dstMipData;
            
            // only space for one chunk for now
            dstMipData.resize(h * w * sizeof(Color));
            
            // want to just decode one chunk of the level that was unpacked abovve
            if (!decoder.decodeBlocks(w, h, mipData.data(), (int32_t)mipData.size(), image.pixelFormat, dstMipData, params)) {
                // Can't return NSError
                //error = KLOGF("kramv %s failed to decode blocks\n", filename);
                return NO;
            }
            
            // copy over original encoded data
            mipData = dstMipData;
        }
        else if (isExplicitFormat(image.pixelFormat)) {
            // explicit formats like r/rg/rgb and 16f/32F need to be converted to rgba8 here
            // this should currently clamp, but could do range tonemap, see Image::convertToFourChannel()
            // but this needs to be slightly different.  This will decompress mip again
            
            Image image2D;
            if (!image2D.loadThumbnailFromKTX(image, mipNumber)) {
                //KLOGF("kramv %s failed to convert image to 4 channels\n", filename);
                return NO;
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
        
        // don't need to allocate, can reuse memory from mip
        bool skipPixelCopy = true;
        
        vImage_Error err = 0;
        CGImageRef cgImage = vImageCreateCGImageFromBuffer(&buf, &format, NULL, NULL, skipPixelCopy ? kvImageNoAllocate : kvImageNoFlags, &err);
        if (err) {
            // Can't return NSError
            //error = KLOGF("kramv %s failed create cgimage\n", filename);
            return NO;
        }
        
        CGRect rect = CGRectMake(0, 0,
                                 (uint32_t)roundf(contextSize.width * requestScale),
                                 (uint32_t)roundf(contextSize.height * requestScale));

        // Default is white, but that messes up all content that uses alpha
        // and doesn't match the preview code or kramv background (or Preview).
        CGContextSetFillColorWithColor(context, CGColorGetConstantColor(kCGColorBlack));
        CGContextFillRect(context, rect);
        
        // TODO: should this clear to NSColor clearColor ?
        // don't want default white?
        
        // The image is scaled—disproportionately
        
        //CGContextSetBlendMode(context, kCGBlendModeCopy);
        CGContextSetBlendMode(context, kCGBlendModeNormal);
        
        CGContextDrawImage(context, rect, cgImage);

        // This seems to cause plugin to fail
        // Needed?
        if (!skipPixelCopy)
            CGImageRelease(cgImage);
        
        return YES;
     }], nil);
}

@end
