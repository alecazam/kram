#include "KramViewerBase.h"

namespace kram
{
using namespace simd;
using namespace std;

int32_t ShowSettings::totalLevels() const {
    int32_t one = 1;
    return std::max(one, faceCount) * std::max(one, arrayCount) * std::max(one, sliceCount);
}

void ShowSettings::advanceDebugMode(bool isShiftKeyDown) {
    int32_t numEnums = DebugModeCount;
    if (isShiftKeyDown) {
        debugMode = (DebugMode)(((int32_t)debugMode - 1 + numEnums) % numEnums);
    }
    else {
        debugMode = (DebugMode)(((int32_t)debugMode + 1) % numEnums);
    }
    
    MyMTLPixelFormat format = (MyMTLPixelFormat)originalFormat;
    bool isHdr = isHdrFormat(format);
    
    // DONE: work on skipping some of these based on image
    bool isAlpha = isAlphaFormat(format);
    bool isColor = isColorFormat(format);
    
    if (debugMode == DebugModeTransparent && (numChannels <= 3 || !isAlpha)) {
        advanceDebugMode(isShiftKeyDown);
    }
    
    // 2 channel textures don't really color or grayscale pixels
    if (debugMode == DebugModeColor && (numChannels <= 2 || !isColor)) {
        advanceDebugMode(isShiftKeyDown);
    }
    
    if (debugMode == DebugModeGray && numChannels <= 2) {
        advanceDebugMode(isShiftKeyDown);
    }
    
    if (debugMode == DebugModeHDR && !isHdr) {
        advanceDebugMode(isShiftKeyDown);
    }
    
    // for 3 and for channel textures could skip these with more info about image (hasColor)
    // if (_showSettings->debugMode == DebugModeGray && !hasColor) advanceDebugMode(isShiftKeyDown);

    // for normals show directions
    if (debugMode == DebugModePosX && !(isNormal || isSDF)) {
        advanceDebugMode(isShiftKeyDown);
    }
    if (debugMode == DebugModePosY && !(isNormal)) {
        advanceDebugMode(isShiftKeyDown);
    }
    
    // TODO: have a clipping mode against a variable range too, only show pixels within that range
    // to help isolate problem pixels.  Useful for depth, and have auto-range scaling for it and hdr.
    // make sure to ignore 0 or 1 for clear color of farPlane.
    
}

void printChannels(string& tmp, const string& label, float4 c, int32_t numChannels, bool isFloat, bool isSigned)
{
    if (isFloat || isSigned) {
        switch(numChannels) {
            case 1: sprintf(tmp, "%s%.3f\n", label.c_str(), c.r); break;
            case 2: sprintf(tmp, "%s%.3f, %.3f\n", label.c_str(), c.r, c.g); break;
            case 3: sprintf(tmp, "%s%.3f, %.3f, %.3f\n", label.c_str(), c.r, c.g, c.b); break;
            case 4: sprintf(tmp, "%s%.3f, %.3f, %.3f, %.3f\n", label.c_str(), c.r, c.g, c.b, c.a); break;
        }
    }
    else {
        // unorm data, 8-bit values displayed
        c *= 255.1f;
        
        switch(numChannels) {
            case 1: sprintf(tmp, "%s%.0f\n", label.c_str(), c.r); break;
            case 2: sprintf(tmp, "%s%.0f, %.0f\n", label.c_str(), c.r, c.g); break;
            case 3: sprintf(tmp, "%s%.0f, %.0f, %.0f\n", label.c_str(), c.r, c.g, c.b); break;
            case 4: sprintf(tmp, "%s%.0f, %.0f, %.0f, %.0f\n", label.c_str(), c.r, c.g, c.b, c.a); break;
        }
    }
}

float4x4 matrix4x4_translation(float tx, float ty, float tz)
{
    float4x4 m = {
        (float4){ 1,   0,  0,  0 },
        (float4){ 0,   1,  0,  0 },
        (float4){ 0,   0,  1,  0 },
        (float4){ tx, ty, tz,  1 }
    };
    return m;
}

//static float4x4 matrix4x4_rotation(float radians, vector_float3 axis)
//{
//    axis = vector_normalize(axis);
//    float ct = cosf(radians);
//    float st = sinf(radians);
//    float ci = 1 - ct;
//    float x = axis.x, y = axis.y, z = axis.z;
//
//    float4x4 m = {
//        (float4){ ct + x * x * ci,     y * x * ci + z * st, z * x * ci - y * st, 0},
//        (float4){ x * y * ci - z * st,     ct + y * y * ci, z * y * ci + x * st, 0},
//        (float4){ x * z * ci + y * st, y * z * ci - x * st,     ct + z * z * ci, 0},
//        (float4){                   0,                   0,                   0, 1}
//    };
//    return m;
//}
//
//float4x4 perspective_rhs(float fovyRadians, float aspect, float nearZ, float farZ)
//{
//    float ys = 1 / tanf(fovyRadians * 0.5);
//    float xs = ys / aspect;
//    float zs = farZ / (nearZ - farZ);
//
//    TODO: handle isReverseZ if add option to draw with perspective
//    float4x4 m = {
//        (float4){ xs,   0,          0,  0 },
//        (float4){  0,  ys,          0,  0 },
//        (float4){  0,   0,         zs, -1 },
//        (float4){  0,   0, nearZ * zs,  0 }
//    };
//    return m;
//}

float4x4 orthographic_rhs(float width, float height, float nearZ, float farZ, bool isReverseZ)
{
    //float aspectRatio = width / height;
    float xs = 2.0f/width;
    float ys = 2.0f/height;
    
    float xoff = 0.0f; // -0.5f * width;
    float yoff = 0.0f; // -0.5f * height;
    
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
        (float4){ xs,   0,      0,  0 },
        (float4){  0,   ys,     0,  0 },
        (float4){ 0,     0,     m22, 0 },
        (float4){ xoff, yoff,    m23,  1 }
    };
    return m;
}

}
