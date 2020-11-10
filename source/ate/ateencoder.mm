#import "ateencoder.h"

#if COMPILE_ATE

#include <vector>
#include "KTXImage.h" // for MyMTLPixelFormat

// this contains ATE encoder (libate.dylib)
#include <AppleTextureEncoder.h>
#include <cassert>

namespace kram {

// this can't take MTLPixelFormat, since ASTC not defined on macOS, and BC not defined on iOS
inline at_block_format_t pixelToEncoderFormat(MyMTLPixelFormat format) {
 at_block_format_t encoderFormat = at_block_format_invalid;
 
    switch(format) {
        // BC
        case MyMTLPixelFormatBC1_RGBA:
        case MyMTLPixelFormatBC1_RGBA_sRGB:
            encoderFormat = at_block_format_bc1; break;
        
        // support this?
//        case MyMTLPixelFormatBC2_RGBA:
//        case MyMTLPixelFormatBC2_RGBA_sRGB:
//            encoderFormat = at_block_format_bc2; break;
        
        case MyMTLPixelFormatBC3_RGBA:
        case MyMTLPixelFormatBC3_RGBA_sRGB:
            encoderFormat = at_block_format_bc3; break;
        
        // Note: remapping unorm inputs to snorm outside of call
        case MyMTLPixelFormatBC4_RUnorm:
            encoderFormat = at_block_format_bc4; break;
        case MyMTLPixelFormatBC4_RSnorm:
            encoderFormat = at_block_format_bc4s; break;
            
        case MyMTLPixelFormatBC5_RGUnorm:
            encoderFormat = at_block_format_bc5; break;
        case MyMTLPixelFormatBC5_RGSnorm:
            encoderFormat = at_block_format_bc5s; break;
            
        // hdr
        case MyMTLPixelFormatBC6H_RGBUfloat:
            encoderFormat = at_block_format_bc6u; break;
        case MyMTLPixelFormatBC6H_RGBFloat:
            encoderFormat = at_block_format_bc6; break;
       
        // ldr - can encode rgb or rgba per block
        case MyMTLPixelFormatBC7_RGBAUnorm:
        case MyMTLPixelFormatBC7_RGBAUnorm_sRGB:
            encoderFormat = at_block_format_bc7; break;
         
        // ASTC
        case MyMTLPixelFormatASTC_4x4_sRGB:
        case MyMTLPixelFormatASTC_4x4_LDR:
            encoderFormat = at_block_format_astc_4x4_ldr; break;

// No encoder
//        case MyMTLPixelFormatASTC_5x5_sRGB:
//        case MyMTLPixelFormatASTC_5x5_LDR:
//        case MyMTLPixelFormatASTC_6x6_sRGB:
//        case MyMTLPixelFormatASTC_6x6_LDR:
            
        case MyMTLPixelFormatASTC_8x8_sRGB:
        case MyMTLPixelFormatASTC_8x8_LDR:
            encoderFormat = at_block_format_astc_8x8_ldr; break;
         
        default:
            assert(false); // unsupported format
         break;
    }

    return encoderFormat;
}

// this can't take MTLPixelFormat, since ASTC not defined on macOS, and BC not defined on iOS
inline at_block_format_t pixelToDecoderFormat(MyMTLPixelFormat format) {
 at_block_format_t encoderFormat = at_block_format_invalid;
 
    // decoder supports more formats than encoder
    
    switch(format) {
        // BC
        case MyMTLPixelFormatBC1_RGBA:
        case MyMTLPixelFormatBC1_RGBA_sRGB:
            encoderFormat = at_block_format_bc1; break;
        
        // support this?
//        case MyMTLPixelFormatBC2_RGBA:
//        case MyMTLPixelFormatBC2_RGBA_sRGB:
//            encoderFormat = at_block_format_bc2; break;
        
        case MyMTLPixelFormatBC3_RGBA:
        case MyMTLPixelFormatBC3_RGBA_sRGB:
            encoderFormat = at_block_format_bc3; break;
        
        // Note: remapping unorm inputs to snorm outside of call
        case MyMTLPixelFormatBC4_RUnorm:
            encoderFormat = at_block_format_bc4; break;
        case MyMTLPixelFormatBC4_RSnorm:
            encoderFormat = at_block_format_bc4s; break;
            
        case MyMTLPixelFormatBC5_RGUnorm:
            encoderFormat = at_block_format_bc5; break;
        case MyMTLPixelFormatBC5_RGSnorm:
            encoderFormat = at_block_format_bc5s; break;
            
        // hdr
        case MyMTLPixelFormatBC6H_RGBUfloat:
            encoderFormat = at_block_format_bc6u; break;
        case MyMTLPixelFormatBC6H_RGBFloat:
            encoderFormat = at_block_format_bc6; break;
       
        // ldr - can encode rgb or rgba per block
        case MyMTLPixelFormatBC7_RGBAUnorm:
        case MyMTLPixelFormatBC7_RGBAUnorm_sRGB:
            encoderFormat = at_block_format_bc7; break;
         
        // ASTC
        case MyMTLPixelFormatASTC_4x4_sRGB:
        case MyMTLPixelFormatASTC_4x4_LDR:
            encoderFormat = at_block_format_astc_4x4_ldr; break;

        case MyMTLPixelFormatASTC_5x5_sRGB:
        case MyMTLPixelFormatASTC_5x5_LDR:
            encoderFormat = at_block_format_astc_5x5_ldr; break;

        case MyMTLPixelFormatASTC_6x6_sRGB:
        case MyMTLPixelFormatASTC_6x6_LDR:
            encoderFormat = at_block_format_astc_6x6_ldr; break;
            
        case MyMTLPixelFormatASTC_8x8_sRGB:
        case MyMTLPixelFormatASTC_8x8_LDR:
            encoderFormat = at_block_format_astc_8x8_ldr; break;
         
        default:
            assert(false); // unsupported format
         break;
    }

    return encoderFormat;
}

ATEEncoder::ATEEncoder() {
    uint32_t version = at_encoder_get_version();
    (void)version;
    assert(version >= 0x00030001);
}

// from libate and AppleTextureEncoder.h
bool ATEEncoder::Encode(int metalPixelFormat, int dstDataSize, int blockDimsY,
                          bool hasAlpha, bool weightChannels,
                          bool isVerbose, int quality,
                          int width, int height, const uint8_t* srcData, uint8_t* dstData)
{
    // blocks must be 16-byte aligned (what about 8 byte formats?)
    if (((uintptr_t)dstData & 15) != 0) {
        KLOGE("ATEEncoder", "encode dstData must be 16-byte aligned");
        return false;
    }

    at_block_format_t blockFormat = pixelToEncoderFormat((MyMTLPixelFormat)metalPixelFormat);
    if (blockFormat == at_block_format_invalid) {
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
    if (blockFormat == at_block_format_bc1 ||
        blockFormat == at_block_format_bc4 ||
        blockFormat == at_block_format_bc4s ||
        blockFormat == at_block_format_bc5 ||
        blockFormat == at_block_format_bc5s ||
        blockFormat == at_block_format_bc6 ||
        blockFormat == at_block_format_bc6u)
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
        blockFormat,
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

bool ATEEncoder::Decode(int metalPixelFormat, int dstDataSize, int blockDimsY, 
                        /* bool isSrgb, */ bool isVerbose,
                        int width, int height, const uint8_t* srcData, uint8_t* dstData)
{
    // blocks must be aligned to 16 bytes
    if (((uintptr_t)srcData & 15) != 0) {
        KLOGE("ATEEncoder", "decode srcData must be 16-byte aligned");
        return false;
    }

    at_block_format_t blockFormat = pixelToDecoderFormat((MyMTLPixelFormat)metalPixelFormat);
    if (blockFormat == at_block_format_invalid) {
        KLOGE("ATEEncoder", "decode unsupported format");
        return false;
    }
    
    uint32_t flags = at_flags_skip_parameter_checking
               | at_flags_disable_multithreading;
    
// this is a decode only flag that presumably does srgb -> linear conversion, but leave the texels as is
//    if (isSrgb) {
        flags |= at_flags_srgb_linear_texels;
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
    at_alpha_t dstAlphaType = srcAlphaType;
    
    at_encoder_t encoder = at_encoder_create(
        at_texel_format_rgba8_unorm, // TODO: need float buffer for bc6 and astc hdr
        //at_texel_format_bgra8_unorm, see if this decodes properly rgb channels are wrong on astc4x4 decode, alpha okay
        srcAlphaType,
        blockFormat,
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

    if (error != at_error_success) {
        KLOGE("ATEEncoder", "decode failed");
        return false;
    }
    
    return true;
}


} // namespace

#endif

