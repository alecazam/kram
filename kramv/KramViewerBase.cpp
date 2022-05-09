#include "KramViewerBase.h"

namespace kram {
using namespace simd;
using namespace NAMESPACE_STL;

int32_t ShowSettings::totalChunks() const
{
    int32_t one = 1;
    return std::max(one, faceCount) *
           std::max(one, arrayCount) *
           std::max(one, sliceCount);
}

const char *ShowSettings::meshNumberName(uint32_t meshNumber_) const
{
    const char *text = "";

    switch (meshNumber_) {
        case 0:
            text = "Plane";
            break;
        case 1:
            text = "Box";
            break;
        case 2:
            text = "Sphere";
            break;
        case 3:
            text = "Sphere MirrorU";
            break;
        case 4:
            text = "Capsule";
            break;
        default:
            break;
    }

    return text;
}

const char *ShowSettings::meshNumberText() const
{
    const char *text = "";

    switch (meshNumber) {
        case 0:
            text = "Shape Plane";
            break;
        case 1:
            text = "Shape Box";
            break;
        case 2:
            text = "Shape Sphere";
            break;
        case 3:
            text = "Shape Sphere MirrorU";
            break;
        case 4:
            text = "Shape Capsule";
            break;
        default:
            break;
    }

    return text;
}

const char *ShowSettings::shapeChannelText() const
{
    const char *text = "";

    switch (shapeChannel) {
        case ShapeChannelNone:
            text = "Show Off";
            break;
        case ShapeChannelUV0:
            text = "Show UV0";
            break;
        case ShapeChannelNormal:
            text = "Show Normal";
            break;
        case ShapeChannelTangent:
            text = "Show Tangent";
            break;
        case ShapeChannelBitangent:
            text = "Show Bitangent";
            break;
        case ShapeChannelDepth:
            text = "Show Depth";
            break;
        case ShapeChannelFaceNormal:
            text = "Show Faces";
            break;
        // case ShapeChannelBumpNormal: text = "Show Bumps"; break;
        case ShapeChannelMipLevel:
            text = "Show Mip Levels";
            break;
        default:
            break;
    }

    return text;
}

const char *ShowSettings::debugModeText() const
{
    const char *text = "";

    switch (debugMode) {
        case DebugModeNone:
            text = "Debug Off";
            break;
        case DebugModeTransparent:
            text = "Debug Transparent";
            break;
        case DebugModeNonZero:
            text = "Debug NonZero";
            break;
        case DebugModeColor:
            text = "Debug Color";
            break;
        case DebugModeGray:
            text = "Debug Gray";
            break;
        case DebugModeHDR:
            text = "Debug HDR";
            break;
        case DebugModePosX:
            text = "Debug +X";
            break;
        case DebugModePosY:
            text = "Debug +Y";
            break;
        case DebugModeCircleXY:
            text = "Debug XY>=1";
            break;
        default:
            break;
    }
    return text;
}

const char *ShowSettings::lightingModeText() const
{
    const char *text = "";

    switch (lightingMode) {
        case LightingModeDiffuse:
            text = "Light Diffuse";
            break;
        case LightingModeSpecular:
            text = "Light Specular";
            break;
        case LightingModeNone:
            text = "Light None";
            break;
        default:
            break;
    }
    return text;
}

bool ShowSettings::isEyedropperFromDrawable()
{
    return meshNumber > 0 || isPreview || isShowingAllLevelsAndMips ||
           shapeChannel > 0;
}

void ShowSettings::advanceMeshNumber(bool decrement)
{
    int32_t numEnums = meshCount;
    int32_t number = meshNumber;
    if (decrement) {
        number += numEnums - 1;
    }
    else {
        number += 1;
    }

    meshNumber = number % numEnums;
}

void ShowSettings::advanceShapeChannel(bool decrement)
{
    int32_t numEnums = ShapeChannelCount;
    int32_t mode = shapeChannel;
    if (decrement) {
        mode += numEnums - 1;
    }
    else {
        mode += 1;
    }

    shapeChannel = (ShapeChannel)(mode % numEnums);

    // skip this channel for now, in ortho it's mostly pure white
    if (shapeChannel == ShapeChannelDepth) {
        advanceShapeChannel(decrement);
    }
}

void ShowSettings::advanceLightingMode(bool decrement)
{
    int32_t numEnums = LightingModeCount;
    int32_t number = lightingMode;
    if (decrement) {
        number += numEnums - 1;
    }
    else {
        number += 1;
    }

    lightingMode = (LightingMode)(number % numEnums);
}

void ShowSettings::advanceDebugMode(bool decrement)
{
    int32_t numEnums = DebugModeCount;
    int32_t mode = debugMode;
    if (decrement) {
        mode += numEnums - 1;
    }
    else {
        mode += 1;
    }

    debugMode = (DebugMode)(mode % numEnums);

    MyMTLPixelFormat format = (MyMTLPixelFormat)originalFormat;
    bool isHdr = isHdrFormat(format);

    // DONE: work on skipping some of these based on image
    bool isAlpha = isAlphaFormat(format);
    bool isColor = isColorFormat(format);

    if (debugMode == DebugModeTransparent && (numChannels <= 3 || !isAlpha)) {
        advanceDebugMode(decrement);
    }

    // 2 channel textures don't really have color or grayscale pixels
    if (debugMode == DebugModeColor && (numChannels <= 2 || !isColor)) {
        advanceDebugMode(decrement);
    }

    if (debugMode == DebugModeGray && numChannels <= 2) {
        advanceDebugMode(decrement);
    }

    if (debugMode == DebugModeHDR && !isHdr) {
        advanceDebugMode(decrement);
    }

    // for 3 and for channel textures could skip these with more info about image
    // (hasColor) if (_showSettings->debugMode == DebugModeGray && !hasColor)
    // advanceDebugMode(isShiftKeyDown);

    bool isNormal = texContentType == TexContentTypeNormal;
    bool isSDF = texContentType == TexContentTypeSDF;
    
    // for normals show directions
    if (debugMode == DebugModePosX && !(isNormal || isSDF)) {
        advanceDebugMode(decrement);
    }
    if (debugMode == DebugModePosY && !(isNormal)) {
        advanceDebugMode(decrement);
    }
    if (debugMode == DebugModeCircleXY && !(isNormal)) {
        advanceDebugMode(decrement);
    }

    // TODO: have a clipping mode against a variable range too, only show pixels
    // within that range to help isolate problem pixels.  Useful for depth, and
    // have auto-range scaling for it and hdr. make sure to ignore 0 or 1 for
    // clear color of farPlane.
}

void ShowSettings::updateUVPreviewState()
{
    if (is3DView) {
        if (uvPreviewFrames > 0) {
            if (isUVPreview) {
                if (uvPreview < 1.0)
                    uvPreview += uvPreviewStep;
            }
            else
            {
                if (uvPreview > 0.0)
                    uvPreview -= uvPreviewStep;
            }

            uvPreview = saturate(uvPreview);
        }
    }
    else {
        // This hides the uvView even when switchig back to 3d shape
        //uvPreview = 0.0;
    }
    
    // stop the frame update
    if (uvPreview == 0.0f || uvPreview == 1.0f) {
        uvPreviewFrames = 0;
    }
}

void printChannels(string &tmp, const string &label, float4 c,
                   int32_t numChannels, bool isFloat, bool isSigned)
{
    if (isFloat || isSigned) {
        switch (numChannels) {
            case 1:
                sprintf(tmp, "%s%.3f\n", label.c_str(), c.r);
                break;
            case 2:
                sprintf(tmp, "%s%.3f, %.3f\n", label.c_str(), c.r, c.g);
                break;
            case 3:
                sprintf(tmp, "%s%.3f, %.3f, %.3f\n", label.c_str(), c.r, c.g, c.b);
                break;
            case 4:
                sprintf(tmp, "%s%.3f, %.3f, %.3f, %.3f\n", label.c_str(), c.r, c.g, c.b,
                        c.a);
                break;
        }
    }
    else {
        // unorm data, 8-bit values displayed
        c *= 255.1f;

        switch (numChannels) {
            case 1:
                sprintf(tmp, "%s%.0f\n", label.c_str(), c.r);
                break;
            case 2:
                sprintf(tmp, "%s%.0f, %.0f\n", label.c_str(), c.r, c.g);
                break;
            case 3:
                sprintf(tmp, "%s%.0f, %.0f, %.0f\n", label.c_str(), c.r, c.g, c.b);
                break;
            case 4:
                sprintf(tmp, "%s%.0f, %.0f, %.0f, %.0f\n", label.c_str(), c.r, c.g, c.b,
                        c.a);
                break;
        }
    }
}

float4x4 matrix4x4_translation(float tx, float ty, float tz)
{
    float4x4 m = {(float4){1, 0, 0, 0},
                  (float4){0, 1, 0, 0},
                  (float4){0, 0, 1, 0},
                  (float4){tx, ty, tz, 1}};
    return m;
}

float4x4 matrix4x4_rotation(float radians, vector_float3 axis)
{
    axis = vector_normalize(axis);
    float ct = cosf(radians);
    float st = sinf(radians);
    float ci = 1 - ct;
    float x = axis.x, y = axis.y, z = axis.z;

    float4x4 m = {
        (float4){ ct + x * x * ci,     y * x * ci + z * st, z * x * ci - y * st, 0},
        (float4){ x * y * ci - z * st,     ct + y * y * ci, z * y * ci + x * st, 0},
        (float4){ x * z * ci + y * st, y * z * ci - x * st,     ct + z * z * ci, 0},
        (float4){                   0,                   0,                   0, 1}
    };
    return m;
}

float4x4 perspective_rhs(float fovyRadians, float aspectXtoY, float nearZ, float farZ, bool isReverseZ)
{
    // form tangents
    float tanY = tanf(fovyRadians * 0.5f);
    float tanX = tanY * aspectXtoY;

    // currently symmetric
    // all postive values from center
    float4 tangents = { tanY, tanY, tanX, tanX };
    tangents *= nearZ;
    
    float t =  tangents.x;
    float b = -tangents.y;
    float r =  tangents.z;
    float l = -tangents.w;
    
    float dx = (r - l);
    float dy = (t - b);
     
    float xs = 2.0f * nearZ / dx;
    float ys = 2.0f * nearZ / dy;

    // 0.5x?
    float xoff = (r + l) / dx;
    float yoff = (t + b) / dy;

    float m22;
    float m23;

    if (isReverseZ) {
        // zs drops out since zs = inf / -inf = 1, 1-1 = 0
        // z' = near / -z
        
        m22 = 0;
        m23 = nearZ;
    }
    else {
        float zs = farZ / (nearZ - farZ);

        m22 = zs;
        m23 = zs * nearZ;
    }
     
    float4x4 m = {
        (float4){ xs,       0,   0,  0 },
        (float4){  0,      ys,   0,  0 },
        (float4){  xoff, yoff, m22, -1 },
        (float4){  0,       0, m23,  0 }
    };
     
    return m;
}

float4x4 orthographic_rhs(float width, float height, float nearZ, float farZ,
                          bool isReverseZ)
{
    // float aspectRatio = width / height;
    float xs = 2.0f / width;
    float ys = 2.0f / height;

    float xoff = 0.0f;  // -0.5f * width;
    float yoff = 0.0f;  // -0.5f * height;

    float dz = -(farZ - nearZ);
    float zs = 1.0f / dz;

    float m22 = zs;
    float m23 = zs * nearZ;

    // revZ, can't use infiniteZ with ortho view
    if (isReverseZ) {
        m22 = -m22;
        m23 = 1.0f - m23;
    }

    float4x4 m = {
        (float4){xs, 0, 0, 0},
        (float4){0, ys, 0, 0},
        (float4){0, 0, m22, 0},
        (float4){xoff, yoff, m23, 1}
    };
    return m;
}

}  // namespace kram
