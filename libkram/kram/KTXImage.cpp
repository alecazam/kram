// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KTXImage.h"

#include <stdio.h>

//#include <algorithm>
//#include <map>
//#include <unordered_map>
#include <mutex>

// for zlib decompress
#include "miniz.h"
// for zstd decompress
#include "zstd.h"

namespace kram {

// These are props added into the KTX file props data.
// Name-value are all ascii for ease of printing and parsing.
// KTX2 has standardized some pairs like KTXvulkanPixelFormat
const char* kPropVulkanFormat = "KramVulkanFormat";
const char* kPropMetalFormat = "KramMetalFormat";
const char* kPropPreswizzle = "KramPreswizzle";
const char* kPropPostswizzle = "KramPostswizzle";
const char* kPropSourceHash = "KramSourceHash";
const char* kPropChannels = "KramChannels";
const char* kPropAddress = "KramAddress";
const char* kPropFilter = "KramFilter";

using namespace NAMESPACE_STL;

// These start each KTX file to indicate the type
const uint8_t kKTXIdentifier[kKTXIdentifierSize] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
    //'«', 'K', 'T', 'X', ' ', '1', '1', '»', '\r', '\n', '\x1A', '\n'
};
const uint8_t kKTX2Identifier[kKTX2IdentifierSize] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
    // '«', 'K', 'T', 'X', ' ', '2', '0', '»', '\r', '\n', '\x1A', '\n'
};

//---------------------------------------------------

// making this up, since Microsoft doesn't publish these constants
const uint32_t DXGI_FORMAT_ETC2_OFFSET = 50;

// enum based on dxgiformat.h
// Copyright (C) Microsoft.  All rights reserved.
// TODO: (renamte to DXGI_FORMAT_FORMAT) so easier to search for all
enum DXFormat : uint32_t
{
    DXGI_FORMAT_UNKNOWN                                 = 0,
    
    //DXGI_FORMAT_R32G32B32A32_TYPELESS                   = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT                      = 2,
    DXGI_FORMAT_R32G32B32A32_UINT                       = 3,
    DXGI_FORMAT_R32G32B32A32_SINT                       = 4,
    //DXGI_FORMAT_R32G32B32_TYPELESS                      = 5,
    DXGI_FORMAT_R32G32B32_FLOAT                         = 6,
    DXGI_FORMAT_R32G32B32_UINT                          = 7,
    DXGI_FORMAT_R32G32B32_SINT                          = 8,
    
    //DXGI_FORMAT_R16G16B16A16_TYPELESS                   = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT                      = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM                      = 11,
    DXGI_FORMAT_R16G16B16A16_UINT                       = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM                      = 13,
    DXGI_FORMAT_R16G16B16A16_SINT                       = 14,
    
    //DXGI_FORMAT_R32G32_TYPELESS                         = 15,
    DXGI_FORMAT_R32G32_FLOAT                            = 16,
    DXGI_FORMAT_R32G32_UINT                             = 17,
    DXGI_FORMAT_R32G32_SINT                             = 18,
    //DXGI_FORMAT_R32G8X24_TYPELESS                       = 19,
    //DXGI_FORMAT_D32_FLOAT_S8X24_UINT                    = 20,
    //DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS                = 21,
    //DXGI_FORMAT_X32_TYPELESS_G8X24_UINT                 = 22,
    
    //DXGI_FORMAT_R10G10B10A2_TYPELESS                    = 23,
    //DXGI_FORMAT_R10G10B10A2_UNORM                       = 24,
    //DXGI_FORMAT_R10G10B10A2_UINT                        = 25,
    //DXGI_FORMAT_R11G11B10_FLOAT                         = 26,
    
    //DXGI_FORMAT_R8G8B8A8_TYPELESS                       = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM                          = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB                     = 29,
    DXGI_FORMAT_R8G8B8A8_UINT                           = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM                          = 31,
    DXGI_FORMAT_R8G8B8A8_SINT                           = 32,
    
    //DXGI_FORMAT_R16G16_TYPELESS                         = 33,
    DXGI_FORMAT_R16G16_FLOAT                            = 34,
    DXGI_FORMAT_R16G16_UNORM                            = 35,
    DXGI_FORMAT_R16G16_UINT                             = 36,
    DXGI_FORMAT_R16G16_SNORM                            = 37,
    DXGI_FORMAT_R16G16_SINT                             = 38,
    
    //DXGI_FORMAT_R32_TYPELESS                            = 39,
    //DXGI_FORMAT_D32_FLOAT                               = 40,
    DXGI_FORMAT_R32_FLOAT                               = 41,
    DXGI_FORMAT_R32_UINT                                = 42,
    DXGI_FORMAT_R32_SINT                                = 43,
    
    //DXGI_FORMAT_R24G8_TYPELESS                          = 44,
    //DXGI_FORMAT_D24_UNORM_S8_UINT                       = 45,
    //DXGI_FORMAT_R24_UNORM_X8_TYPELESS                   = 46,
    //DXGI_FORMAT_X24_TYPELESS_G8_UINT                    = 47,
    //DXGI_FORMAT_R8G8_TYPELESS                           = 48,
    
    DXGI_FORMAT_R8G8_UNORM                              = 49,
    DXGI_FORMAT_R8G8_UINT                               = 50,
    DXGI_FORMAT_R8G8_SNORM                              = 51,
    DXGI_FORMAT_R8G8_SINT                               = 52,
    
    DXGI_FORMAT_R16_FLOAT                               = 54,
    //DXGI_FORMAT_D16_UNORM                               = 55,
    DXGI_FORMAT_R16_UNORM                               = 56,
    DXGI_FORMAT_R16_UINT                                = 57,
    DXGI_FORMAT_R16_SNORM                               = 58,
    DXGI_FORMAT_R16_SINT                                = 59,
    
    DXGI_FORMAT_R8_UNORM                                = 61,
    DXGI_FORMAT_R8_UINT                                 = 62,
    DXGI_FORMAT_R8_SNORM                                = 63,
    DXGI_FORMAT_R8_SINT                                 = 64,
    
    //DXGI_FORMAT_A8_UNORM                                = 65,
    //DXGI_FORMAT_R1_UNORM                                = 66,
    //DXGI_FORMAT_R9G9B9E5_SHAREDEXP                      = 67,
    
    //DXGI_FORMAT_R8G8_B8G8_UNORM                         = 68,
    //DXGI_FORMAT_G8R8_G8B8_UNORM                         = 69,
    
    DXGI_FORMAT_BC1_UNORM                               = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB                          = 72,
    DXGI_FORMAT_BC3_UNORM                               = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB                          = 78,
    DXGI_FORMAT_BC4_UNORM                               = 80,
    DXGI_FORMAT_BC4_SNORM                               = 81,
    DXGI_FORMAT_BC5_UNORM                               = 83,
    DXGI_FORMAT_BC5_SNORM                               = 84,
    DXGI_FORMAT_BC6H_UF16                               = 95,
    DXGI_FORMAT_BC6H_SF16                               = 96,
    DXGI_FORMAT_BC7_UNORM                               = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB                          = 99,
    
    DXGI_FORMAT_B8G8R8A8_UNORM                          = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM                          = 88,
    //DXGI_FORMAT_B8G8R8A8_TYPELESS                       = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB                     = 91,
    //DXGI_FORMAT_B8G8R8X8_TYPELESS                       = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB                     = 93,
    
    // Astc constants are taken from here.
    // HDR constant weren't too hard to guess from gap, but are a guess.
    // Not officially in DX now that Windows Mobile was killed off.
    // https://gli.g-truc.net/0.6.1/api/a00001.html
    DXGI_FORMAT_ASTC_4X4_UNORM = 134,
    DXGI_FORMAT_ASTC_4X4_UNORM_SRGB = 135,
    DXGI_FORMAT_ASTC_4X4_HDR = 136,
    // DXGI_FORMAT_ASTC_5X4_TYPELESS = 137,
    DXGI_FORMAT_ASTC_5X4_UNORM = 138,
    DXGI_FORMAT_ASTC_5X4_UNORM_SRGB = 139,
    DXGI_FORMAT_ASTC_5x4_HDR = 140,
    // DXGI_FORMAT_ASTC_5X5_TYPELESS = 141,
    DXGI_FORMAT_ASTC_5X5_UNORM = 142,
    DXGI_FORMAT_ASTC_5X5_UNORM_SRGB = 143,
    DXGI_FORMAT_ASTC_5X5_HDR = 144,
    // DXGI_FORMAT_ASTC_6X5_TYPELESS = 145,
    DXGI_FORMAT_ASTC_6X5_UNORM = 146,
    DXGI_FORMAT_ASTC_6X5_UNORM_SRGB = 147,
    DXGI_FORMAT_ASTC_6x5_HDR = 148,
    // DXGI_FORMAT_ASTC_6X6_TYPELESS = 149,
    DXGI_FORMAT_ASTC_6X6_UNORM = 150,
    DXGI_FORMAT_ASTC_6X6_UNORM_SRGB = 151,
    DXGI_FORMAT_ASTC_6X6_HDR = 152,
    // DXGI_FORMAT_ASTC_8X5_TYPELESS = 153,
    DXGI_FORMAT_ASTC_8X5_UNORM = 154,
    DXGI_FORMAT_ASTC_8X5_UNORM_SRGB = 155,
    DXGI_FORMAT_ASTC_8X5_HDR = 156,
    // DXGI_FORMAT_ASTC_8X6_TYPELESS = 157,
    DXGI_FORMAT_ASTC_8X6_UNORM = 158,
    DXGI_FORMAT_ASTC_8X6_UNORM_SRGB = 159,
    DXGI_FORMAT_ASTC_8X6_HDR = 160,
    // DXGI_FORMAT_ASTC_8X8_TYPELESS = 161,
    DXGI_FORMAT_ASTC_8X8_UNORM = 162,
    DXGI_FORMAT_ASTC_8X8_UNORM_SRGB = 163,
    DXGI_FORMAT_ASTC_8X8_HDR = 164,
    // DXGI_FORMAT_ASTC_10X5_TYPELESS = 165,
    DXGI_FORMAT_ASTC_10X5_UNORM = 166,
    DXGI_FORMAT_ASTC_10X5_UNORM_SRGB = 167,
    DXGI_FORMAT_ASTC_10X5_HDR = 168,
    // DXGI_FORMAT_ASTC_10X6_TYPELESS = 169,
    DXGI_FORMAT_ASTC_10X6_UNORM = 170,
    DXGI_FORMAT_ASTC_10X6_UNORM_SRGB = 171,
    DXGI_FORMAT_ASTC_10X6_HDR = 172,
    // DXGI_FORMAT_ASTC_10X8_TYPELESS = 173,
    DXGI_FORMAT_ASTC_10X8_UNORM = 174,
    DXGI_FORMAT_ASTC_10X8_UNORM_SRGB = 175,
    DXGI_FORMAT_ASTC_10X8_HDR = 176,
    // DXGI_FORMAT_ASTC_10X10_TYPELESS = 177,
    DXGI_FORMAT_ASTC_10X10_UNORM = 178,
    DXGI_FORMAT_ASTC_10X10_UNORM_SRGB = 179,
    DXGI_FORMAT_ASTC_10X10_HDR = 180,
    // DXGI_FORMAT_ASTC_12X10_TYPELESS = 181,
    DXGI_FORMAT_ASTC_12X10_UNORM = 182,
    DXGI_FORMAT_ASTC_12X10_UNORM_SRGB = 183,
    DXGI_FORMAT_ASTC_12X10_HDR = 181,
    // DXGI_FORMAT_ASTC_12X12_TYPELESS = 185,
    DXGI_FORMAT_ASTC_12X12_UNORM = 186,
    DXGI_FORMAT_ASTC_12X12_UNORM_SRGB = 187,
    DXGI_FORMAT_ASTC_12X12_HDR = 188,
    
