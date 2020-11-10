// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include "KTXImage.h"

#include <stdio.h>

#include <algorithm>
#include <map>
#include <unordered_map>

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

using namespace std;

//---------------------------------------------------

enum GLFormat {
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
        const char* metalName_,
        const char* vulkanName_,
        const char* glName_,

        MyMTLPixelFormat metalType_,
        VKFormat vulkanType_,
        GLFormat glType_,
        GLFormatBase glBase_,

        uint8_t blockDimsX_,
        uint8_t blockDimsY_,

        uint8_t blockSize_,
        uint8_t numChannels_,

        KTXFormatFlags flags_)
    {
        metalName = metalName_;
        vulkanName = vulkanName_;
        glName = glName_;

        metalType = metalType_;
        vulkanType = vulkanType_;
        glType = glType_;
        glBase = glBase_;

        blockSize = blockSize_;
        numChannels = numChannels_;

        blockDimsX = blockDimsX_;
        blockDimsY = blockDimsY_;

        flags = flags_;
    }

    const char* metalName;
    const char* vulkanName;
    const char* glName;

    uint16_t metalType;
    uint16_t vulkanType;
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

static unordered_map<uint32_t /*MyMTLPixelFormat*/, KTXFormatInfo> gFormatTable;

bool initFormats()
{
// the following table could be included multiple ties to build switch statements, but instead use a hashmap
#define KTX_FORMAT(fmt, metalType, vulkanType, glType, glBase, x, y, blockSize, numChannels, flags) \
    gFormatTable[(uint32_t)metalType] = KTXFormatInfo(                                              \
        #metalType, #vulkanType, #glType,                                                           \
        metalType, vulkanType, glType, glBase,                                                      \
        x, y, blockSize, numChannels, (flags));

    KTX_FORMAT(Invalid, MyMTLPixelFormatInvalid, VK_FORMAT_UNDEFINED, GL_FORMAT_UNKNOWN, GL_RGBA, 1, 1, 0, 0, 0)

    // BC
    KTX_FORMAT(BC1rgb, MyMTLPixelFormatBC1_RGBA, VK_FORMAT_BC1_RGB_UNORM_BLOCK, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_RGB, 4, 4, 8, 3, FLAG_ENC_BC)
    KTX_FORMAT(BC1rgbs, MyMTLPixelFormatBC1_RGBA_sRGB, VK_FORMAT_BC1_RGB_SRGB_BLOCK, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, GL_SRGB, 4, 4, 8, 3, FLAG_ENC_BC | FLAG_SRGB)

    KTX_FORMAT(BC3, MyMTLPixelFormatBC3_RGBA, VK_FORMAT_BC3_UNORM_BLOCK, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_RGBA, 4, 4, 16, 4, FLAG_ENC_BC)
    KTX_FORMAT(BC3s, MyMTLPixelFormatBC3_RGBA_sRGB, VK_FORMAT_BC3_SRGB_BLOCK, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, GL_SRGB_ALPHA, 4, 4, 16, 4, FLAG_ENC_BC | FLAG_SRGB)

    KTX_FORMAT(BC4, MyMTLPixelFormatBC4_RUnorm, VK_FORMAT_BC4_UNORM_BLOCK, GL_COMPRESSED_RED_RGTC1, GL_RED, 4, 4, 8, 1, FLAG_ENC_BC)
    KTX_FORMAT(BC4sn, MyMTLPixelFormatBC4_RSnorm, VK_FORMAT_BC4_SNORM_BLOCK, GL_COMPRESSED_SIGNED_RED_RGTC1, GL_RED, 4, 4, 8, 1, FLAG_ENC_BC | FLAG_SIGNED)

    KTX_FORMAT(BC5, MyMTLPixelFormatBC5_RGUnorm, VK_FORMAT_BC5_UNORM_BLOCK, GL_COMPRESSED_RG_RGTC2, GL_RG, 4, 4, 16, 2, FLAG_ENC_BC)
    KTX_FORMAT(BC5sn, MyMTLPixelFormatBC5_RGSnorm, VK_FORMAT_BC5_SNORM_BLOCK, GL_COMPRESSED_SIGNED_RG_RGTC2, GL_RG, 4, 4, 16, 2, FLAG_ENC_BC | FLAG_SIGNED)

    KTX_FORMAT(BC6h, MyMTLPixelFormatBC6H_RGBFloat, VK_FORMAT_BC6H_SFLOAT_BLOCK, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB, GL_RGB, 4, 4, 16, 4, FLAG_ENC_BC | FLAG_16F)
    KTX_FORMAT(BC6uh, MyMTLPixelFormatBC6H_RGBUfloat, VK_FORMAT_BC6H_SFLOAT_BLOCK, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB, GL_RGB, 4, 4, 16, 4, FLAG_ENC_BC | FLAG_16F)

    KTX_FORMAT(BC7, MyMTLPixelFormatBC7_RGBAUnorm, VK_FORMAT_BC7_UNORM_BLOCK, GL_COMPRESSED_RGBA_BPTC_UNORM_ARB, GL_RGBA, 4, 4, 16, 4, FLAG_ENC_BC)
    KTX_FORMAT(BC7s, MyMTLPixelFormatBC7_RGBAUnorm_sRGB, VK_FORMAT_BC7_SRGB_BLOCK, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB, GL_SRGB_ALPHA, 4, 4, 16, 4, FLAG_ENC_BC | FLAG_SRGB)

    // ETC
    KTX_FORMAT(ETCr, MyMTLPixelFormatEAC_R11Unorm, VK_FORMAT_EAC_R11_UNORM_BLOCK, GL_COMPRESSED_R11_EAC, GL_RED, 4, 4, 8, 1, FLAG_ENC_ETC)
    KTX_FORMAT(ETCrsn, MyMTLPixelFormatEAC_R11Snorm, VK_FORMAT_EAC_R11_SNORM_BLOCK, GL_COMPRESSED_SIGNED_R11_EAC, GL_RED, 4, 4, 8, 1, FLAG_ENC_ETC | FLAG_SIGNED)

    KTX_FORMAT(ETCrg, MyMTLPixelFormatEAC_RG11Unorm, VK_FORMAT_EAC_R11G11_UNORM_BLOCK, GL_COMPRESSED_RG11_EAC, GL_RG, 4, 4, 16, 2, FLAG_ENC_ETC)
    KTX_FORMAT(ETCrgsn, MyMTLPixelFormatEAC_RG11Snorm, VK_FORMAT_EAC_R11G11_SNORM_BLOCK, GL_COMPRESSED_SIGNED_RG11_EAC, GL_RG, 4, 4, 16, 2, FLAG_ENC_ETC | FLAG_SIGNED)

    KTX_FORMAT(ETCrgb, MyMTLPixelFormatETC2_RGB8, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, GL_COMPRESSED_RGB8_ETC2, GL_RGB, 4, 4, 8, 3, FLAG_ENC_ETC)
    KTX_FORMAT(ETCsrgb, MyMTLPixelFormatETC2_RGB8_sRGB, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, GL_COMPRESSED_SRGB8_ETC2, GL_SRGB, 4, 4, 8, 3, FLAG_ENC_ETC | FLAG_SRGB)

    KTX_FORMAT(ETCrgba, MyMTLPixelFormatEAC_RGBA8, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, GL_COMPRESSED_RGBA8_ETC2_EAC, GL_RGBA, 4, 4, 16, 4, FLAG_ENC_ETC)
    KTX_FORMAT(ETCsrgba, MyMTLPixelFormatEAC_RGBA8_sRGB, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, GL_COMPRESSED_SRGBA8_ETC2_EAC, GL_SRGB_ALPHA, 4, 4, 16, 4, FLAG_ENC_ETC | FLAG_SRGB)

    // ASTC
    KTX_FORMAT(ASTC4x4, MyMTLPixelFormatASTC_4x4_LDR, VK_FORMAT_ASTC_4x4_UNORM_BLOCK, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, GL_RGBA, 4, 4, 16, 4, FLAG_ENC_ASTC)
    KTX_FORMAT(ASTC4x4s, MyMTLPixelFormatASTC_4x4_sRGB, VK_FORMAT_ASTC_4x4_SRGB_BLOCK, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, GL_SRGB_ALPHA, 4, 4, 16, 4, FLAG_ENC_ASTC | FLAG_SRGB)
    KTX_FORMAT(ASTC4x4h, MyMTLPixelFormatASTC_4x4_HDR, VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, GL_RGBA, 4, 4, 16, 4, FLAG_ENC_ASTC | FLAG_16F)  // gl type same as LDR

    KTX_FORMAT(ASTC4x4, MyMTLPixelFormatASTC_5x5_LDR, VK_FORMAT_ASTC_5x5_UNORM_BLOCK, GL_COMPRESSED_RGBA_ASTC_5x5_KHR, GL_RGBA, 5, 5, 16, 4, FLAG_ENC_ASTC)
    KTX_FORMAT(ASTC5x5s, MyMTLPixelFormatASTC_5x5_sRGB, VK_FORMAT_ASTC_5x5_SRGB_BLOCK, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR, GL_SRGB_ALPHA, 5, 5, 16, 4, FLAG_ENC_ASTC | FLAG_SRGB)
    KTX_FORMAT(ASTC5x5h, MyMTLPixelFormatASTC_5x5_HDR, VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT, GL_COMPRESSED_RGBA_ASTC_5x5_KHR, GL_RGBA, 5, 5, 16, 4, FLAG_ENC_ASTC | FLAG_16F)  // gl type same as LDR

    KTX_FORMAT(ASTC6x6, MyMTLPixelFormatASTC_6x6_LDR, VK_FORMAT_ASTC_6x6_UNORM_BLOCK, GL_COMPRESSED_RGBA_ASTC_6x6_KHR, GL_RGBA, 6, 6, 16, 4, FLAG_ENC_ASTC)
    KTX_FORMAT(ASTC6x6s, MyMTLPixelFormatASTC_6x6_sRGB, VK_FORMAT_ASTC_6x6_SRGB_BLOCK, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR, GL_SRGB_ALPHA, 6, 6, 16, 4, FLAG_ENC_ASTC | FLAG_SRGB)
    KTX_FORMAT(ASTC6x6h, MyMTLPixelFormatASTC_6x6_HDR, VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT, GL_COMPRESSED_RGBA_ASTC_6x6_KHR, GL_RGBA, 6, 6, 16, 4, FLAG_ENC_ASTC | FLAG_16F)  // gl type same as LDR

    KTX_FORMAT(ASTC8x8, MyMTLPixelFormatASTC_8x8_LDR, VK_FORMAT_ASTC_8x8_UNORM_BLOCK, GL_COMPRESSED_RGBA_ASTC_8x8_KHR, GL_RGBA, 8, 8, 16, 4, FLAG_ENC_ASTC)
    KTX_FORMAT(ASTC8x8s, MyMTLPixelFormatASTC_8x8_sRGB, VK_FORMAT_ASTC_8x8_SRGB_BLOCK, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR, GL_SRGB_ALPHA, 8, 8, 16, 4, FLAG_ENC_ASTC | FLAG_SRGB)
    KTX_FORMAT(ASTC8x8h, MyMTLPixelFormatASTC_8x8_HDR, VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT, GL_COMPRESSED_RGBA_ASTC_8x8_KHR, GL_RGBA, 8, 8, 16, 4, FLAG_ENC_ASTC | FLAG_16F)  // gl type same as LDR

    // Explicit
    KTX_FORMAT(EXPr8, MyMTLPixelFormatR8Unorm, VK_FORMAT_R8_UNORM, GL_R8, GL_RED, 1, 1, 1, 1, 0)
    KTX_FORMAT(EXPrg8, MyMTLPixelFormatRG8Unorm, VK_FORMAT_R8G8_UNORM, GL_RG8, GL_RG, 1, 1, 2, 2, 0)
    KTX_FORMAT(EXPrgba8, MyMTLPixelFormatRGBA8Unorm, VK_FORMAT_R8G8B8A8_UNORM, GL_RGBA8, GL_RGBA, 1, 1, 4, 4, 0)
    KTX_FORMAT(EXPsrgba8, MTLPixelFormatRGBA8Unorm_sRGB, VK_FORMAT_R8G8B8A8_SRGB, GL_SRGB8_ALPHA8, GL_SRGB_ALPHA, 1, 1, 4, 4, FLAG_SRGB)

    KTX_FORMAT(EXPr16f, MyMTLPixelFormatR16Float, VK_FORMAT_R16_SFLOAT, GL_R16F, GL_RED, 1, 1, 2, 1, FLAG_16F)
    KTX_FORMAT(EXPrg16f, MyMTLPixelFormatRG16Float, VK_FORMAT_R16G16_SFLOAT, GL_RG16F, GL_RG, 1, 1, 4, 2, FLAG_16F)
    KTX_FORMAT(EXPrgba16f, MyMTLPixelFormatRGBA16Float, VK_FORMAT_R16G16B16A16_SFLOAT, GL_RGBA16F, GL_RGBA, 1, 1, 8, 4, FLAG_16F)

    KTX_FORMAT(EXPr32f, MyMTLPixelFormatR32Float, VK_FORMAT_R32_SFLOAT, GL_R32F, GL_RED, 1, 1, 4, 1, FLAG_32F)
    KTX_FORMAT(EXPrg32f, MyMTLPixelFormatRG32Float, VK_FORMAT_R32G32_SFLOAT, GL_RG32F, GL_RG, 1, 1, 8, 2, FLAG_32F)
    KTX_FORMAT(EXPrg32f, MyMTLPixelFormatRGBA32Float, VK_FORMAT_R32G32B32A32_SFLOAT, GL_RGBA32F, GL_RGBA, 1, 1, 16, 4, FLAG_32F)

    return true;
}

