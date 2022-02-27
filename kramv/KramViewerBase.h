// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include <cstdint>

#include "KramLib.h"  // for MyMTLPixelFormat
//#include <string>
//#include <simd/simd.h>

// All portable C++ code.

namespace kram {

using namespace NAMESPACE_STL;
using namespace simd;

enum TextureChannels {
    ModeRGBA = 0,

    ModeR001 = 1,
    Mode0G01 = 2,
    Mode00B1 = 3,

    // see grayscale channels
    //    ModeRRR1 = 5,
    //    ModeGGG1 = 6,
    //    ModeBBB1 = 7,

    ModeAAA1 = 8,
};

// Must line up with ShDebugMode
enum DebugMode {
    DebugModeNone = 0,
    DebugModeTransparent,
    DebugModeNonZero,
    DebugModeColor,
    DebugModeGray,
    DebugModeHDR,

    DebugModePosX,
    DebugModePosY,
    DebugModeCircleXY,

    DebugModeCount
};

enum ShapeChannel {
    ShapeChannelNone = 0,

    ShapeChannelDepth,

    ShapeChannelUV0,

    ShapeChannelFaceNormal,  // gen from dfdx and dfdy

    ShapeChannelNormal,  // vertex normal
    ShapeChannelTangent,
    ShapeChannelBitangent,

    ShapeChannelMipLevel,  // can estimate mip chose off dfdx/dfdy, and pseudocolor

    // don't need bump, since can already see it, but what if combined diffuse +
    // normal ShapeChannelBumpNormal,

    ShapeChannelCount
};

enum LightingMode {
    LightingModeDiffuse = 0,   // amb + diffuse
    LightingModeSpecular = 1,  // amb + diffuse + specular
    LightingModeNone = 2,      // no lighting, just mips
    
    LightingModeCount,
};

class ShowSettings {
public:
    // Can mask various channels (r/g/b/a only, vs. all), may also add toggle of
    // channel
    TextureChannels channels;

    // this is gap used for showAll
    int32_t showAllPixelGap = 2;

    // These control which texture is viewed in single texture mode
    int32_t mipNumber = 0;
    int32_t mipCount = 1;

    int32_t faceNumber = 0;
    int32_t faceCount = 0;

    int32_t arrayNumber = 0;
    int32_t arrayCount = 0;

    int32_t sliceNumber = 0;
    int32_t sliceCount = 0;

    int32_t totalChunks() const;

    // DONE: hook all these up to shader and view
    bool isHudShown = true;

    // transparency checkboard under the image
    bool isCheckerboardShown = false;

    // draw a 1x1 or blockSize grid, note ASTC has non-square grid sizes
    bool isPixelGridShown = false;
    bool isBlockGridShown = false;
    bool isAtlasGridShown = false;

    bool isAnyGridShown() const
    {
        return isPixelGridShown || isBlockGridShown || isAtlasGridShown;
    }

    // show all mips, faces, arrays all at once
    bool isShowingAllLevelsAndMips = false;

    // expands uv from [0,1] to [0,2] in shader to see the repeat pattern
    bool isWrap = false;

    bool isNormal = false;
    bool isSigned = false;
    bool isPremul = false;  // needed for png which only holds unmul
    bool isSwizzleAGToRG = false;
    bool isSDF = false;

    // this mode shows the content with lighting or with bilinear/mips active
    bool isPreview = false;

    // the 2d view doesn't want to inset pixels for clamp, or point sampling is
    // thrown off expecially on small 4x4 textures
#if USE_PERSPECTIVE
    bool is3DView = true; // only use perspective
#else
    bool is3DView = false;
#endif
    
    // TODO: Might eliminate this, since mips are either built with or without
    // srgb and disabling with a MTLView caused many flags to have to be set on
    // MTLTexture
    // bool isSRGBShown = true;

    // whether to use normal to tangent (false), or vertex tangents (true)
    bool useTangent = true;

    // draw with reverseZ to better match perspective
    bool isReverseZ = true;

    // whether files are pulled from zip archive.
    bool isArchive = false;

    // whether files are pulled from folder(s)
    bool isFolder = false;

    // image vs. gltf model
    bool isModel = false;
    
    // can sample from drawable or from single source texture
    bool isEyedropperFromDrawable();

    // can have up to 5 channels (xyz as xy, 2 other channels)
    int32_t numChannels = 0;

    // this could be boundary of all visible images, so that pan doesn't go flying
    // off to nowhere
    int32_t imageBoundsX = 0;  // px
    int32_t imageBoundsY = 0;  // px

    // size of the block, used in block grid drawing
    int32_t blockX = 1;
    int32_t blockY = 1;

    // set when isGridShown is true
    int32_t gridSizeX = 1;
    int32_t gridSizeY = 1;

    // for eyedropper, lookup this pixel value, and return it to CPU
    int32_t textureLookupX = 0;
    int32_t textureLookupY = 0;

    int32_t lastCursorX = 0;
    int32_t lastCursorY = 0;

    // exact pixel in the mip level
    int32_t textureLookupMipX = 0;
    int32_t textureLookupMipY = 0;

    int32_t textureResultX = 0;
    int32_t textureResultY = 0;
    float4 textureResult;

    // size of the view and its contentScaleFactor
    int32_t viewSizeX = 1;  // px
    int32_t viewSizeY = 1;  // px
    float viewContentScaleFactor = 1.0f;

    // cursor is in view coordinates, but doesn't include contentScaleFactor
    int32_t cursorX = 0;
    int32_t cursorY = 0;

    // these control the view transform, zoomFit fits the image vertically to he
    // view bound
    float zoomFit = 1.0f;
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;

    DebugMode debugMode = DebugModeNone;

    ShapeChannel shapeChannel = ShapeChannelNone;

    LightingMode lightingMode = LightingModeDiffuse;

    bool isInverted;

    // cached on load, raw info about the texture from libkram
    string imageInfo;
    string imageInfoVerbose;

    // format before any transcode to supported formats
    MyMTLPixelFormat originalFormat;
    MyMTLPixelFormat decodedFormat;

    void advanceMeshNumber(bool decrement);
    void advanceDebugMode(bool decrement);
    void advanceShapeChannel(bool decrement);
    void advanceLightingMode(bool decrement);

    const char *meshNumberText() const;
    const char *shapeChannelText() const;
    const char *debugModeText() const;
    const char *lightingModeText() const;
    
    const char *meshNumberName(uint32_t meshNumber) const;
    
    float imageAspectRatio() const {
        float ar = 1.0f;
        if (meshNumber == 0 && !isModel && imageBoundsY > 0)
            ar = imageBoundsX / (float)imageBoundsY;
        return ar;
    }
    string lastFilename;
    double lastTimestamp = 0.0;

    int32_t meshNumber = 0;
    int32_t meshCount = 5;
};

float4x4 matrix4x4_translation(float tx, float ty, float tz);

float4x4 perspective_rhs(float fovyRadians, float aspect, float nearZ, float
                         farZ, bool isReverseZ);

float4x4 orthographic_rhs(float width, float height, float nearZ, float farZ,
                          bool isReverseZ);

float4x4 matrix4x4_rotation(float radians, vector_float3 axis);

void printChannels(string &tmp, const string &label, float4 c,
                   int32_t numChannels, bool isFloat, bool isSigned);

}  // namespace kram