    // These are fabricated by kram.  See here for RFI on formats
    // and extensibility on DDS format.  Use at own risk.
    // Set to DXGI_FORMAT_UNKNOWN if don't like this hack.
    // https://github.com/microsoft/DirectXTex/issues/264
    DXGI_FORMAT_EAC_R11_UNORM = 153 + DXGI_FORMAT_ETC2_OFFSET,
    DXGI_FORMAT_EAC_R11_SNORM = 154 + DXGI_FORMAT_ETC2_OFFSET,
    DXGI_FORMAT_EAC_R11G11_UNORM = 155 + DXGI_FORMAT_ETC2_OFFSET,
    DXGI_FORMAT_EAC_R11G11_SNORM = 156 + DXGI_FORMAT_ETC2_OFFSET,
    DXGI_FORMAT_ETC2_R8G8B8_UNORM = 147 + DXGI_FORMAT_ETC2_OFFSET,
    DXGI_FORMAT_ETC2_R8G8B8_UNORM_SRGB = 148 + DXGI_FORMAT_ETC2_OFFSET,
    // DXGI_FORMAT_ETC2_R8G8B8A1_UNORM = 149 + DXGI_FORMAT_ETC2_OFFSET,
    // DXGI_FORMAT_ETC2_R8G8B8A1_UNORM_SRGB = 150 + DXGI_FORMAT_ETC2_OFFSET,
    DXGI_FORMAT_ETC2_R8G8B8A8_UNORM = 151 + DXGI_FORMAT_ETC2_OFFSET,
    DXGI_FORMAT_ETC2_R8G8B8A8_UNORM_SRGB = 152 + DXGI_FORMAT_ETC2_OFFSET,
};

//---------------------------------------------------

enum GLFormat : uint32_t {
    GL_FORMAT_UNKNOWN = 0,

    //#ifndef GL_EXT_texture_compression_s3tc
    // BC1
    GL_COMPRESSED_RGB_S3TC_DXT1_EXT = 0x83F0,
    GL_COMPRESSED_SRGB_S3TC_DXT1_EXT = 0x8C4C,

    // BC1a
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT = 0x83F1,
    GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT = 0x8C4D,

    // BC2
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT = 0x83F2,
    GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT = 0x8C4E,

    // BC3
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT = 0x83F3,
    GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT = 0x8C4F,
    //#endif

    //#ifndef GL_ARB_texture_compression_rgtc
    // bC4
    GL_COMPRESSED_RED_RGTC1 = 0x8DBB,
    GL_COMPRESSED_SIGNED_RED_RGTC1 = 0x8DBC,

    // BC5
    GL_COMPRESSED_RG_RGTC2 = 0x8DBD,
    GL_COMPRESSED_SIGNED_RG_RGTC2 = 0x8DBE,

    //#ifndef GL_ARB_texture_compression_bptc
    // BC6
    GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB = 0x8E8E,
    GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB = 0x8E8F,

    // BC7
    GL_COMPRESSED_RGBA_BPTC_UNORM_ARB = 0x8E8C,
    GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB = 0x8E8D,

    //#if GL_KHR_texture_compression_astc_ldr
    // Note: these hold 16, 20x, 25, 30x, 36, 40x, 48x, 64, ... 50, 60, 80, 100,
    // 120, 144 pixels that's a very large cloud of points to fit to one or 2 lines,
    // and mips bleed too Not supporting non-squre block sizes yet,
    GL_COMPRESSED_RGBA_ASTC_4x4_KHR = 0x93B0,
    GL_COMPRESSED_RGBA_ASTC_5x5_KHR = 0x93B2,
    GL_COMPRESSED_RGBA_ASTC_6x6_KHR = 0x93B4,
    GL_COMPRESSED_RGBA_ASTC_8x8_KHR = 0x93B7,

    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR = 0x93D0,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR = 0x93D2,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR = 0x93D4,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR = 0x93D7,

    //GL_COMPRESSED_RGBA_ASTC_5x4_KHR                         0x93B1
    //GL_COMPRESSED_RGBA_ASTC_6x5_KHR                         0x93B3
    //GL_COMPRESSED_RGBA_ASTC_8x5_KHR                         0x93B5
    //GL_COMPRESSED_RGBA_ASTC_8x6_KHR                         0x93B6
    //GL_COMPRESSED_RGBA_ASTC_10x5_KHR                        0x93B8
    //GL_COMPRESSED_RGBA_ASTC_10x6_KHR                        0x93B9
    //GL_COMPRESSED_RGBA_ASTC_10x8_KHR                        0x93BA
    //GL_COMPRESSED_RGBA_ASTC_10x10_KHR                       0x93BB
    //GL_COMPRESSED_RGBA_ASTC_12x10_KHR                       0x93BC
    //GL_COMPRESSED_RGBA_ASTC_12x12_KHR                       0x93BD

    //GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR                 0x93D1
    //GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR                 0x93D3
    //GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR                 0x93D5
    //GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR                 0x93D6
    //GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR                0x93D8
    //GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR                0x93D9
    //GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR                0x93DA
    //GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR               0x93DB
    //GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR               0x93DC
    //GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR               0x93DD

    // ETC2 constants
    GL_COMPRESSED_R11_EAC = 0x9270,
    GL_COMPRESSED_SIGNED_R11_EAC = 0x9271,
    GL_COMPRESSED_RG11_EAC = 0x9272,
    GL_COMPRESSED_SIGNED_RG11_EAC = 0x9273,

    GL_COMPRESSED_RGB8_ETC2 = 0x9274,
    GL_COMPRESSED_SRGB8_ETC2 = 0x9275,
    // missing gap for unsupported S/RGBA1
    GL_COMPRESSED_RGBA8_ETC2_EAC = 0x9278,
    GL_COMPRESSED_SRGBA8_ETC2_EAC = 0x9279,

    // not supporting these
    //GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2       0x9276
    //GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2      0x9277

    // explicit format
    GL_R8 = 0x8229,
    GL_RG8 = 0x822B,
    GL_RGBA8 = 0x8058,
    GL_SRGB8_ALPHA8 = 0x8C43,
    // TODO: add unorm/snorm types

    GL_R16F = 0x822D,
    GL_RG16F = 0x822F,
    GL_RGBA16F = 0x881A,

    GL_R32F = 0x822E,
    GL_RG32F = 0x8230,
    GL_RGBA32F = 0x8814,

#if SUPPORT_RGB
    GL_RGB8 = 0x8051,
    GL_SRGB8 = 0x8C41,
    GL_RGB16F = 0x881B,
    GL_RGB32F = 0x8815
#endif

    /* These are all of the variants of ASTC, ugh.  Only way to identify them is to
 walk blocks and it's unclear how to convert from 3D to 2D blocks or whether hw
 supports sliced 3D.

 The LDR profile of ASTC is part of the core API for OpenGL ES 3.2. Support for
 earlier API versions and for the other format features is provided via a number
 official Khronos extensions:
 * KHR_texture_compression_astc_ldr: 2D for LDR images
 * KHR_texture_compression_astc_sliced_3d: 2D and sliced 3D for LDR images

 * KHR_texture_compression_astc_hdr: 2D and sliced 3D for LDR and HDR images
     only in iOS13+

 3D blocks and all of the above.  Unclear who implements this:

 * OES_texture_compression_astc: 2D + 3D, LDR + HDR support
 Note: There is a distinction between sliced 3D, where each slice can be
 compressed independently, and 3D formats like 4x4x4.
 OES_texture_compression_astc is the only extension that brings in the 3D
 formats, and is a superset of the KHR extensions.

 */
};

enum GLElementType {
    GL_BYTE = 0x1400,
    GL_UNSIGNED_BYTE = 0x1401,
    GL_SHORT = 0x1402,
    GL_UNSIGNED_SHORT = 0x1403,
    GL_HALF_FLOAT = 0x140B,
    GL_FLOAT = 0x1406,
    //GL_FIXED                          = 0x140C,
};