static bool gPreInitFormats = initFormats();

//---------------------------------------------------------

const KTXFormatInfo& formatInfo(MyMTLPixelFormat format)
{
    const auto& it = gFormatTable.find(format);
    assert(it != gFormatTable.end());
    return it->second;
}

bool isFloatFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.is16F() || it.is32F();
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

int blockSizeOfFormat(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.blockSize;
}

int metalType(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.metalType;
}

const char* metalTypeName(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.metalName + 2;  // skip the "My" part
}

const char* vulkanTypeName(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.vulkanName;
}

int vulkanType(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.vulkanType;
}

const char* glTypeName(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.glName;
}

int glType(MyMTLPixelFormat format)
{
    const auto& it = formatInfo(format);
    return it.glType;
}

// https://docs.unity3d.com/ScriptReference/Experimental.Rendering.GraphicsFormat.html
// Unity only handles 4,5,6,8,10,12 square block dimensions

int KTXImage::mipLevelSize(int width, int height) const
{
    int count = blockCount(width, height);
    int size = blockSize();
    return count * size;
}

int KTXImage::blockCount(int width, int height) const
{
    Int2 dims = blockDims();

    width = (width + dims.x - 1) / dims.x;
    height = (height + dims.y - 1) / dims.y;

    int count = width * height;
    return count;
}

