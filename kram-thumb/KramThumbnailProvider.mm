//
//  KramThumbnailProvider.mm
//  kram-thumb
//
//  Created by Alec on 5/26/21.
//

#import "KramThumbnailProvider.h"

#include "Kram.h"
#include "KramLog.h"
#include "KTXImage.h"
#include "KramImage.h" // for KramDecoder

#include <CoreGraphics/CoreGraphics.h>

//@import Accelerate // for vimage
#import <Accelerate/Accelerate.h>

using namespace kram;

@implementation KramThumbnailProvider

void KLOGF(const char* format, ...) {
    string str;
    
    va_list args;
    va_start(args, format);
    /* int32_t len = */ append_vsprintf(str, format, args);
    va_end(args);
    
    // log here, so it can see it in Console
    NSLog(@"%@", [NSString stringWithUTF8String: str.c_str()]);
}

- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request completionHandler:(void (^)(QLThumbnailReply * _Nullable, NSError * _Nullable))handler {

    // This
    // Second way: Draw the thumbnail into a context passed to your block, set up with Core Graphics's coordinate system.
    handler([QLThumbnailReply replyWithContextSize:request.maximumSize drawingBlock:^BOOL(CGContextRef  _Nonnull context)
    {
        const char* filename = [request.fileURL fileSystemRepresentation];

        if (!(endsWith(filename, ".ktx") || endsWith(filename, ".ktx2"))) {
            KLOGF("kramv %s only supports ktx/ktx2 files\n", filename);
            return NO;
        }
             
        KTXImage image;
        KTXImageData imageData;
        
        if (!imageData.open(filename, image)) {
            KLOGF("kramv %s coould not open file\n", filename);
            
        }
        // no BC6 or ASTC HDR yet for thumbs, just do LDR first
        if (isHdrFormat(image.pixelFormat)) {
            KLOGF("kramv %s doesn't support hdr thumbnails yet\n", filename);
            return NO;
        }
        
        // TODO: hookup to whether content is already premul with alpha
        // will have to come from props. ASTC always 4 channels but may hold other daa.
        bool isPremul = numChannelsOfFormat(image.pixelFormat) >= 4;
        bool isSrgb = isSrgbFormat(image.pixelFormat);
        
        // unpack a level to get the blocks
        uint32_t mipNumber = 0;
        
        uint32_t w, h, d;
        for (uint32_t i = 0; i < image.header.numberOfMipmapLevels; ++i) {
            image.mipDimensions(i, w, h, d);
            if (w > request.maximumSize.width || h > request.maximumSize.height) {
                mipNumber++;
            }
        }
        
        // clamp to smallest
        mipNumber = std::min(mipNumber, image.header.numberOfMipmapLevels);
        image.mipDimensions(mipNumber, w, h, d);
        
        uint32_t chunkNum = 0; // TODO: could embed chunk(s) to gen thumbnail from, cube/array?
        uint32_t numChunks = image.totalChunks();
        
        vector<uint8_t> mipData;
        
         // then decode any blocks to rgba8u, not dealing with HDR formats yet
        if (image.isSupercompressed()) {
            const uint8_t* srcData = image.fileData + image.mipLevels[mipNumber].offset;
            
            mipData.resize(image.mipLevels[mipNumber].length * numChunks);
            uint8_t* dstData = mipData.data();
            if (!image.unpackLevel(mipNumber, srcData, dstData)) {
                KLOGF("kramv %s failed to unpack mip\n", filename);
                return NO;
            }
        }
        
        // now extract the chunk for the thumbnail out of that level
        if (numChunks > 1) {
            macroUnusedVar(chunkNum);
            assert(chunkNum == 0);
            
            // this just truncate to chunk 0 instead of copying chunkNum first
            mipData.resize(image.mipLevels[mipNumber].length);
        }
        
        // new decode the blocks in that chunk to
        KTXImage imageDecoded;
        if (isBlockFormat(image.pixelFormat)) {
            
            KramDecoder decoder;
            KramDecoderParams params;
            
            vector<uint8_t> dstMipData;
            
            // want to just decode one chunk of the level that was unpacked abovve
            if (!decoder.decodeBlocks(w, h, mipData.data(), (int32_t)mipData.size(), image.pixelFormat, dstMipData, params)) {
                KLOGF("kramv %s failed to decode blocks\n", filename);
                return NO;
            }
            
            mipData = dstMipData;
        }
        
        // https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/drawingwithquartz2d/dq_images/dq_images.html#//apple_ref/doc/uid/TP30001066-CH212-TPXREF101

        uint32_t rowBytes = w * sizeof(uint32_t);

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
            KLOGF("kramv %s failed create cgimage\n", filename);
            return NO;
        }
        CGRect rect = CGRectMake(0, 0, w, h);

        // The image is scaled—disproportionately, if necessary—to fit the bounds
        // specified by the rect parameter.
        CGContextDrawImage(context, rect, cgImage);

        return YES;
     }], nil);
}

@end