enum GLFormatBase {
    // base formats
    GL_RED = 0x1903,
    GL_RG = 0x8227,
    GL_RGB = 0x1907,
    GL_RGBA = 0x1908,
    GL_SRGB = 0x8C40,  // only for BC1
    GL_SRGB_ALPHA = 0x8C42,
};

// The encoder only handles sliced 3D astc and not 3D blocks.
// Also no MTL equivalent for 3D blocks.
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#VkPhysicalDeviceTextureCompressionASTCHDRFeaturesEXT
enum VKFormat {
    VK_FORMAT_UNDEFINED = 0,

    // this is explcit format supprt
    VK_FORMAT_R8_UNORM = 9,
    VK_FORMAT_R8G8_UNORM = 16,
    VK_FORMAT_R8G8B8A8_UNORM = 37,
    VK_FORMAT_R8G8B8A8_SRGB = 43,

    VK_FORMAT_R16_SFLOAT = 76,
    VK_FORMAT_R16G16_SFLOAT = 83,
    VK_FORMAT_R16G16B16A16_SFLOAT = 97,

    VK_FORMAT_R32_SFLOAT = 100,
    VK_FORMAT_R32G32_SFLOAT = 103,
    VK_FORMAT_R32G32B32A32_SFLOAT = 109,

    // distinguish HDR from LDR formats
    // Provided by VK_EXT_texture_compression_astc_hdr
    VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT = 1000066000,  // large decimal
    VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT = 1000066001,
    VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT = 1000066002,
    VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT = 1000066003,
    VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT = 1000066004,
    VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT = 1000066005,
    VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT = 1000066006,
    VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT = 1000066007,
    VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT = 1000066008,
    VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT = 1000066009,
    VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT = 1000066010,
    VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT = 1000066011,
    VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT = 1000066012,
    VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT = 1000066013,

    VK_FORMAT_BC1_RGB_UNORM_BLOCK = 131,
    VK_FORMAT_BC1_RGB_SRGB_BLOCK = 132,
    // VK_FORMAT_BC1_RGBA_UNORM_BLOCK = 133,
    // VK_FORMAT_BC1_RGBA_SRGB_BLOCK = 134,
    VK_FORMAT_BC2_UNORM_BLOCK = 135,
    VK_FORMAT_BC2_SRGB_BLOCK = 136,
    VK_FORMAT_BC3_UNORM_BLOCK = 137,
    VK_FORMAT_BC3_SRGB_BLOCK = 138,
    VK_FORMAT_BC4_UNORM_BLOCK = 139,
    VK_FORMAT_BC4_SNORM_BLOCK = 140,
    VK_FORMAT_BC5_UNORM_BLOCK = 141,
    VK_FORMAT_BC5_SNORM_BLOCK = 142,
    VK_FORMAT_BC6H_UFLOAT_BLOCK = 143,
    VK_FORMAT_BC6H_SFLOAT_BLOCK = 144,
    VK_FORMAT_BC7_UNORM_BLOCK = 145,
    VK_FORMAT_BC7_SRGB_BLOCK = 146,

    VK_FORMAT_EAC_R11_UNORM_BLOCK = 153,
    VK_FORMAT_EAC_R11_SNORM_BLOCK = 154,
    VK_FORMAT_EAC_R11G11_UNORM_BLOCK = 155,
    VK_FORMAT_EAC_R11G11_SNORM_BLOCK = 156,
    VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147,
    VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK = 148,
    // VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK = 149,
    // VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK = 150,
    VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK = 151,
    VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK = 152,

    VK_FORMAT_ASTC_4x4_UNORM_BLOCK = 157,
    VK_FORMAT_ASTC_4x4_SRGB_BLOCK = 158,
    VK_FORMAT_ASTC_5x5_UNORM_BLOCK = 161,
    VK_FORMAT_ASTC_5x5_SRGB_BLOCK = 162,
    VK_FORMAT_ASTC_6x6_UNORM_BLOCK = 165,
    VK_FORMAT_ASTC_6x6_SRGB_BLOCK = 166,
    VK_FORMAT_ASTC_8x8_UNORM_BLOCK = 171,
    VK_FORMAT_ASTC_8x8_SRGB_BLOCK = 172,

// not support these
//    VK_FORMAT_ASTC_5x4_UNORM_BLOCK = 159,
//    VK_FORMAT_ASTC_5x4_SRGB_BLOCK = 160,
//    VK_FORMAT_ASTC_6x5_UNORM_BLOCK = 163,
//    VK_FORMAT_ASTC_6x5_SRGB_BLOCK = 164,
//    VK_FORMAT_ASTC_8x5_UNORM_BLOCK = 167,
//    VK_FORMAT_ASTC_8x5_SRGB_BLOCK = 168,
//    VK_FORMAT_ASTC_8x6_UNORM_BLOCK = 169,
//    VK_FORMAT_ASTC_8x6_SRGB_BLOCK = 170,

//    VK_FORMAT_ASTC_10x5_UNORM_BLOCK = 173,
//    VK_FORMAT_ASTC_10x5_SRGB_BLOCK = 174,
//    VK_FORMAT_ASTC_10x6_UNORM_BLOCK = 175,
//    VK_FORMAT_ASTC_10x6_SRGB_BLOCK = 176,
//    VK_FORMAT_ASTC_10x8_UNORM_BLOCK = 177,
//    VK_FORMAT_ASTC_10x8_SRGB_BLOCK = 178,
//    VK_FORMAT_ASTC_10x10_UNORM_BLOCK = 179,
//    VK_FORMAT_ASTC_10x10_SRGB_BLOCK = 180,
//    VK_FORMAT_ASTC_12x10_UNORM_BLOCK = 181,
//    VK_FORMAT_ASTC_12x10_SRGB_BLOCK = 182,
//    VK_FORMAT_ASTC_12x12_UNORM_BLOCK = 183,
//    VK_FORMAT_ASTC_12x12_SRGB_BLOCK = 184,

#if SUPPORT_RGB
    // import only
    VK_FORMAT_R8G8B8_UNORM = 23,
    VK_FORMAT_R8G8B8_SRGB = 29,
    VK_FORMAT_R16G16B16_SFLOAT = 90,
    VK_FORMAT_R32G32B32_SFLOAT = 106,

#endif
};

// DONE: setup a format table, so can switch on it
// FLAG_SRGB, FLAG_HDR, FLAG_SIGNED/UNORM, FLAG_16F, FLAG_32F
// FLAG_AUTOMIP, FLAG_COMPRESSED, ..
// FLAG_ENC_BC, FLAG_ENC_ASTC, FLAG_ENC_ETC, FLAG_ENC_EXPLICIT

//---------------------------------------------------------

enum KTXFormatFlag {
    FLAG_SRGB = (1 << 0),
    FLAG_SIGNED = (1 << 2),
    FLAG_16F = (1 << 3),
    FLAG_32F = (1 << 4),

    // compressed block forats
    FLAG_ENC_BC = (1 << 5),
    FLAG_ENC_ASTC = (1 << 6),
    FLAG_ENC_ETC = (1 << 7),
};
using KTXFormatFlags = uint32_t;

class KTXFormatInfo {
public:
    KTXFormatInfo() {}

    KTXFormatInfo(
        const char* formatName_,
        const char* metalName_,
        const char* vulkanName_,
        const char* directxName_,
        const char* glName_,

        MyMTLPixelFormat metalType_,
        VKFormat vulkanType_,
        DXFormat directxType_,
        GLFormat glType_,
        GLFormatBase glBase_,

        uint8_t blockDimsX_,
        uint8_t blockDimsY_,

        uint8_t blockSize_,
        uint8_t numChannels_,

        KTXFormatFlags flags_)
    {
        formatName = formatName_;
        metalName = metalName_;
        vulkanName = vulkanName_;
        directxName = directxName_;
        glName = glName_;

        metalType = metalType_;
        vulkanType = vulkanType_;
        directxType = directxType_;
        glType = glType_;
        glBase = glBase_;

        blockSize = blockSize_;
        numChannels = numChannels_;

        blockDimsX = blockDimsX_;
        blockDimsY = blockDimsY_;

        flags = flags_;
    }

    const char* formatName;
    const char* metalName;
    const char* vulkanName;
    const char* directxName;
    const char* glName;

    uint16_t metalType;
    uint16_t vulkanType;
    uint16_t directxType;
    uint16_t glType;
    uint16_t glBase;

    uint8_t blockDimsX;
    uint8_t blockDimsY;

    uint8_t blockSize;
    uint8_t numChannels;

    uint16_t flags;

    Int2 blockDims() const
    {
        Int2 dims = {blockDimsX, blockDimsY};
        return dims;
    }

    // srgb is reserved for rgb/a 8-bit textures
    bool isSRGB() const { return flags & FLAG_SRGB; }
    bool isSigned() const { return flags & FLAG_SIGNED; }

    // these typically store linear color
    bool isHDR() const { return flags & (FLAG_16F | FLAG_32F); }
    bool is16F() const { return flags & FLAG_16F; }
    bool is32F() const { return flags & FLAG_32F; }

    bool isBC() const { return flags & FLAG_ENC_BC; }
    bool isASTC() const { return flags & FLAG_ENC_ASTC; }
    bool isETC() const { return flags & FLAG_ENC_ETC; }
    bool isCompressed() const { return flags & (FLAG_ENC_ETC | FLAG_ENC_ASTC | FLAG_ENC_BC); }

    bool isColor() const { return numChannels >= 3; }
    bool isAlpha() const { return numChannels >= 4; }
};

//---------------------------------------------------------

//using tFormatTable = unordered_map<MyMTLPixelFormat, KTXFormatInfo>;

using mymap = unordered_map<uint32_t /*MyMTLPixelFormat*/, KTXFormatInfo>;
static mymap* gFormatTable = nullptr;

// thumbnailing can hit this from multiple threads
using mymutex = std::recursive_mutex;
using mylock = std::unique_lock<mymutex>;
static mymutex gFormatTableMutex;