Int2 KTXImage::blockDims() const
{
    return kram::blockDimsOfFormat(pixelFormat);
}

int KTXImage::blockSize() const
{
    return kram::blockSizeOfFormat(pixelFormat);
}

MyMTLPixelFormat KTXHeader::metalFormat() const
{
    for (const auto& it : gFormatTable) {
        // this isn't 1:1, since ASTC_HDR aren't unique types, prefer ldr type
        const KTXFormatInfo& info = it.second;
        if (info.glType == glInternalFormat && !(info.isASTC() && info.isHDR())) {
            return (MyMTLPixelFormat)info.metalType;
        }
    }

    assert(false);
    return MyMTLPixelFormatInvalid;
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

int KTXHeader::totalMipLevels() const
{
    return totalChunks() * std::max(1, (int)numberOfMipmapLevels);
}

int KTXHeader::totalChunks() const
{
    return std::max(1, (int)numberOfArrayElements) *
           std::max(1, (int)numberOfFaces) * std::max(1, (int)pixelDepth);
}

int KTXImage::totalMipLevels() const
{
    return header.totalMipLevels();
}

int KTXImage::totalChunks() const
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

bool KTXImage::open(const uint8_t* imageData, int imageDataLength,
                    bool skipImageLength_)
{
    skipImageLength = skipImageLength_;
    fileData = imageData;
    fileDataLength = (int)imageDataLength;

    // copy out the header, TODO: should make sure bytes exist
    header = *(const KTXHeader*)fileData;

    width = header.pixelWidth;
    // for 2d and 3d textures
    height = max(1, (int)header.pixelHeight);
    depth = max(1, (int)header.pixelDepth);

    textureType = header.metalTextureType();

    // convert keyValues to props vector
    initProps();

    // find prop for KTXmetalFormat, and use that
    // only if not found, then fallback on gl format conversion from KTXHeader
    // astc hdr doesn't have unique gl types.
    const string& metalFormat = getProp(kPropMetalFormat);
    if (!metalFormat.empty()) {
        if (sscanf(metalFormat.c_str(), "%d", &pixelFormat) != 1) {
            return false;
        }

        // TODO: validate that format is one supported
    }
    else {
        pixelFormat = header.metalFormat();
    }

    return initMipLevels(true);
}

void KTXImage::initProps()
{
    props.clear();

    if (header.bytesOfKeyValueData > 0) {
        const uint8_t* keyValuesStart =
            (const uint8_t*)fileData + sizeof(KTXHeader);
        const uint8_t* keyValuesEnd =
            (const uint8_t*)fileData + sizeof(KTXHeader) + header.bytesOfKeyValueData;

        while (keyValuesStart < keyValuesEnd) {
            size_t dataSize = *(const uint32_t*)keyValuesStart;
            keyValuesStart += sizeof(uint32_t);

            // split key off at null char
            const uint8_t* keyStart = keyValuesStart;
            const uint8_t* valueStart = NULL;
            for (int i = 0; i < (int)dataSize; ++i) {
                if (keyValuesStart[i] == '\0') {
                    valueStart = &keyValuesStart[i + 1];
                    break;
                }
            }

            //LOGD("KTXImage", "KTXProp '%s': %s\n", keyStart, valueStart);

            props.push_back(make_pair(string((const char*)keyStart), string((const char*)valueStart)));

            // pad to 4 byte alignment
            int valuePadding = 3 - ((dataSize + 3) % 4);
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

void KTXImage::toPropsData(vector<uint8_t>& propsData)
{
    for (const auto& prop : props) {
        uint32_t size = prop.first.length() + 1 +
                        prop.second.length() + 1;
        const uint8_t* sizeData = (const uint8_t*)&size;

        // add size
        propsData.insert(propsData.end(), sizeData, sizeData + sizeof(uint32_t));

        const char* key = prop.first.c_str();
        const char* value = prop.second.c_str();

        // add null-terminate key, and value data
        propsData.insert(propsData.end(), key, key + prop.first.length() + 1);
        propsData.insert(propsData.end(), value, value + prop.second.length() + 1);

        // padding to 4 byte multiple
        uint32_t padding = 3 - ((size + 3) % 4);
        if (padding) {
            uint8_t paddingData[4] = {0, 0, 0, 0};
            propsData.insert(propsData.end(), paddingData, paddingData + padding);
        }
    }

    // TODO: this needs to padd to 16-bytes, so may need a prop for that
}

bool KTXImage::initMipLevels(bool vaidateLengthFromRead)
{
    const uint8_t* mips = (const uint8_t*)fileData + sizeof(KTXHeader) +
                          header.bytesOfKeyValueData;

    // largest mips are first in file
    int numMips = max(1, (int)header.numberOfMipmapLevels);
    int numArrays = max(1, (int)header.numberOfArrayElements);
    int numFaces = max(1, (int)header.numberOfFaces);
    int numSlices = max(1, (int)depth);

    int totalMipLevels = header.totalMipLevels();

    mipLevels.reserve(totalMipLevels);
    mipLevels.clear();

    size_t totalDataSize = sizeof(KTXHeader) + header.bytesOfKeyValueData;
    size_t blockSize = this->blockSize();

    int w = width;
    int h = height;

    for (int i = 0; i < numMips; ++i) {
        size_t dataSize = mipLevelSize(w, h);
        ;
        // compute dataSize from header data

        if (!skipImageLength) {
            // read data size
            // 4-byte dataSize throws off alignment of mips to block size
            // would need to pad after this by block size

            // validate that no weird size to image
            if (vaidateLengthFromRead) {
                int fileDataSize = *(const uint32_t*)mips;
                if (fileDataSize != (int)dataSize) {
                    return false;
                }
            }

            // advance past the length
            mips += sizeof(uint32_t);
            totalDataSize += sizeof(uint32_t);
        }

        for (int array = 0; array < numArrays; ++array) {
            for (int face = 0; face < numFaces; ++face) {
                for (int slice = 0; slice < numSlices; ++slice) {
                    const uint8_t* srcImageData = mips;
                    mips += dataSize;
                    totalDataSize += dataSize;

                    // assumes all images are in same mmap file, so can just
                    // alias the offset these offsets need to be at a multiple
                    // of the block size
                    size_t offset = srcImageData - fileData;
                    KTXImageLevel level = {offset, dataSize};
                    mipLevels.push_back(level);

                    if (skipImageLength) {
                        if ((offset & (blockSize - 1)) != 0) {
                            return false;
                        }
                    }

                    // TODO: pad to 4 on 1/2/3 byte formats
                    // but make sure if this is on every mip or not
                }

                // cube padding to 4 byte alignment
                if (textureType == MyMTLTextureTypeCube) {
                    size_t padding =
                        3 - ((dataSize + 3) % 4);  // 0, 1, 2, 3 -> 0, 3, 2, 1
                    if (padding > 0) {
                        mips += padding;
                        totalDataSize += padding;
                    }

                    if (skipImageLength) {
                        if (padding != 0) {
                            return false;
                        }
                    }
                }
            }
        }

        // mip padding to 4 byte alignment
        size_t padding =
            3 - ((totalDataSize + 3) % 4);  // 0, 1, 2, 3 -> 0, 3, 2, 1
        if (padding > 0) {
            mips += padding;
            totalDataSize += padding;
        }

        if (skipImageLength) {
            if (padding != 0) {
                return false;
            }
        }

        w = (w + 1) / 2;
        h = (h + 1) / 2;
    }

    return true;
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

}  // namespace kram
