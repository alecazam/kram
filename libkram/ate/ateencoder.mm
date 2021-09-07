#import "ateencoder.h"

#if COMPILE_ATE

#include <vector>
#include "KTXImage.h" // for MyMTLPixelFormat

// this contains ATE encoder (libate.dylib)
#include <AppleTextureEncoder.h>
#include <cassert>

namespace kram {

// this is enum without availability
OS_ENUM( my_at_block_format, unsigned long,
    my_at_block_format_invalid          = 0,           ///< MTLPixelFormatInvalid

    // v1 formats
    my_at_block_format_astc_4x4_ldr    ,       ///< MTLPixelFormatASTC_4x4_LDR
    my_at_block_format_astc_5x4_ldr    ,       ///< MTLPixelFormatASTC_5x4_LDR, decode only
    my_at_block_format_astc_5x5_ldr    ,       ///< MTLPixelFormatASTC_5x5_LDR, decode only
    my_at_block_format_astc_6x5_ldr    ,       ///< MTLPixelFormatASTC_6x5_LDR, decode only
    my_at_block_format_astc_6x6_ldr    ,       ///< MTLPixelFormatASTC_6x6_LDR, decode only
    my_at_block_format_astc_8x5_ldr    ,       ///< MTLPixelFormatASTC_8x5_LDR, decode only
    my_at_block_format_astc_8x6_ldr    ,       ///< MTLPixelFormatASTC_8x6_LDR, decode only
    my_at_block_format_astc_8x8_ldr    ,       ///< MTLPixelFormatASTC_8x8_LDR
    my_at_block_format_astc_10x5_ldr   ,       ///< MTLPixelFormatASTC_10x5_LDR, decode only
    my_at_block_format_astc_10x6_ldr   ,       ///< MTLPixelFormatASTC_10x6_LDR, decode only
    my_at_block_format_astc_10x8_ldr   ,       ///< MTLPixelFormatASTC_10x5_LDR, decode only
    my_at_block_format_astc_10x10_ldr  ,       ///< MTLPixelFormatASTC_10x10_LDR, decode only
    my_at_block_format_astc_12x10_ldr  ,       ///< MTLPixelFormatASTC_12x10_LDR, decode only
    my_at_block_format_astc_12x12_ldr  ,       ///< MTLPixelFormatASTC_12x12_LDR, decode only

    // v2 formats, but only for decode so basically pointless
    // would have to modify decode to store to float data if these do work
    my_at_block_format_astc_4x4_hdr     = 17,  ///< MTLPixelFormatASTC_4x4_HDR, decode only
    my_at_block_format_astc_5x4_hdr    ,       ///< MTLPixelFormatASTC_5x4_HDR, decode only
    my_at_block_format_astc_5x5_hdr    ,       ///< MTLPixelFormatASTC_5x5_HDR, decode only
    my_at_block_format_astc_6x5_hdr    ,       ///< MTLPixelFormatASTC_6x5_HDR, decode only
    my_at_block_format_astc_6x6_hdr    ,       ///< MTLPixelFormatASTC_6x6_HDR, decode only
    my_at_block_format_astc_8x5_hdr    ,       ///< MTLPixelFormatASTC_8x5_HDR, decode only
    my_at_block_format_astc_8x6_hdr    ,       ///< MTLPixelFormatASTC_8x6_HDR, decode only
    my_at_block_format_astc_8x8_hdr    ,       ///< MTLPixelFormatASTC_8x8_HDR, decode only
    my_at_block_format_astc_10x5_hdr   ,       ///< MTLPixelFormatASTC_10x5_HDR, decode only
    my_at_block_format_astc_10x6_hdr   ,       ///< MTLPixelFormatASTC_10x6_HDR, decode only
    my_at_block_format_astc_10x8_hdr   ,       ///< MTLPixelFormatASTC_10x5_HDR, decode only
    my_at_block_format_astc_10x10_hdr  ,       ///< MTLPixelFormatASTC_10x10_HDR, decode only
    my_at_block_format_astc_12x10_hdr  ,       ///< MTLPixelFormatASTC_12x10_HDR, decode only
    my_at_block_format_astc_12x12_hdr  ,       ///< MTLPixelFormatASTC_12x12_HDR, decode only
  
    // v2 formats
    my_at_block_format_bc1              = 33,  ///< MTLPixelFormatBC1_RGBA
    my_at_block_format_bc2             ,       ///< MTLPixelFormatBC2_RGBA
    my_at_block_format_bc3             ,       ///< MTLPixelFormatBC3_RGBA - not supported, maybe decode
    my_at_block_format_bc4             ,       ///< MTLPixelFormatBC4_RUnorm
    my_at_block_format_bc4s            ,       ///< MTLPixelFormatBC4_RSnorm
    my_at_block_format_bc5             ,       ///< MTLPixelFormatBC5_RGUnorm
    my_at_block_format_bc5s            ,       ///< MTLPixelFormatBC5_RGSnorm
    my_at_block_format_bc6             ,       ///< MTLPixelFormatBC6H_RGBFloat - not supported, maybe decode
    my_at_block_format_bc6u            ,       ///< MTLPixelFormatBC6H_RGBUFloat  - not supported, maybe decode
    my_at_block_format_bc7             ,       ///< MTLPixelFormatBC7_RGBAUnorm

    // always last
    my_at_block_format_count                                          /* CAUTION: value subject to change! */
);

// this can't take MTLPixelFormat, since ASTC not defined on macOS, and BC not defined on iOS
inline my_at_block_format_t pixelToEncoderFormat(MyMTLPixelFormat format, bool isBCSupported) {
    my_at_block_format_t encoderFormat = my_at_block_format_invalid;
 
    if (isBCSupported) {
        switch(format) {
            // BC
            case MyMTLPixelFormatBC1_RGBA:
            case MyMTLPixelFormatBC1_RGBA_sRGB:
                encoderFormat = my_at_block_format_bc1; break;
            
            // support this?
    //        case MyMTLPixelFormatBC2_RGBA:
    //        case MyMTLPixelFormatBC2_RGBA_sRGB:
    //            encoderFormat = my_at_block_format_bc2; break;
            
            case MyMTLPixelFormatBC3_RGBA:
            case MyMTLPixelFormatBC3_RGBA_sRGB:
                encoderFormat = my_at_block_format_bc3; break;
            
            // Note: remapping unorm inputs to snorm outside of call
            case MyMTLPixelFormatBC4_RUnorm:
                encoderFormat = my_at_block_format_bc4; break;
            case MyMTLPixelFormatBC4_RSnorm:
                encoderFormat = my_at_block_format_bc4s; break;
                
            case MyMTLPixelFormatBC5_RGUnorm:
                encoderFormat = my_at_block_format_bc5; break;
            case MyMTLPixelFormatBC5_RGSnorm:
                encoderFormat = my_at_block_format_bc5s; break;
                
            // hdr
            case MyMTLPixelFormatBC6H_RGBUfloat:
                encoderFormat = my_at_block_format_bc6u; break;
            case MyMTLPixelFormatBC6H_RGBFloat:
                encoderFormat = my_at_block_format_bc6; break;
           
            // ldr - can encode rgb or rgba per block
            case MyMTLPixelFormatBC7_RGBAUnorm:
            case MyMTLPixelFormatBC7_RGBAUnorm_sRGB:
                encoderFormat = my_at_block_format_bc7; break;
                
            default:
             break;
        }
    }
    
    if (encoderFormat == my_at_block_format_invalid) {
        switch(format) {
            // ASTC
            case MyMTLPixelFormatASTC_4x4_sRGB:
            case MyMTLPixelFormatASTC_4x4_LDR:
                encoderFormat = my_at_block_format_astc_4x4_ldr; break;
                
            case MyMTLPixelFormatASTC_8x8_sRGB:
            case MyMTLPixelFormatASTC_8x8_LDR:
                encoderFormat = my_at_block_format_astc_8x8_ldr; break;
             
            default:
                break;
        }
    }
    
    return encoderFormat;
}

// this can't take MTLPixelFormat, since ASTC not defined on macOS, and BC not defined on iOS
inline my_at_block_format_t pixelToDecoderFormat(MyMTLPixelFormat format, bool isBCSupported) {
 my_at_block_format_t encoderFormat = my_at_block_format_invalid;
 
    // decoder supports more formats than encoder
    
    if (isBCSupported) {
        switch(format) {
            // BC
            case MyMTLPixelFormatBC1_RGBA:
            case MyMTLPixelFormatBC1_RGBA_sRGB:
                encoderFormat = my_at_block_format_bc1; break;
            
    //        case MyMTLPixelFormatBC2_RGBA:
    //        case MyMTLPixelFormatBC2_RGBA_sRGB:
    //            encoderFormat = my_at_block_format_bc2; break;
            
            case MyMTLPixelFormatBC3_RGBA:
            case MyMTLPixelFormatBC3_RGBA_sRGB:
                encoderFormat = my_at_block_format_bc3; break;
            
            // Note: remapping unorm inputs to snorm outside of call
            case MyMTLPixelFormatBC4_RUnorm:
                encoderFormat = my_at_block_format_bc4; break;
                
            // Can't use bc4s/5s decoding, or must pass a float image to the ate decoder
            // can correct this unorm to snorm after image is loaded?
            case MyMTLPixelFormatBC4_RSnorm:
                encoderFormat = my_at_block_format_bc4; break; // my_at_block_format_bc4s; break;
                
            case MyMTLPixelFormatBC5_RGUnorm:
                encoderFormat = my_at_block_format_bc5; break;
            case MyMTLPixelFormatBC5_RGSnorm:
                encoderFormat = my_at_block_format_bc5; break; // my_at_block_format_bc5s; break;
                
            // hdr
            case MyMTLPixelFormatBC6H_RGBUfloat:
                encoderFormat = my_at_block_format_bc6u; break;
            case MyMTLPixelFormatBC6H_RGBFloat:
                encoderFormat = my_at_block_format_bc6; break;
           
            // ldr - can encode rgb or rgba per block
            case MyMTLPixelFormatBC7_RGBAUnorm:
            case MyMTLPixelFormatBC7_RGBAUnorm_sRGB:
                encoderFormat = my_at_block_format_bc7; break;
                
            default:
             break;
        }
    }
    
    if (encoderFormat == my_at_block_format_invalid) {
        switch(format) {
            // ASTC
            case MyMTLPixelFormatASTC_4x4_sRGB:
            case MyMTLPixelFormatASTC_4x4_LDR:
                encoderFormat = my_at_block_format_astc_4x4_ldr; break;

            case MyMTLPixelFormatASTC_5x5_sRGB:
            case MyMTLPixelFormatASTC_5x5_LDR:
                encoderFormat = my_at_block_format_astc_5x5_ldr; break;

            case MyMTLPixelFormatASTC_6x6_sRGB:
            case MyMTLPixelFormatASTC_6x6_LDR:
                encoderFormat = my_at_block_format_astc_6x6_ldr; break;
                
            case MyMTLPixelFormatASTC_8x8_sRGB:
            case MyMTLPixelFormatASTC_8x8_LDR:
                encoderFormat = my_at_block_format_astc_8x8_ldr; break;
             
            default:
             break;
        }
    }

    return encoderFormat;
}

ATEEncoder::ATEEncoder() {
    uint32_t version = at_encoder_get_version();
    
    // at_block_get_features can get fetures, but that's a v3 availability call
    //const int32_t kVersion1 = 0x011300; // Astc 4x4/8x8 only, min version with correct ATE, iOS 11, macOS 13
    const int32_t kVersion2 = 0x020000; // added BC1457, iOS 13, macOS 15,
    //const int32_t kVersion3 = 0x030001;  // version tested against ios 13.4, macOS 15.4
    // still no hdr encode/decode
    
    if (version < kVersion2) {
        static bool firstTime = true;
        if (firstTime) {
            // The tables of encode/decode may not be correct, or there may be bugs.
            KLOGW("ATEEncoder", "Ate version %X, but expected >= %X.  Disabling ATE BC encode/decode",
                  version, kVersion2);
            firstTime = false;
        }
    }
    else {
        _isBCSupported = true;
        _isHDRDecodeSupported = true;
    }
}

// from libate and AppleTextureEncoder.h
bool ATEEncoder::Encode(uint32_t metalPixelFormat, size_t dstDataSize, int32_t blockDimsY,
                          bool hasAlpha, bool weightChannels,
                          bool isVerbose, int32_t quality,
                          int32_t width, int32_t height, const uint8_t* srcData, uint8_t* dstData)
{
    // blocks must be 16-byte aligned (what about 8 byte formats?)
    if (((uintptr_t)dstData & 15) != 0) {
        KLOGE("ATEEncoder", "encode dstData must be 16-byte aligned");
        return false;
    }

    my_at_block_format_t blockFormat = pixelToEncoderFormat((MyMTLPixelFormat)metalPixelFormat, _isBCSupported);
    if (blockFormat == my_at_block_format_invalid) {
        KLOGE("ATEEncoder", "encode unsupported format");
        return false;
    }
    
    size_t w = width;
    size_t h = height;
    
    size_t hSnapped = ((h + blockDimsY - 1) / blockDimsY);
    
    size_t rowBytes = dstDataSize / hSnapped;
    
    size_t sliceBytes = dstDataSize;

    size_t srcLength = h*(w*4); // assumes incoming data is 4 bytes per pixel
    
    at_alpha_t srcAlphaType = at_alpha_not_premultiplied;
    if (!hasAlpha) {
        srcAlphaType = at_alpha_opaque;
    }
// already premultiltiplied data, so don't want encoder to do that
//    else if (hasAlpha) {
//        srcAlphaType = at_alpha_premultiplied;
//    }
    
    at_alpha_t dstAlphaType = srcAlphaType;
    if (blockFormat == my_at_block_format_bc1 ||
        blockFormat == my_at_block_format_bc4 ||
        blockFormat == my_at_block_format_bc4s ||
        blockFormat == my_at_block_format_bc5 ||
        blockFormat == my_at_block_format_bc5s ||
        blockFormat == my_at_block_format_bc6 ||
        blockFormat == my_at_block_format_bc6u)
    {
        dstAlphaType = at_alpha_opaque;
    }

    at_texel_region_t texelRegion =
    {
        (void*)srcData, /**< A pointer to the top left corner of the region of input data to be encoded. */
        { (uint32_t)w, (uint32_t)h, 1 },  /**< The size (in texels) of the source region to encode. Does not need to be a multiple of a block size. */
        srcLength/h,   /**< The number of bytes from the start of one row to the next. */
        srcLength       /**< The number of bytes from the start of one slice to the next. */
    };

    at_block_buffer_t blockBuffer =
    {
        (void*)dstData,     /**< A pointer to the top left corner of the region of block data to write after encoding. Must be 16-byte aligned. */
        rowBytes,   /**< The number of bytes from the start of one row to the next. Must be a multiple of the block size.*/
        sliceBytes /**< The number of bytes from the start of one slice to the next. Must be a multiple of the block size. */
    };
    
    // All incoming data is rgba8unorm, even signed data.
    // This call can take bgra8u and r/rg/rgba16f data as well (not fp32 or 8s).
    // But no hdr encoders anyways.
    at_encoder_t encoder = at_encoder_create(
        at_texel_format_rgba8_unorm,
        srcAlphaType,
        (at_block_format_t)blockFormat,
        dstAlphaType,
        nil // bg color
    );
    
    // Common error thresholds are in the 2^-10 (fast) to 2^-15 (best quality) range.
    // 0.0 tries all, range 0,1
    float errorThreshold = 0.0f;
    if (quality <= 10) {
        errorThreshold = 1.0f / (float)(1 << 10);
    }
    else if (quality <= 90) {
        errorThreshold = 1.0f / (float)(1 << 12);
    }
    else {
        errorThreshold = 1.0f / (float)(1 << 15);
    }
    
    uint32_t flags = (weightChannels ? at_flags_default : at_flags_weight_channels_equally)
               | at_flags_skip_parameter_checking
               | at_flags_disable_multithreading
               | at_flags_skip_error_calculation;
    
    if (isVerbose) {
        flags |= at_flags_print_debug_info;
    }
                        
    float psnr = at_encoder_compress_texels(
        encoder,
        &texelRegion, // src
        &blockBuffer, // dst
        errorThreshold,
        at_flags_t(flags)
    );
    
    bool success = true;
    if (psnr < 0) {
        KLOGE("ATEEncoder", "encode failed");
        success = false;
    }
    
    return success;
}

bool ATEEncoder::Decode(uint32_t metalPixelFormat, size_t dstDataSize, int32_t blockDimsY,
                        /* bool isSrgb, */ bool isVerbose,
                        int32_t width, int32_t height, const uint8_t* srcData, uint8_t* dstData)
{
    // blocks must be aligned to 16 bytes
    if (((uintptr_t)srcData & 15) != 0) {
        KLOGE("ATEEncoder", "decode srcData must be 16-byte aligned");
        return false;
    }

    my_at_block_format_t blockFormat = pixelToDecoderFormat((MyMTLPixelFormat)metalPixelFormat, _isBCSupported);
    if (blockFormat == my_at_block_format_invalid) {
        KLOGE("ATEEncoder", "decode unsupported format");
        return false;
    }
    
    uint32_t flags = at_flags_skip_parameter_checking
               | at_flags_disable_multithreading;
    
    // Notes say the following.  So
    // Signed or HDR block formats must be paired with a float texel format and cannot be paired with at_flags_srgb_linear_texels.
    
// this is a decode only flag that presumably does srgb -> linear conversion, but leave the texels as is
//    if (isSrgb) {
//        flags |= at_flags_srgb_linear_texels;
//    }
    
    if (isVerbose) {
        flags |= at_flags_print_debug_info;
    }
     
    size_t w = width;
    size_t h = height;
   
    size_t hSnapped = ((h + blockDimsY - 1) / blockDimsY);
    
    size_t rowBytes = dstDataSize / hSnapped;
    
    size_t sliceBytes = dstDataSize;
    
    at_block_buffer_t blockBuffer =
    {
        (void*)srcData,     /**< A pointer to the top left corner of the region of block data to write after encoding. Must be 16-byte aligned. */
        rowBytes,   /**< The number of bytes from the start of one row to the next. Must be a multiple of the block size.*/
        sliceBytes /**< The number of bytes from the start of one slice to the next. Must be a multiple of the block size. */
    };

    // content may be premul, but don't want decoder multiplying
    at_alpha_t srcAlphaType = at_alpha_not_premultiplied;
    // drop the alpha if needed
    if (blockFormat == my_at_block_format_bc1 ||
        blockFormat == my_at_block_format_bc4 ||
        blockFormat == my_at_block_format_bc4s ||
        blockFormat == my_at_block_format_bc5 ||
        blockFormat == my_at_block_format_bc5s ||
        blockFormat == my_at_block_format_bc6 ||
        blockFormat == my_at_block_format_bc6u)
    {
        srcAlphaType = at_alpha_opaque;
    }
    at_alpha_t dstAlphaType = srcAlphaType;
    
    at_encoder_t encoder = at_encoder_create(
        at_texel_format_rgba8_unorm, // TODO: need float buffer for bc6 and astc hdr
        //at_texel_format_bgra8_unorm, see if this decodes properly rgb channels are wrong on astc4x4 decode, alpha okay
        srcAlphaType,
        (at_block_format_t)blockFormat,
        dstAlphaType,
        nil // bg color
    );
    
    size_t dstLength = h*(w*4); // assumes outgoing data is 4 bytes per pixel
    
    at_texel_region_t texelRegion =
    {
        (void*)dstData, /**< A pointer to the top left corner of the region of input data to be encoded. */
        { (uint32_t)w, (uint32_t)h, 1 },  /**< The size (in texels) of the source region to encode. Does not need to be a multiple of a block size. */
        dstLength/h,   /**< The number of bytes from the start of one row to the next. */
        dstLength       /**< The number of bytes from the start of one slice to the next. */
    };
    
    at_error_t error = at_encoder_decompress_texels(
                                   encoder,
                                   &blockBuffer, // src
                                   &texelRegion, // dst
                                   at_flags_t(flags)
                                 );

    // decode is leaving a=60 for some bizarro reason, so correct that
    if (srcAlphaType == at_alpha_opaque)
    {
        for (uint32_t i = 0, iEnd = w*h; i < iEnd; ++i)
        {
            dstData[4*i+3] = 255;
        }
    }
    
    if (error != at_error_success) {
        KLOGE("ATEEncoder", "decode failed");
        return false;
    }
    
    return true;
}


} // namespace

#endif