static bool initFormatsIfNeeded()
{
    if (gFormatTable) {
        return true;
    }
    
    mylock lock(gFormatTableMutex);
    
    if (gFormatTable) {
        return true;
    }
    
    mymap* formatTable = new unordered_map<uint32_t /*MyMTLPixelFormat*/, KTXFormatInfo>();

// the following table could be included multiple ties to build switch statements, but instead use a hashmap
#define KTX_FORMAT(fmt, metalType, vulkanType, directxType, glType, glBase, x, y, blockSize, numChannels, flags) \
    (*formatTable)[(uint32_t)metalType] = KTXFormatInfo(             \
        #fmt, #metalType, #vulkanType, #directxType, #glType,        \
        metalType, vulkanType, directxType, glType, glBase,          \
        x, y, blockSize, numChannels, (flags));

    KTX_FORMAT(Invalid, MyMTLPixelFormatInvalid, VK_FORMAT_UNDEFINED, DXGI_FORMAT_UNKNOWN, GL_FORMAT_UNKNOWN, GL_RGBA, 1, 1, 0, 0, 0)

    // BC
    KTX_FORMAT(BC1, MyMTLPixelFormatBC1_RGBA, VK_FORMAT_BC1_RGB_UNORM_BLOCK, DXGI_FORMAT_BC1_UNORM, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_RGB, 4, 4, 8, 3, FLAG_ENC_BC)
    KTX_FORMAT(BC1s, MyMTLPixelFormatBC1_RGBA_sRGB, VK_FORMAT_BC1_RGB_SRGB_BLOCK, DXGI_FORMAT_BC1_UNORM_SRGB,GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, GL_SRGB, 4, 4, 8, 3, FLAG_ENC_BC | FLAG_SRGB)

    KTX_FORMAT(BC3, MyMTLPixelFormatBC3_RGBA, VK_FORMAT_BC3_UNORM_BLOCK, DXGI_FORMAT_BC3_UNORM, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_RGBA, 4, 4, 16, 4, FLAG_ENC_BC)
    KTX_FORMAT(BC3s, MyMTLPixelFormatBC3_RGBA_sRGB, VK_FORMAT_BC3_SRGB_BLOCK, DXGI_FORMAT_BC3_UNORM_SRGB, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, GL_SRGB_ALPHA, 4, 4, 16, 4, FLAG_ENC_BC | FLAG_SRGB)

    KTX_FORMAT(BC4, MyMTLPixelFormatBC4_RUnorm, VK_FORMAT_BC4_UNORM_BLOCK, DXGI_FORMAT_BC4_UNORM, GL_COMPRESSED_RED_RGTC1, GL_RED, 4, 4, 8, 1, FLAG_ENC_BC)
    KTX_FORMAT(BC4sn, MyMTLPixelFormatBC4_RSnorm, VK_FORMAT_BC4_SNORM_BLOCK, DXGI_FORMAT_BC4_SNORM, GL_COMPRESSED_SIGNED_RED_RGTC1, GL_RED, 4, 4, 8, 1, FLAG_ENC_BC | FLAG_SIGNED)

    KTX_FORMAT(BC5, MyMTLPixelFormatBC5_RGUnorm, VK_FORMAT_BC5_UNORM_BLOCK, DXGI_FORMAT_BC5_UNORM, GL_COMPRESSED_RG_RGTC2, GL_RG, 4, 4, 16, 2, FLAG_ENC_BC)
    KTX_FORMAT(BC5sn, MyMTLPixelFormatBC5_RGSnorm, VK_FORMAT_BC5_SNORM_BLOCK, DXGI_FORMAT_BC5_SNORM, GL_COMPRESSED_SIGNED_RG_RGTC2, GL_RG, 4, 4, 16, 2, FLAG_ENC_BC | FLAG_SIGNED)

    KTX_FORMAT(BC6h, MyMTLPixelFormatBC6H_RGBFloat, VK_FORMAT_BC6H_SFLOAT_BLOCK, DXGI_FORMAT_BC6H_SF16, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB, GL_RGB, 4, 4, 16, 4, FLAG_ENC_BC | FLAG_16F)
    KTX_FORMAT(BC6uh, MyMTLPixelFormatBC6H_RGBUfloat, VK_FORMAT_BC6H_SFLOAT_BLOCK, DXGI_FORMAT_BC6H_UF16, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB, GL_RGB, 4, 4, 16, 4, FLAG_ENC_BC | FLAG_16F)

    KTX_FORMAT(BC7, MyMTLPixelFormatBC7_RGBAUnorm, VK_FORMAT_BC7_UNORM_BLOCK, DXGI_FORMAT_BC7_UNORM, GL_COMPRESSED_RGBA_BPTC_UNORM_ARB, GL_RGBA, 4, 4, 16, 4, FLAG_ENC_BC)
    KTX_FORMAT(BC7s, MyMTLPixelFormatBC7_RGBAUnorm_sRGB, VK_FORMAT_BC7_SRGB_BLOCK, DXGI_FORMAT_BC7_UNORM_SRGB, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB, GL_SRGB_ALPHA, 4, 4, 16, 4, FLAG_ENC_BC | FLAG_SRGB)

    // ETC
    KTX_FORMAT(ETCr, MyMTLPixelFormatEAC_R11Unorm, VK_FORMAT_EAC_R11_UNORM_BLOCK, DXGI_FORMAT_EAC_R11_UNORM, GL_COMPRESSED_R11_EAC, GL_RED, 4, 4, 8, 1, FLAG_ENC_ETC)
    KTX_FORMAT(ETCrsn, MyMTLPixelFormatEAC_R11Snorm, VK_FORMAT_EAC_R11_SNORM_BLOCK, DXGI_FORMAT_EAC_R11_SNORM, GL_COMPRESSED_SIGNED_R11_EAC, GL_RED, 4, 4, 8, 1, FLAG_ENC_ETC | FLAG_SIGNED)

    KTX_FORMAT(ETCrg, MyMTLPixelFormatEAC_RG11Unorm, VK_FORMAT_EAC_R11G11_UNORM_BLOCK, DXGI_FORMAT_EAC_R11G11_UNORM, GL_COMPRESSED_RG11_EAC, GL_RG, 4, 4, 16, 2, FLAG_ENC_ETC)
    KTX_FORMAT(ETCrgsn, MyMTLPixelFormatEAC_RG11Snorm, VK_FORMAT_EAC_R11G11_SNORM_BLOCK, DXGI_FORMAT_EAC_R11G11_SNORM, GL_COMPRESSED_SIGNED_RG11_EAC, GL_RG, 4, 4, 16, 2, FLAG_ENC_ETC | FLAG_SIGNED)

    KTX_FORMAT(ETCrgb, MyMTLPixelFormatETC2_RGB8, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, DXGI_FORMAT_ETC2_R8G8B8_UNORM, GL_COMPRESSED_RGB8_ETC2, GL_RGB, 4, 4, 8, 3, FLAG_ENC_ETC)
    KTX_FORMAT(ETCsrgb, MyMTLPixelFormatETC2_RGB8_sRGB, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, DXGI_FORMAT_ETC2_R8G8B8_UNORM_SRGB, GL_COMPRESSED_SRGB8_ETC2, GL_SRGB, 4, 4, 8, 3, FLAG_ENC_ETC | FLAG_SRGB)

    KTX_FORMAT(ETCrgba, MyMTLPixelFormatEAC_RGBA8, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, DXGI_FORMAT_ETC2_R8G8B8A8_UNORM, GL_COMPRESSED_RGBA8_ETC2_EAC, GL_RGBA, 4, 4, 16, 4, FLAG_ENC_ETC)
    KTX_FORMAT(ETCsrgba, MyMTLPixelFormatEAC_RGBA8_sRGB, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, DXGI_FORMAT_ETC2_R8G8B8A8_UNORM_SRGB, GL_COMPRESSED_SRGBA8_ETC2_EAC, GL_SRGB_ALPHA, 4, 4, 16, 4, FLAG_ENC_ETC | FLAG_SRGB)

    // ASTC
    KTX_FORMAT(ASTC4x4, MyMTLPixelFormatASTC_4x4_LDR, VK_FORMAT_ASTC_4x4_UNORM_BLOCK, DXGI_FORMAT_ASTC_4X4_UNORM, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, GL_RGBA, 4, 4, 16, 4, FLAG_ENC_ASTC)
    KTX_FORMAT(ASTC4x4s, MyMTLPixelFormatASTC_4x4_sRGB, VK_FORMAT_ASTC_4x4_SRGB_BLOCK, DXGI_FORMAT_ASTC_4X4_UNORM_SRGB, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, GL_SRGB_ALPHA, 4, 4, 16, 4, FLAG_ENC_ASTC | FLAG_SRGB)
    KTX_FORMAT(ASTC4x4h, MyMTLPixelFormatASTC_4x4_HDR, VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT, DXGI_FORMAT_UNKNOWN, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, GL_RGBA, 4, 4, 16, 4, FLAG_ENC_ASTC | FLAG_16F)  // gl type same as LDR

    KTX_FORMAT(ASTC5x5, MyMTLPixelFormatASTC_5x5_LDR, VK_FORMAT_ASTC_5x5_UNORM_BLOCK, DXGI_FORMAT_ASTC_5X5_UNORM, GL_COMPRESSED_RGBA_ASTC_5x5_KHR, GL_RGBA, 5, 5, 16, 4, FLAG_ENC_ASTC)
    KTX_FORMAT(ASTC5x5s, MyMTLPixelFormatASTC_5x5_sRGB, VK_FORMAT_ASTC_5x5_SRGB_BLOCK, DXGI_FORMAT_ASTC_5X5_UNORM_SRGB, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR, GL_SRGB_ALPHA, 5, 5, 16, 4, FLAG_ENC_ASTC | FLAG_SRGB)
    KTX_FORMAT(ASTC5x5h, MyMTLPixelFormatASTC_5x5_HDR, VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT, DXGI_FORMAT_ASTC_5X5_HDR, GL_COMPRESSED_RGBA_ASTC_5x5_KHR, GL_RGBA, 5, 5, 16, 4, FLAG_ENC_ASTC | FLAG_16F)  // gl type same as LDR

    KTX_FORMAT(ASTC6x6, MyMTLPixelFormatASTC_6x6_LDR, VK_FORMAT_ASTC_6x6_UNORM_BLOCK, DXGI_FORMAT_ASTC_6X6_UNORM, GL_COMPRESSED_RGBA_ASTC_6x6_KHR, GL_RGBA, 6, 6, 16, 4, FLAG_ENC_ASTC)
    KTX_FORMAT(ASTC6x6s, MyMTLPixelFormatASTC_6x6_sRGB, VK_FORMAT_ASTC_6x6_SRGB_BLOCK, DXGI_FORMAT_ASTC_6X6_UNORM_SRGB, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR, GL_SRGB_ALPHA, 6, 6, 16, 4, FLAG_ENC_ASTC | FLAG_SRGB)
    KTX_FORMAT(ASTC6x6h, MyMTLPixelFormatASTC_6x6_HDR, VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT, DXGI_FORMAT_ASTC_6X6_HDR, GL_COMPRESSED_RGBA_ASTC_6x6_KHR, GL_RGBA, 6, 6, 16, 4, FLAG_ENC_ASTC | FLAG_16F)  // gl type same as LDR

    KTX_FORMAT(ASTC8x8, MyMTLPixelFormatASTC_8x8_LDR, VK_FORMAT_ASTC_8x8_UNORM_BLOCK, DXGI_FORMAT_ASTC_8X8_UNORM, GL_COMPRESSED_RGBA_ASTC_8x8_KHR, GL_RGBA, 8, 8, 16, 4, FLAG_ENC_ASTC)
    KTX_FORMAT(ASTC8x8s, MyMTLPixelFormatASTC_8x8_sRGB, VK_FORMAT_ASTC_8x8_SRGB_BLOCK, DXGI_FORMAT_ASTC_8X8_UNORM_SRGB, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR, GL_SRGB_ALPHA, 8, 8, 16, 4, FLAG_ENC_ASTC | FLAG_SRGB)
    KTX_FORMAT(ASTC8x8h, MyMTLPixelFormatASTC_8x8_HDR, VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT, DXGI_FORMAT_ASTC_8X8_HDR, GL_COMPRESSED_RGBA_ASTC_8x8_KHR, GL_RGBA, 8, 8, 16, 4, FLAG_ENC_ASTC | FLAG_16F)  // gl type same as LDR

    // Explicit
    KTX_FORMAT(EXPr8, MyMTLPixelFormatR8Unorm, VK_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM, GL_R8, GL_RED, 1, 1, 1, 1, 0)
    KTX_FORMAT(EXPrg8, MyMTLPixelFormatRG8Unorm,  VK_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_UNORM,GL_RG8, GL_RG, 1, 1, 2, 2, 0)
    KTX_FORMAT(EXPrgba8, MyMTLPixelFormatRGBA8Unorm, VK_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, GL_RGBA8, GL_RGBA, 1, 1, 4, 4, 0)
    KTX_FORMAT(EXPsrgba8, MyMTLPixelFormatRGBA8Unorm_sRGB, VK_FORMAT_R8G8B8A8_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, GL_SRGB8_ALPHA8, GL_SRGB_ALPHA, 1, 1, 4, 4, FLAG_SRGB)

    KTX_FORMAT(EXPr16f, MyMTLPixelFormatR16Float, VK_FORMAT_R16_SFLOAT, DXGI_FORMAT_R16_FLOAT, GL_R16F, GL_RED, 1, 1, 2, 1, FLAG_16F)
    KTX_FORMAT(EXPrg16f, MyMTLPixelFormatRG16Float, VK_FORMAT_R16G16_SFLOAT, DXGI_FORMAT_R16G16_FLOAT, GL_RG16F, GL_RG, 1, 1, 4, 2, FLAG_16F)
    KTX_FORMAT(EXPrgba16f, MyMTLPixelFormatRGBA16Float, VK_FORMAT_R16G16B16A16_SFLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, GL_RGBA16F, GL_RGBA, 1, 1, 8, 4, FLAG_16F)

    KTX_FORMAT(EXPr32f, MyMTLPixelFormatR32Float, VK_FORMAT_R32_SFLOAT, DXGI_FORMAT_R32_FLOAT, GL_R32F, GL_RED, 1, 1, 4, 1, FLAG_32F)
    KTX_FORMAT(EXPrg32f, MyMTLPixelFormatRG32Float, VK_FORMAT_R32G32_SFLOAT, DXGI_FORMAT_R32G32_FLOAT, GL_RG32F, GL_RG, 1, 1, 8, 2, FLAG_32F)
    KTX_FORMAT(EXPrgba32f, MyMTLPixelFormatRGBA32Float, VK_FORMAT_R32G32B32A32_SFLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, GL_RGBA32F, GL_RGBA, 1, 1, 16, 4, FLAG_32F)

#if SUPPORT_RGB
    // these are import only formats
    // DX only has one of these as a valid type
    KTX_FORMAT(EXPrgb8, MyMTLPixelFormatRGB8Unorm_internal, VK_FORMAT_R8G8B8_UNORM, DXGI_FORMAT_UNKNOWN, GL_RGB8, GL_RGB, 1, 1, 3, 3, 0)
    KTX_FORMAT(EXPsrgb8, MyMTLPixelFormatRGB8Unorm_sRGB_internal, VK_FORMAT_R8G8B8_SRGB, DXGI_FORMAT_UNKNOWN, GL_SRGB8, GL_SRGB, 1, 1, 3, 3, FLAG_SRGB)
    KTX_FORMAT(EXPrgb16f, MyMTLPixelFormatRGB16Float_internal, VK_FORMAT_R16G16B16_SFLOAT, DXGI_FORMAT_UNKNOWN, GL_RGB16F, GL_RGB, 1, 1, 6, 3, FLAG_16F)
    KTX_FORMAT(EXPrgb32f, MyMTLPixelFormatRGB32Float_internal, VK_FORMAT_R32G32B32_SFLOAT, DXGI_FORMAT_R32G32B32_FLOAT, GL_RGB32F, GL_RGB, 1, 1, 12, 3, FLAG_32F)
#endif

    gFormatTable = formatTable;
    
    return true;
}

//static bool gPreInitFormats = initFormats();

//---------------------------------------------------------

const KTXFormatInfo& formatInfo(MyMTLPixelFormat format)
{
    initFormatsIfNeeded();

    const auto& it = gFormatTable->find(format);
    if (it == gFormatTable->end()) {
        return gFormatTable->find(MyMTLPixelFormatInvalid)->second;
    }
    return it->second;
}

bool isFloatFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.is16F() || it.is32F();
}

bool isHalfFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.is16F();
}

bool isBCFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.isBC();
}

bool isETCFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.isETC();
}

bool isASTCFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.isASTC();
}

bool isBlockFormat(MyMTLPixelFormat format)
{
    return isBCFormat(format) || isETCFormat(format) || isASTCFormat(format);
}

bool isExplicitFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return !(it.isASTC() || it.isETC() || it.isBC());
}

bool isHdrFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.isHDR();
}

bool isSrgbFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.isSRGB();
}

bool isColorFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.isColor();
}

bool isAlphaFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.isAlpha();
}

bool isSignedFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.isSigned();
}

Int2 blockDimsOfFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.blockDims();
}

uint32_t blockSizeOfFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.blockSize;
}

uint32_t numChannelsOfFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.numChannels;
}

uint32_t metalType(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.metalType;
}

const char* metalTypeName(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.metalName + 2;  // strlen("My");
}

const char* formatTypeName(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.formatName;
}

const char* directxTypeName(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.directxName;
}

uint32_t directxType(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.directxType;
}


const char* vulkanTypeName(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.vulkanName;
}

uint32_t vulkanType(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.vulkanType;
}

const char* glTypeName(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.glName;
}

uint32_t glType(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.glType;
}

MyMTLPixelFormat glToMetalFormat(uint32_t format)
{
    if (format == 0) {
        return MyMTLPixelFormatInvalid;
    }
    
    initFormatsIfNeeded();

    for (const auto& it : *gFormatTable) {
        // this isn't 1:1, since ASTC_HDR aren't unique types, prefer ldr type
        const KTXFormatInfo& info = it.second;
        if (info.glType == format && !(info.isASTC() && info.isHDR())) {
            return (MyMTLPixelFormat)info.metalType;
        }
    }

    return MyMTLPixelFormatInvalid;
}

MyMTLPixelFormat vulkanToMetalFormat(uint32_t format)
{
    if (format == 0) {
        return MyMTLPixelFormatInvalid;
    }
    
    initFormatsIfNeeded();

    for (const auto& it : *gFormatTable) {
        // this isn't 1:1, since ASTC_HDR aren't unique types, prefer ldr type
        const KTXFormatInfo& info = it.second;
        if (info.vulkanType == format) {
            return (MyMTLPixelFormat)info.metalType;
        }
    }

    return MyMTLPixelFormatInvalid;
}

MyMTLPixelFormat directxToMetalFormat(uint32_t format)
{
    if (format == 0) {
        return MyMTLPixelFormatInvalid;
    }
    
    initFormatsIfNeeded();

    for (const auto& it : *gFormatTable) {
        // many formats are missing (Astc/Etc and rgb formats), so only use for limited load/save
        const KTXFormatInfo& info = it.second;
        if (info.directxType == format) {
            return (MyMTLPixelFormat)info.metalType;
        }
    }

    return MyMTLPixelFormatInvalid;
}

MyMTLPixelFormat toggleSrgbFormat(MyMTLPixelFormat format)
{
    switch (format) {
        // bc
        case MyMTLPixelFormatBC1_RGBA:
            return MyMTLPixelFormatBC1_RGBA_sRGB;
        case MyMTLPixelFormatBC1_RGBA_sRGB:
            return MyMTLPixelFormatBC1_RGBA;

        case MyMTLPixelFormatBC3_RGBA:
            return MyMTLPixelFormatBC3_RGBA_sRGB;
        case MyMTLPixelFormatBC3_RGBA_sRGB:
            return MyMTLPixelFormatBC3_RGBA;

        case MyMTLPixelFormatBC7_RGBAUnorm:
            return MyMTLPixelFormatBC7_RGBAUnorm_sRGB;
        case MyMTLPixelFormatBC7_RGBAUnorm_sRGB:
            return MyMTLPixelFormatBC7_RGBAUnorm;

        // astc
        case MyMTLPixelFormatASTC_4x4_sRGB:
            return MyMTLPixelFormatASTC_4x4_LDR;
        case MyMTLPixelFormatASTC_4x4_LDR:
            return MyMTLPixelFormatASTC_4x4_sRGB;

        case MyMTLPixelFormatASTC_5x5_sRGB:
            return MyMTLPixelFormatASTC_5x5_LDR;
        case MyMTLPixelFormatASTC_5x5_LDR:
            return MyMTLPixelFormatASTC_5x5_sRGB;

        case MyMTLPixelFormatASTC_6x6_sRGB:
            return MyMTLPixelFormatASTC_6x6_LDR;
        case MyMTLPixelFormatASTC_6x6_LDR:
            return MyMTLPixelFormatASTC_6x6_sRGB;

        case MyMTLPixelFormatASTC_8x8_sRGB:
            return MyMTLPixelFormatASTC_8x8_LDR;
        case MyMTLPixelFormatASTC_8x8_LDR:
            return MyMTLPixelFormatASTC_8x8_sRGB;

        // etc
        case MyMTLPixelFormatETC2_RGB8:
            return MyMTLPixelFormatETC2_RGB8_sRGB;
        case MyMTLPixelFormatETC2_RGB8_sRGB:
            return MyMTLPixelFormatETC2_RGB8;
        case MyMTLPixelFormatEAC_RGBA8:
            return MyMTLPixelFormatEAC_RGBA8_sRGB;
        case MyMTLPixelFormatEAC_RGBA8_sRGB:
            return MyMTLPixelFormatEAC_RGBA8;

        // explicit
        case MyMTLPixelFormatRGBA8Unorm:
            return MyMTLPixelFormatRGBA8Unorm_sRGB;
        case MyMTLPixelFormatRGBA8Unorm_sRGB:
            return MyMTLPixelFormatRGBA8Unorm;

        default:
            break;
    }

    return format;
}

const char* supercompressionName(KTX2Supercompression type)
{
    const char* name = "Unknown";
    switch (type) {
        case KTX2SupercompressionNone:
            name = "None";
            break;
        case KTX2SupercompressionBasisLZ:
            name = "BasisLZ";
            break;
        case KTX2SupercompressionZstd:
            name = "Zstd";
            break;
        case KTX2SupercompressionZlib:
            name = "Zlib";
            break;
    }
    return name;
}

// https://docs.unity3d.com/ScriptReference/Experimental.Rendering.GraphicsFormat.html
// Unity only handles 4,5,6,8,10,12 square block dimensions

uint32_t KTXImage::mipLengthCalc(uint32_t width_, uint32_t height_) const
{
    // TODO: ktx has 4 byte row alignment, fix that in calcs and code
    // data isn't fully packed on explicit formats like r8, rg8, r16f.
    // That affects iterating through pixel count.

    uint32_t count = blockCount(width_, height_);
    uint32_t size = blockSize();
    return count * size;
}

uint32_t KTXImage::mipLengthCalc(uint32_t mipNumber) const
{
    uint32_t w = width;
    uint32_t h = height;
    uint32_t d = depth;

    mipDown(w, h, d, mipNumber);
    return mipLengthCalc(w, h);
}

uint32_t KTXImage::blockCountRows(uint32_t width_) const
{
    assert(width_ >= 1);

    Int2 dims = blockDims();

    width_ = (width_ + dims.x - 1) / dims.x;

    return width_;
}

uint32_t KTXImage::blockCount(uint32_t width_, uint32_t height_) const
{
    assert(width_ >= 1 && height_ >= 1);

    Int2 dims = blockDims();

    width_ = (width_ + dims.x - 1) / dims.x;
    height_ = (height_ + dims.y - 1) / dims.y;

    uint32_t count = width_ * height_;
    return count;
}

Int2 KTXImage::blockDims() const
{
    return kram::blockDimsOfFormat(pixelFormat);
}

uint32_t KTXImage::blockSize() const
{
    return kram::blockSizeOfFormat(pixelFormat);
}

MyMTLPixelFormat KTXHeader::metalFormat() const
{
    return glToMetalFormat(glInternalFormat);
}

void KTXHeader::initFormatGL(MyMTLPixelFormat pixelFormat)
{
    const auto& info = formatInfo(pixelFormat);

    glInternalFormat = info.glType;
    glBaseInternalFormat = info.glBase;

    // https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/
    // https://www.khronos.org/registry/OpenGL/specs/gl/glspec44.core.pdf#nameddest=table-8.2
    // ugh, like 5 fields to define a texture format in GL
    // Preview fails to open a basic S/RGBA8 KTX file with mips, saying the file is damaged even though
    // PVRTexToolGUI can open it.

    if (info.isCompressed()) {
        glType = 0;
        glTypeSize = 1;

        // glFormat = 0 for compressed textures
        glFormat = 0;
    }
    else {
        if (info.is16F()) {
            glType = GL_HALF_FLOAT;
            glTypeSize = 2;
        }
        else if (info.is32F()) {
            glType = GL_FLOAT;
            glTypeSize = 4;
        }
        else {
            glType = GL_UNSIGNED_BYTE;
            glTypeSize = 1;
        }
        // TODO: does typeSize need to be multiplied by numChannels?

        // glFormat same as base internal format for uncompressed texture
        glFormat = info.glBase;
    }
}

uint32_t KTXHeader::totalChunks() const
{
    return std::max(1u, numberOfArrayElements) *
           std::max(1u, numberOfFaces) *
           std::max(1u, pixelDepth);
}

uint32_t KTXImage::totalChunks() const
{
    return header.totalChunks();
}

MyMTLTextureType KTXHeader::metalTextureType() const
{
    MyMTLTextureType textureType = MyMTLTextureType2D;

    // these are the only types that can be loaded from ktx
    // textureBuffer is not one of the types.
    if (numberOfArrayElements > 0) {
        if (numberOfFaces > 1) {
            textureType = MyMTLTextureTypeCubeArray;
        }
        else if (pixelHeight > 0) {
            textureType = MyMTLTextureType2DArray;
        }
        else {
            textureType = MyMTLTextureType1DArray;
        }
    }
    else {
        if (numberOfFaces > 1) {
            textureType = MyMTLTextureTypeCube;
        }
        else if (pixelDepth > 0) {
            textureType = MyMTLTextureType3D;
        }
        else if (pixelHeight > 0) {
            textureType = MyMTLTextureType2D;
        }
        // TODO: do we even expose these ?
        //        else {
        //           textureType = MyMTLTextureType1D;
        //        }
    }

    return textureType;
}

//---------------------------------------------------

inline bool isKTX2File(const uint8_t* data, size_t dataSize) {
    if (dataSize < sizeof(kKTX2Identifier)) {
        return false;
    }
    
    if (memcmp(data, kKTX2Identifier, sizeof(kKTX2Identifier)) != 0) {
        return false;
    }
    return true;
}

inline bool isKTX1File(const uint8_t* data, size_t dataSize) {
    if (dataSize < sizeof(kKTXIdentifier)) {
        return false;
    }
    
    if (memcmp(data, kKTXIdentifier, sizeof(kKTXIdentifier)) != 0) {
        return false;
    }
    return true;
}

bool KTXImage::open(const uint8_t* imageData, size_t imageDataLength, bool isInfoOnly)
{
    // Note: never trust the extension, always load based on the identifier
    if (isKTX2File(imageData, imageDataLength)) {
        return openKTX2(imageData, imageDataLength, isInfoOnly);
    }

    // check for ktx1
    if (!isKTX1File(imageData, imageDataLength)) {
        return false;
    }

    //skipImageLength = skipImageLength_;

    // since KTX1 doesn't have compressed mips, can alias the file data directly
    fileData = imageData;
    fileDataLength = imageDataLength;

    // copy out the header, TODO: should make sure bytes exist
    header = *(const KTXHeader*)fileData;

    width = header.pixelWidth;
    height = max(1u, header.pixelHeight);
    depth = max(1u, header.pixelDepth);

    textureType = header.metalTextureType();

    // convert keyValues to props vector
    initProps(fileData + sizeof(KTXHeader), header.bytesOfKeyValueData);

    // find prop for KTXmetalFormat, and use that
    // only if not found, then fallback on gl format conversion from KTXHeader
    // astc hdr doesn't have unique gl types.
    const string& metalFormat = getProp(kPropMetalFormat);
    if (!metalFormat.empty()) {
        if (sscanf(metalFormat.c_str(), "%d", &pixelFormat) != 1) {
            KLOGE("kram", "pixelFormat not parsed from prop");
            return false;
        }

        // TODO: validate that format is one supported
    }
    else {
        pixelFormat = header.metalFormat();
    }

    if (pixelFormat == MyMTLPixelFormatInvalid) {
        KLOGE("kram", "unsupported texture format glBase/glInternalFormat 0x%04X 0x%04X", header.glBaseInternalFormat, header.glInternalFormat);
        return false;
    }

    initMipLevels(sizeof(KTXHeader) + header.bytesOfKeyValueData);
    return validateMipLevels();
}

void KTXImage::initProps(const uint8_t* propsData, size_t propDataSize)
{
    props.clear();

    if (propDataSize > 0) {
        const uint8_t* keyValuesStart =
            (const uint8_t*)propsData;
        const uint8_t* keyValuesEnd =
            (const uint8_t*)propsData + propDataSize;

        while (keyValuesStart < keyValuesEnd) {
            size_t dataSize = *(const uint32_t*)keyValuesStart;
            keyValuesStart += sizeof(uint32_t);

            // split key off at null char
            const uint8_t* keyStart = keyValuesStart;
            const uint8_t* valueStart = NULL;
            for (int32_t i = 0; i < (int)dataSize; ++i) {
                if (keyValuesStart[i] == '\0') {
                    valueStart = &keyValuesStart[i + 1];
                    break;
                }
            }

            //LOGD("KTXImage", "KTXProp '%s': %s\n", keyStart, valueStart);

            props.push_back(make_pair(string((const char*)keyStart), string((const char*)valueStart)));

            // pad to 4 byte alignment
            int32_t valuePadding = 3 - ((dataSize + 3) % 4);
            keyValuesStart += dataSize + valuePadding;
        }
    }
}
void KTXImage::addProp(const char* name, const char* value)
{
    // simple linear search, not a map to keep orderings
    for (int i = 0; i < (int)props.size(); ++i) {
        if (props[i].first == name) {
            props[i].second = value;
            return;
        }
    }
    props.push_back(make_pair(string(name), string(value)));
}

string KTXImage::getProp(const char* name) const
{
    // simple linear search, not a map to keep orderings
    for (int i = 0; i < (int)props.size(); ++i) {
        if (props[i].first == name) {
            return props[i].second;
        }
    }
    return "";
}

void KTXImage::addFormatProps()
{
    string propValue;

    // write these as hex strings instead of 4-byte ints
    sprintf(propValue, "%d", vulkanType(pixelFormat));
    addProp(kPropVulkanFormat, propValue.c_str());

    sprintf(propValue, "%d", metalType(pixelFormat));
    addProp(kPropMetalFormat, propValue.c_str());
}

void KTXImage::addSwizzleProps(const char* swizzleTextPre, const char* swizzleTexPost)
{
    // add the swizzles
    if (swizzleTextPre && swizzleTextPre[0] != 0)
        addProp(kPropPreswizzle, swizzleTextPre);
    if (swizzleTexPost && swizzleTexPost[0] != 0)
        addProp(kPropPostswizzle, swizzleTexPost);
}

void KTXImage::addSourceHashProps(uint32_t sourceHash)
{
    if (sourceHash != 0) {
        string propValue;
        sprintf(propValue, "0x%0X", sourceHash);

        addProp(kPropSourceHash, propValue.c_str());
    }
}

void KTXImage::addAddressProps(const char* addressContent)
{
    addProp(kPropAddress, addressContent);
}
void KTXImage::addFilterProps(const char* filterContent)
{
    addProp(kPropFilter, filterContent);
}

void KTXImage::addChannelProps(const char* channelContent)
{
    addProp(kPropChannels, channelContent);

    // When mixing channels into one, need names for each channel
    // Nrm - normal
    // Alb - albedo
    // Mtl - metallic
    // Rgh - roughness
    // Emi - emissive
    // Hgt - heightmap
    // Rfl - reflectance
    // X/Ign - ignore
    // Msk  - mask
    // Ocl - ambient occlusion
    // Trn - translusency amount

    // this is tied to postSwizzle if present, otherwise to preSwizzle
    // on platforms without swizzles, then have to convert.

    // Nrm.x,Nrm.y,X,X
    // Alb.ra,Alb.ga,Alb.ba,Alb.a, // indicate premul channel
    // Rgh.x,Mtl.x,X,X
}

bool KTXImage::isPremul() const
{
    string channels = getProp(kPropChannels);
    if (strstr(channels.c_str(), "Alb.ra,Alb.ga,Alb.ba") != nullptr) {
        return true;
    }
    return false;
}

void KTXImage::toPropsData(vector<uint8_t>& propsData) const
{
    for (const auto& prop : props) {
        uint32_t size = uint32_t(prop.first.length() + 1 +
                                 prop.second.length() + 1);
        const uint8_t* sizeData = (const uint8_t*)&size;

        // add size
        propsData.insert(propsData.end(), sizeData, sizeData + sizeof(uint32_t));

        const char* key = prop.first.c_str();
        const char* value = prop.second.c_str();

        // add null-terminate key, and value data
        propsData.insert(propsData.end(), key, key + prop.first.length() + 1);
        propsData.insert(propsData.end(), value, value + prop.second.length() + 1);

        // padding to 4 byte multiple
        uint32_t numPadding = 3 - ((size + 3) % 4);
        if (numPadding) {
            uint8_t paddingData[4] = {0, 0, 0, 0};
            propsData.insert(propsData.end(), paddingData, paddingData + numPadding);
        }
    }

    // TODO: this needs to pad to 16-bytes, so may need a prop for that
}

void KTXImage::initMipLevels(bool doMipmaps, int32_t mipMinSize, int32_t mipMaxSize, int32_t mipSkip, uint32_t& numSkippedMips)
{
    // dst levels
    int32_t w = width;
    int32_t h = height;
    int32_t d = depth;

    numSkippedMips = mipSkip;

    bool needsDownsample = (numSkippedMips > 0) || (w > mipMaxSize || h > mipMaxSize);

    int32_t maxMipLevels = 16;  // 64K x 64K

    // can still downsample src multiple times even with only 1 level exported
    if ((!doMipmaps) && needsDownsample) {
        maxMipLevels = 1;
    }

    KTXImageLevel level;
    //level.offset = 0; // compute later, once know ktx vs. ktx2
    //level.lengthCompressed = 0;

    mipLevels.clear();

    if (doMipmaps || needsDownsample) {
        bool keepMip =
            (mipSkip != 0 && numSkippedMips >= (uint32_t)mipSkip) ||
            ((w >= mipMinSize && w <= mipMaxSize) &&
             (h >= mipMinSize && h <= mipMaxSize));

        if (keepMip) {
            level.length = mipLengthCalc(w, h);

            if (mipLevels.empty()) {
                // adjust the top dimensions
                width = w;
                height = h;
                depth = d;
            }
            mipLevels.push_back(level);
        }
        else {
            if (mipLevels.empty()) {
                numSkippedMips++;
            }
        }

        do {
            mipDown(w, h, d);

            keepMip =
                (mipSkip != 0 && numSkippedMips >= (uint32_t)mipSkip) ||
                ((w >= mipMinSize && w <= mipMaxSize) &&
                 (h >= mipMinSize && h <= mipMaxSize));

            if (keepMip && (mipLevels.size() < (size_t)maxMipLevels)) {
                // length needs to be multiplied by chunk size before writing out
                level.length = mipLengthCalc(w, h);

                if (mipLevels.empty()) {
                    // adjust the top dimensions
                    width = w;
                    height = h;
                    depth = d;
                }

                mipLevels.push_back(level);
            }
            else {
                if (mipLevels.empty()) {
                    numSkippedMips++;
                }
            }

        } while (w > 1 || h > 1 || d > 1);
    }
    else {
        // length needs to be multiplied by chunk size before writing out
        level.length = mipLengthCalc(w, h);

        mipLevels.push_back(level);
    }

    header.numberOfMipmapLevels = mipLevels.size();

    header.pixelWidth = width;
    header.pixelHeight = height;
}

void KTXImage::initMipLevels(size_t mipOffset)
{
    // largest mips are first in file
    uint32_t numMips = max(1u, header.numberOfMipmapLevels);

    int numChunks = header.totalChunks();

    mipLevels.reserve(numMips);
    mipLevels.clear();

    size_t offset = mipOffset;

    int32_t w = width;
    int32_t h = height;
    int32_t d = depth;

    for (uint32_t i = 0; i < numMips; ++i) {
        size_t dataSize = mipLengthCalc(w, h);

        uint32_t levelSize = dataSize * numChunks;

        // TODO: align mip offset to multiple of 4 bytes for KTX1, may need for kTX2
        // make sure when adding up offsets with length to include this padding
        //        if (!skipImageLength) {
        //            offset += 3 - (offset & 3); // align level to 4 bytes
        //        }

        // compute dataSize from header data
        if (!skipImageLength) {
            // advance past the length
            offset += sizeof(uint32_t);
        }

        // level holds single texture size not level size, but offset reflects level start
        KTXImageLevel level = {offset, 0, dataSize};
        mipLevels.push_back(level);

        offset += levelSize;

        mipDown(w, h, d);
    }
}

bool KTXImage::validateMipLevels() const
{
    if (skipImageLength)
        return true;

    bool isValid = true;

    int numChunks = header.totalChunks();

    // validate that no weird size to image
    for (uint32_t i = 0; i < mipLevels.size(); ++i) {
        auto& level = mipLevels[i];

        const uint8_t* levelSizeField = (const uint8_t*)fileData + level.offset - sizeof(uint32_t);
        uint32_t levelSizeFromRead = *(const uint32_t*)levelSizeField;

        // cube only stores size of one face, ugh
        if (textureType == MyMTLTextureTypeCube) {
            levelSizeFromRead *= 6;
        }

        if (levelSizeFromRead != level.length * numChunks) {
            KLOGE("kram", "mip %d levelSize mismatch %d %d", i, (int)levelSizeFromRead, (int)level.length);
            isValid = false;
            break;
        }
    }

    return isValid;
}

const char* textureTypeName(MyMTLTextureType textureType)
{
    const char* name = "";
    switch (textureType) {
        case MyMTLTextureTypeCubeArray:
            return "CubeA";
        case MyMTLTextureType2DArray:
            return "2DA";
        case MyMTLTextureType1DArray:
            return "1DA";

        case MyMTLTextureTypeCube:
            return "Cube";
        case MyMTLTextureType3D:
            return "3D";
        case MyMTLTextureType2D:
            return "2D";
    }
    return name;
}

// KTX2 layout
//// Data Format Descriptor
//uint32_t dfdTotalSize = 0;
//continue
//    dfDescriptorBlock dfdBlock
//          ︙
//until dfdByteLength read
//
//// Key/Value Data
//continue
//    uint32_t   keyAndValueByteLength = 0;
//    Byte     keyAndValue[keyAndValueByteLength]
//    align(4) valuePadding
//                    ︙
//until kvdByteLength read
//if (sgdByteLength > 0)
//    align(8) sgdPadding
//
//// Supercompression Global Data
//Byte supercompressionGlobalData[sgdByteLength];
//
//// Mip Level Array
//for (int32_t i = 0; i < levelCount; ++i) {
//    Byte     levelImages[bytesOfLevelImages];
//}

// this converts KTX2 to KTX1, will do opposite if writing to ktx2
// may just use the converters and only write to KTX1 from kram for now
// can use ktx2ktx2 and ktx2sc to supercompress, and kramv can use this to open and view data as a KTX1 file.
// ignoring Basis and supercompression data, etc.

bool KTXImage::openKTX2(const uint8_t* imageData, size_t imageDataLength, bool isInfoOnly)
{
    if ((size_t)imageDataLength < sizeof(KTX2Header)) {
        return false;
    }

    // identifier not detected
    if (memcmp(imageData, kKTX2Identifier, sizeof(kKTX2Identifier)) != 0) {
        return false;
    }

    // these are set after decompress of mips if needed
    //fileData = imageData;
    //fileDataLength = (int)imageDataLength;

    // copy out the header,
    const KTX2Header& header2 = *(const KTX2Header*)imageData;

    if (header2.supercompressionScheme == KTX2SupercompressionBasisLZ) {
        KLOGE("kram", "Basis decode not yet supported");
        return false;
    }

    if (header2.supercompressionScheme != KTX2SupercompressionNone &&
        header2.supercompressionScheme != KTX2SupercompressionZstd &&
        header2.supercompressionScheme != KTX2SupercompressionZlib) {
        KLOGE("kram", "Unknown supercompression %d", header2.supercompressionScheme);
        return false;
    }

    bool isCompressed = header2.supercompressionScheme != KTX2SupercompressionNone;

    // This typically means UASTC encoding + zstd supercompression, and code doesn't handle that below yet
    if (header2.vkFormat == 0) {
        KLOGE("kram", "UASTC and vkFormat of 0 decode not yet supported");
        return false;
    }

    header.pixelWidth = header2.pixelWidth;
    header.pixelHeight = header2.pixelHeight;
    header.pixelDepth = header2.pixelDepth;

    header.numberOfArrayElements = header2.layerCount;
    header.numberOfFaces = header2.faceCount;
    header.numberOfMipmapLevels = max((int)header2.levelCount, 1);

    // for 2d and 3d textures
    width = header.pixelWidth;
    height = max(1, (int)header.pixelHeight);
    depth = max(1, (int)header.pixelDepth);

    textureType = header.metalTextureType();

    int32_t numChunks = totalChunks();

    vector<KTXImageLevel> levels;
    uint32_t levelOffset = sizeof(KTX2Header);
    for (uint32_t i = 0; i < header.numberOfMipmapLevels; ++i) {
        // ktx2 stores levels in same order as ktx1, but larger mips occur later in the file
        // only KTX2 writes this array out due to lengthCompressed field.

        auto level = *(const KTXImageLevel*)(imageData + levelOffset + sizeof(KTXImageLevel) * i);

        assert(level.length % numChunks == 0);

        // ktx2 doesn't mess up the length like ktx1 does on cubemaps.  This is the true level length.
        // Divide by chunks so that can compare against KTX1 mips from initMipLevels.
        level.length /= numChunks;

        levels.push_back(level);
    }

    // convert format to MyMTLPixelFormat
    pixelFormat = vulkanToMetalFormat(header2.vkFormat);

    // kram can only load a subset of format
    if (pixelFormat == MyMTLPixelFormatInvalid) {
        KLOGE("kram", "unsupported texture format VK_FORMAT %u", header2.vkFormat);
        return false;
    }

    // transfer key-value data pairs
    // bytesOfKeyValueData will be updated if props written out
    header.bytesOfKeyValueData = 0;
    initProps(imageData + header2.kvdByteOffset, header2.kvdByteLength);

    // skip parsing the levels
    if (isInfoOnly) {
        skipImageLength = true;
        fileData = imageData;
        fileDataLength = imageDataLength;

        // copy this in to return as info
        supercompressionType = (KTX2Supercompression)header2.supercompressionScheme;

        // copy these over from ktx2
        mipLevels = levels;

        // copy the original ktx2 levels, this includes mip compression
        isCompressed =
            (mipLevels[0].lengthCompressed > 0) &&
            ((mipLevels[0].length * numChunks) != mipLevels[0].lengthCompressed);

        if (!isCompressed) {
            for (auto& level : mipLevels) {
                level.lengthCompressed = 0;
            }
        }
        return true;
    }

    if (!isCompressed) {
        // Note: this is aliasing the mips from a ktx2 file into a ktx1 KTXImage
        // This is highly unsafe but mostly works for input.

        // Note: KTX2 also doesn't have the length field embedded the mipData
        // so need to be able to set skipLength to unify the mipgen if aliasing the mip data
        // Only reading this format, never writing it out.
        skipImageLength = true;

        fileData = imageData;
        fileDataLength = imageDataLength;

        // these are mip offset for KTX2 file
        size_t mipOffset = header2.sgdByteOffset + header2.sgdByteLength;
        initMipLevels(mipOffset);

        // TODO: KTX1 packs rows to 4 bytes, but KTX2 packs tightly to 1
        // for now just reverse the ktx2 mips back to ktx1, aliasing fileData
        // Note ktx2 is align 1, and ktx1 is align 4, so can't always do this

        for (uint32_t i = 0; i < header.numberOfMipmapLevels; ++i) {
            const auto& level2 = levels[i];
            auto& level1 = mipLevels[i];

            // the offsets are reversed in ktx2 file
            level1.offset = level2.offset;
            assert(level1.lengthCompressed == 0);

            if (level1.length != level2.length) {
                // This is likely due to the reversal of mips
                // but many of the test images from libkx are hitting this, fix this issue.

                KLOGE("kram", "mip sizes aren't equal");
                return false;
            }
        }
    }
    else {
        // This is decompressing KTX2 into KTX1
        size_t mipOffset = sizeof(KTXHeader) + header.bytesOfKeyValueData;
        initMipLevels(mipOffset);

        // compute the decompressed size
        // Note: initMipLevels computes but doesn't store this
        reserveImageData();

        // TODO: may need to fill out length field in fileData

        supercompressionType = (KTX2Supercompression)header2.supercompressionScheme;

        // need to decompress mips here
        for (uint32_t i = 0; i < header.numberOfMipmapLevels; ++i) {
            // compresssed level
            const auto& level2 = levels[i];
            const uint8_t* srcData = imageData + level2.offset;

            // uncompressed level
            auto& level1 = mipLevels[i];
            level1.lengthCompressed = level2.lengthCompressed;      // need this for copyLevel to have enough data
            uint8_t* dstData = (uint8_t*)fileData + level1.offset;  // can const_cast, since class owns data

            if (!unpackLevel(i, srcData, dstData)) {
                return false;
            }

            // have decompressed here, so set to 0
            level1.lengthCompressed = 0;
        }

        // have decompressed ktx1, so change back to None
        supercompressionType = KTX2SupercompressionNone;
    }

    return true;
}

bool KTXImage::unpackLevel(uint32_t mipNumber, const uint8_t* srcData, uint8_t* dstData) const
{
    // uncompressed level
    uint32_t numChunks = totalChunks();
    const auto& level = mipLevels[mipNumber];
    size_t dstDataSize = level.length * numChunks;

    if (level.lengthCompressed == 0) {
        memcpy(dstData, srcData, dstDataSize);
    }
    else {
        size_t srcDataSize = level.lengthCompressed;

        // TODO: use basis transcoder (single file) for Basis UASTC here, then don't need libktx yet
        // wont work for BasisLZ (which is ETC1S).
        // copy this in to return as info

        switch (supercompressionType) {
            case KTX2SupercompressionZstd: {
                // decompress from zstd directly into ktx1 ordered chunk
                // Note: decode fails with FSE_decompress.
                size_t dstDataSizeZstd = ZSTD_decompress(
                    dstData, dstDataSize,
                    srcData, srcDataSize);

                if (ZSTD_isError(dstDataSizeZstd)) {
                    KLOGE("kram", "decode mip zstd failed");
                    return false;
                }
                if (dstDataSizeZstd != dstDataSize) {
                    KLOGE("kram", "decode mip zstd size not expected");
                    return false;
                }
                break;
            }

            case KTX2SupercompressionZlib: {
                // can use miniz or libCompression
                mz_ulong dstDataSizeMiniz = 0;
                if (mz_uncompress(dstData, &dstDataSizeMiniz,
                                  srcData, srcDataSize) != MZ_OK) {
                    KLOGE("kram", "decode mip zlib failed");
                    return false;
                }
                if (dstDataSizeMiniz != dstDataSize) {
                    KLOGE("kram", "decode mip zlib size not expected");
                    return false;
                }

                break;
            }

            // already checked at top of function
            default: {
                return false;
            }
        }
    }

    return true;
}

vector<uint8_t>& KTXImage::imageData()
{
    return _imageData;
}

const vector<uint8_t>& KTXImage::imageData() const
{
    return _imageData;
}

void KTXImage::reserveImageData()
{
    int32_t numChunks = totalChunks();

    // on KTX1 the last mip is the smallest and last in the file
    // on KTX2 the first mip is the largest and last in the file
    const auto& firstMip = mipLevels[0];
    const auto& lastMip = mipLevels[header.numberOfMipmapLevels - 1];

    size_t firstMipOffset =
        firstMip.offset + firstMip.length * numChunks;
    size_t lastMipOffset =
        lastMip.offset + lastMip.length * numChunks;
    size_t totalSize = max(firstMipOffset, lastMipOffset);

    reserveImageData(totalSize);
}

void KTXImage::reserveImageData(size_t totalSize)
{
    _imageData.resize(totalSize);
    memset(_imageData.data(), 0, totalSize);

    fileDataLength = totalSize;
    fileData = _imageData.data();
}

}  // namespace kram
