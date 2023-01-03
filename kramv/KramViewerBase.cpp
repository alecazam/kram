#include "KramViewerBase.h"

#include "simdjson/simdjson.h"

namespace kram {
using namespace simd;
using namespace NAMESPACE_STL;

#define ArrayCount(x) (sizeof(x) / sizeof(x[0]))

#ifdef NDEBUG
bool doPrintPanZoom = false;
#else
bool doPrintPanZoom = false;
#endif

// Writing out to rgba32 for sampling, but unorm formats like ASTC and RGBA8
// are still off and need to use the following.
float  toSnorm8(float c)  { return (255.0f / 127.0f) * c - (128.0f / 127.0f); }
float2 toSnorm8(float2 c) { return (255.0f / 127.0f) * c - (128.0f / 127.0f); }
float3 toSnorm8(float3 c) { return (255.0f / 127.0f) * c - (128.0f / 127.0f); }
float4 toSnorm8(float4 c) { return (255.0f / 127.0f) * c - (128.0f / 127.0f); }

float4 toSnorm(float4 c)  { return 2.0f * c - 1.0f; }

inline float4 toPremul(const float4 &c)
{
    // premul with a
    float4 cpremul = c;
    float a = c.a;
    cpremul.w = 1.0f;
    cpremul *= a;
    return cpremul;
}

inline bool almost_equal_elements(float3 v, float tol)
{
    return (fabs(v.x - v.y) < tol) && (fabs(v.x - v.z) < tol);
}

inline const float3x3& toFloat3x3(const float4x4 &m) { return (const float3x3 &)m; }

float4 inverseScaleSquared(const float4x4 &m)
{
    float3 scaleSquared = float3m(length_squared(m.columns[0].xyz),
                                  length_squared(m.columns[1].xyz),
                                  length_squared(m.columns[2].xyz));

    // if uniform, then set scaleSquared all to 1
    if (almost_equal_elements(scaleSquared, 1e-5f)) {
        scaleSquared = float3m(1.0f);
    }

    // don't divide by 0
    float3 invScaleSquared =
        recip(simd::max(float3m(0.0001 * 0.0001), scaleSquared));

    // identify determinant here for flipping orientation
    // all shapes with negative determinant need orientation flipped for
    // backfacing and need to be grouned together if rendering with instancing
    float det = determinant(toFloat3x3(m));

    return float4m(invScaleSquared, det);
}

static string filenameNoExtension(const char* filename)
{
    const char* dotPosStr = strrchr(filename, '.');
    if (dotPosStr == nullptr)
        return filename;
    auto dotPos = dotPosStr - filename;
    
    // now chop off the extension
    string filenameNoExt = filename;
    return filenameNoExt.substr(0, dotPos);
}

static void findPossibleNormalMapFromAlbedoFilename(const char* filename, vector<string>& normalFilenames)
{
    normalFilenames.clear();
    
    string filenameShort = filename;
    
    const char* ext = strrchr(filename, '.');

    const char* dotPosStr = strrchr(filenameShort.c_str(), '.');
    if (dotPosStr == nullptr)
        return;
    
    auto dotPos = dotPosStr - filenameShort.c_str();
    
    // now chop off the extension
    filenameShort = filenameShort.substr(0, dotPos);

    const char* searches[] = { "-a", "-d", "_Color", "_baseColor" };
    
    for (uint32_t i = 0; i < ArrayCount(searches); ++i) {
        const char* search = searches[i];
        if (endsWith(filenameShort, search)) {
            filenameShort = filenameShort.substr(0, filenameShort.length()-strlen(search));
            break;
        }
    }
     
    const char* suffixes[] = { "-n", "_normal", "_Normal" };
    
    string normalFilename;
    for (uint32_t i = 0; i < ArrayCount(suffixes); ++i) {
        const char* suffix = suffixes[i];
        
        // may need to try various names, and see if any exist
        normalFilename = filenameShort;
        normalFilename += suffix;
        normalFilename += ext;
        
        normalFilenames.push_back(normalFilename);
    }
}

// this aliases the existing string, so can't chop extension
inline const char* toFilenameShort(const char* filename) {
    const char* filenameShort = strrchr(filename, '/');
    if (filenameShort == nullptr) {
        filenameShort = filename;
    }
    else {
        filenameShort += 1;
    }
    return filenameShort;
}

static const vector<const char*> supportedModelExt = {
#if USE_GLTF
     ".gltf",
     ".glb",
#endif
#if USE_USD
    ".gltf",
    ".glb",
#endif
};


bool isSupportedModelFilename(const char* filename) {
    for (const char* ext: supportedModelExt) {
        if (endsWithExtension(filename, ext)) {
            return true;
        }
    }
    return false;
}
bool isSupportedArchiveFilename(const char* filename) {
    return endsWithExtension(filename, ".zip");
}

bool isSupportedJsonFilename(const char* filename) {
    return endsWith(filename, "-atlas.json");
}

bool isDirectory(const char* filename) {
    FileHelper fileHelper;
    return fileHelper.isDirectory(filename);
}

int32_t ShowSettings::totalChunks() const
{
    int32_t one = 1;
    return std::max(one, faceCount) *
           std::max(one, arrayCount) *
           std::max(one, sliceCount);
}

File::File(const char* name_, int32_t urlIndex_)
    : name(name_), urlIndex(urlIndex_), nameShort(toFilenameShort(name_))
{
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

string ShowSettings::windowTitleString(const char* filename) const
{    
    // set title to filename, chop this to just file+ext, not directory
    const char* filenameShort = strrchr(filename, '/');
    if (filenameShort == nullptr) {
        filenameShort = filename;
    }
    else {
        filenameShort += 1;
    }
    
    string title = "kramv - ";
    
    if (isModel) {
        title += formatTypeName(originalFormat);
        title += " - ";
        title += filenameShort;
    }
    else {
        // was using subtitle, but that's macOS 11.0 feature.
        title += formatTypeName(originalFormat);
        title += " - ";
        
        // identify what we think the content type is
        const char* typeText = "";
        switch(texContentType) {
            case TexContentTypeAlbedo: typeText = "a"; break;
            case TexContentTypeNormal: typeText = "n"; break;
            case TexContentTypeAO: typeText = "ao"; break;
            case TexContentTypeMetallicRoughness: typeText = "mr"; break;
            case TexContentTypeSDF: typeText = "sdf"; break;
            case TexContentTypeHeight: typeText = "h"; break;
            case TexContentTypeUnknown: typeText = ""; break;
        }
        title += typeText;
        // add some info about the texture to avoid needing to go to info
        // srgb src would be useful too.
        if (texContentType == TexContentTypeAlbedo && isPremul) {
            title += ",p";
            
        }
        title += " - ";
        title += filenameShort;
    }
    
    return title;
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

//--------------------------------

// Want to avoid Apple libs for things that have C++ equivalents.
// simdjson without exceptions isn't super readable or safe looking.
// TODO: see if simdjson is stable enough, using unsafe calls

Data::Data()
{
    _showSettings = new ShowSettings();
    
    _textSlots.resize(2);
}
Data::~Data()
{
    delete _showSettings;
}

bool Data::loadAtlasFile(const char* filename)
{
    using namespace simdjson;
    
    ondemand::parser parser;
    
    // TODO: can just mmap the json to provide
    auto json = padded_string::load(filename);
    auto atlasProps = parser.iterate(json);
       
    // Can use hover or a show all on these entries and names.
    // Draw names on screen using system text in the upper left corner if 1
    // if showing all, then show names across each mip level.  May want to
    // snap to pixels on each mip level so can see overlap.
    
    _showSettings->atlas.clear();
    
    {
        std::vector<double> values;
        string_view atlasName = atlasProps["name"].get_string().value_unsafe();
        
        uint64_t width = atlasProps["width"].get_uint64().value_unsafe();
        uint64_t height = atlasProps["height"].get_uint64().value_unsafe();
    
        uint64_t slice = atlasProps["slice"].get_uint64().value_unsafe();
        
        float uPad = 0.0f;
        float vPad = 0.0f;
        
        if (atlasProps["paduv"].get_array().error() != NO_SUCH_FIELD) {
            values.clear();
            for (auto value : atlasProps["paduv"])
                values.push_back(value.get_double().value_unsafe());
            
            uPad = values[0];
            vPad = values[1];
        }
        else if (atlasProps["padpx"].get_array().error() != NO_SUCH_FIELD) {
            values.clear();
            for (auto value : atlasProps["padpx"])
                values.push_back(value.get_double().value_unsafe());
            
            uPad = values[0];
            vPad = values[1];
            
            uPad /= width;
            vPad /= height;
        }
        
        for (auto regionProps: atlasProps["regions"])
        {
            string_view name = regionProps["name"].get_string().value_unsafe();
            
            float x = 0.0f;
            float y = 0.0f;
            float w = 0.0f;
            float h = 0.0f;
            
            if (regionProps["ruv"].get_array().error() != NO_SUCH_FIELD)
            {
                values.clear();
                for (auto value : regionProps["ruv"])
                    values.push_back(value.get_double().value_unsafe());
            
                // Note: could convert pixel and mip0 size to uv.
                // normalized uv make these easier to draw across all mips
                x = values[0];
                y = values[1];
                w = values[2];
                h = values[3];
            }
            else if (regionProps["rpx"].get_array().error() != NO_SUCH_FIELD)
            {
                values.clear();
                for (auto value : regionProps["rpx"])
                    values.push_back(value.get_double().value_unsafe());
            
                x = values[0];
                y = values[1];
                w = values[2];
                h = values[3];
                
                // normalize to uv using the width/height
                x /= width;
                y /= height;
                w /= width;
                h /= height;
            }
                
            const char* verticalProp = "f"; // regionProps["rot"];
            bool isVertical = verticalProp && verticalProp[0] == 't';
            
            Atlas atlas = {(string)name, x,y, w,h, uPad,vPad, isVertical, (uint32_t)slice};
            _showSettings->atlas.emplace_back(std::move(atlas));
        }
    }
    
    // TODO: also need to be able to bring in vector shapes
    // maybe from svg or files written out from figma or photoshop.
    // Can triangulate those, and use xatlas to pack those.
    // Also xatlas can flatten out a 3d model into a chart.
    
    return true;
}

// opens archive
bool Data::openArchive(const char * zipFilename, int32_t urlIndex)
{
    // grow the array, ptrs so that existing mmaps aren't destroyed
    if (urlIndex >= _containers.size()) {
        _containers.resize(urlIndex + 1, nullptr);
    }
    
    if (_containers[urlIndex] == nullptr)
        _containers[urlIndex] = new FileContainer;
    
    FileContainer& container = *_containers[urlIndex];
    MmapHelper& zipMmap = container.zipMmap;
    ZipHelper& zip = container.zip;
    
    // close any previous zip
    zipMmap.close();
    
    // open the mmap again
    if (!zipMmap.open(zipFilename)) {
        return false;
    }
    if (!zip.openForRead(zipMmap.data(), zipMmap.dataLength())) {
        return false;
    }
    return true;
}

// lists archive into _files
bool Data::listFilesInArchive(int32_t urlIndex)
{
    FileContainer& container = *_containers[urlIndex];
    ZipHelper& zip = container.zip;
    
    // filter out unsupported extensions
    vector<string> extensions = {
        ".ktx", ".ktx2", ".png", ".dds" // textures
#if USE_GLTF
        // TODO: can't support these until have a loader from memory block
        // GLTFAsset requires a URL.
        //, ".glb", ".gltf" // models
#endif
#if USE_USD
        , ".usd", ".usda", ".usb"
#endif
    };
    
    container.zip.filterExtensions(extensions);
    
    // don't switch to empty archive
    if (zip.zipEntrys().empty()) {
        return false;
    }
    
    for (const auto& entry: zip.zipEntrys()) {
        _files.emplace_back(File(entry.filename, urlIndex));
    }
    
    return true;
}

// TODO: can simplify by storing counterpart id when file list is created
bool Data::hasCounterpart(bool increment) {
    if (_files.size() <= 1) {
        return false;
    }
    
    const File& file = _files[_fileIndex];
    string currentFilename = filenameNoExtension(file.nameShort.c_str());
   
    uint32_t nextFileIndex = _fileIndex;
    
    size_t numEntries = _files.size();
    if (increment)
        nextFileIndex++;
    else
        nextFileIndex += numEntries - 1;  // back 1
    
    nextFileIndex = nextFileIndex % numEntries;
    
    const File& nextFile = _files[nextFileIndex];
    string nextFilename = filenameNoExtension(nextFile.nameShort.c_str());
    
    // if short name matches (no ext) then it's a counterpart
    if (currentFilename != nextFilename)
       return false;
    
    return true;
}

bool Data::advanceCounterpart(bool increment) {
    
    if (_files.size() <= 1) {
        return false;
    }
    
    // see if file has counterparts
    const File& file = _files[_fileIndex];
    string currentFilename = filenameNoExtension(file.nameShort.c_str());
    
    // TODO: this should cycle through only the counterparts
    uint32_t nextFileIndex = _fileIndex;
    
    size_t numEntries = _files.size();
    if (increment)
        nextFileIndex++;
    else
        nextFileIndex += numEntries - 1;  // back 1

    nextFileIndex = nextFileIndex % numEntries;
    
    const File& nextFile = _files[nextFileIndex];
    string nextFilename = filenameNoExtension(nextFile.nameShort.c_str());
    
    if (currentFilename != nextFilename)
        return false;
    
    _fileIndex = nextFileIndex;
    
    return _delegate.loadFile(true);
}

bool Data::advanceFile(bool increment) {
    if (_files.empty()) {
        return false;
    }
    
    size_t numEntries = _files.size();
    if (increment)
        _fileIndex++;
    else
        _fileIndex += numEntries - 1;  // back 1

    _fileIndex = _fileIndex % numEntries;
    
    return _delegate.loadFile(true);
}

bool Data::findFilename(const string& filename)
{
    bool isFound = false;
    
    // linear search
    for (const auto& search : _files) {
        if (search.name == filename) {
            isFound = true;
            break;
        }
    }
    return isFound;
}

bool Data::findFilenameShort(const string& filename)
{
    bool isFound = false;
    
    // linear search
    for (const auto& search : _files) {
        if (search.nameShort == filename) {
            isFound = true;
            break;
        }
    }
    return isFound;
}

// rect here is expect xy, wh
bool isPtInRect(float2 pt, float4 r)
{
    return all((pt >= r.xy) & (pt <= r.xy + r.zw));
}

const Atlas* Data::findAtlasAtUV(float2 pt)
{
    if (_showSettings->atlas.empty()) return nullptr;
    if (_showSettings->imageBoundsX == 0) return nullptr;
   
    const Atlas* atlas = nullptr;
    
    // Note: rects are in uv
    
    // This might need to become an atlas array index instead of ptr
    const Atlas* lastAtlas = _showSettings->lastAtlas;
    
    if (lastAtlas) {
        if (isPtInRect(pt, lastAtlas->rect())) {
            atlas = lastAtlas;
        }
    }
    
    if (!atlas) {
        // linear search
        for (const auto& search : _showSettings->atlas) {
            if (isPtInRect(pt, search.rect())) {
                atlas = &search;
                break;
            }
        }
        
        _showSettings->lastAtlas = atlas;
    }
    
    return atlas;
}


bool Data::isArchive() const
{
    //NSArray<NSURL*>* urls_ = (NSArray<NSURL*>*)_delegate._urls;
    //NSURL* url = urls_[_files[_fileIndex].urlIndex];
    //const char* filename = url.fileSystemRepresentation;
    
    string filename = _urls[_files[_fileIndex].urlIndex];
    return isSupportedArchiveFilename(filename.c_str());
}



bool Data::loadFile()
{
    if (isArchive()) {
        return loadFileFromArchive();
    }
    
    // now lookup the filename and data at that entry
    const File& file = _files[_fileIndex];
    const char* filename = file.name.c_str();
    
    string fullFilename = filename;
    auto timestamp = FileHelper::modificationTimestamp(filename);
    
    bool isTextureChanged = _showSettings->isFileChanged(filename, timestamp);
    if (!isTextureChanged) {
        return true;
    }
    
#if USE_GLTF || USE_USD
    bool isModel = isSupportedModelFilename(filename);
    if (isModel) {
        bool success = _delegate.loadModelFile(filename);
        
        if (success) {
            // store the filename
            _showSettings->lastFilename = filename;
            _showSettings->lastTimestamp = timestamp;
        }
        
        return success;
    }
#endif
    
    // have already filtered filenames out, so this should never get hit
    if (!isSupportedFilename(filename)) {
        return false;
    }
    
    // Note: better to extract from filename instead of root of folder dropped
    // or just keep displaying full path of filename.
    
    _archiveName.clear();
    
    vector<string> possibleNormalFilenames;
    string normalFilename;
    bool hasNormal = false;
    
    TexContentType texContentType = findContentTypeFromFilename(filename);
    if (texContentType == TexContentTypeAlbedo) {
        findPossibleNormalMapFromAlbedoFilename(filename, possibleNormalFilenames);
        
        for (const auto& name: possibleNormalFilenames) {
            hasNormal = findFilename(name);
            
            if (hasNormal) {
                normalFilename = name;
                break;
            }
        }
    }
    
    // see if there is an atlas file too, and load the rectangles for preview
    // note sidecar atlas files are a pain to view with a sandbox, may want to
    // splice into ktx/ktx2 files, but no good metadata for png/dds.
    _showSettings->atlas.clear();
    
    string atlasFilename = filenameNoExtension(filename);
    bool hasAtlas = false;
    
    // replace -a, -d, with -atlas.json
    const char* dashPosStr = strrchr(atlasFilename.c_str(), '-');
    if (dashPosStr != nullptr) {
        atlasFilename = atlasFilename.substr(0, dashPosStr - atlasFilename.c_str());
    }
    atlasFilename += "-atlas.json";
    if ( findFilename(atlasFilename.c_str())) {
        if (loadAtlasFile(atlasFilename.c_str())) {
            hasAtlas = true;
        }
    }
    if (!hasAtlas) {
        atlasFilename.clear();
    }
    
    // If it's a compressed file, then set a diff target if a corresponding png
    // is found.  Eventually see if a src dds/ktx/ktx2 exists.  Want to stop
    // using png as source images.  Note png don't have custom mips, unless
    // flattened to one image.  So have to fabricate mips here.  KTXImage
    // can already load up striped png into slices, etc.
    
    string diffFilename = filenameNoExtension(filename);
    bool hasDiff = false;
    
    diffFilename += ".png";
    if ( diffFilename != filename && findFilename(diffFilename.c_str())) {
        // TODO: defer load until diff enabled
        //if ([self loadDiffFile:diffFilename.c_str()]) {
        hasDiff = true;
        //}
    }
    if (!hasDiff) {
        diffFilename.clear();
    }
    
    //-------------------------------
    
    KTXImage image;
    KTXImageData imageDataKTX;
    
    KTXImage imageNormal;
    KTXImageData imageNormalDataKTX;
    
    // this requires decode and conversion to RGBA8u
    if (!imageDataKTX.open(fullFilename.c_str(), image)) {
        return false;
    }
    
    if (hasNormal &&
        imageNormalDataKTX.open(normalFilename.c_str(), imageNormal)) {
        // shaders only pull from albedo + normal on these texture types
        if (imageNormal.textureType == image.textureType &&
            (imageNormal.textureType == MyMTLTextureType2D ||
             imageNormal.textureType == MyMTLTextureType2DArray)) {
            // hasNormal = true;
        }
        else {
            hasNormal = false;
        }
    }
    
    //---------------------------------
    
    // Release any loading model textures
//    Renderer* renderer = (Renderer *)self.delegate;
//    [renderer releaseAllPendingTextures];
//
//    if (![renderer loadTextureFromImage:fullFilename.c_str()
//                              timestamp:timestamp
//                                  image:image
//                            imageNormal:hasNormal ? &imageNormal : nullptr
//                              isArchive:NO]) {
//        return false;
//    }
    
    if (!_delegate.loadTextureFromImage(fullFilename.c_str(), (double)timestamp, image, hasNormal ? &imageNormal : nullptr, false)) {
        return false;
    }
    
    // store the filename
    _showSettings->lastFilename = filename;
    _showSettings->lastTimestamp = timestamp;
    
    return true;
}

bool Data::loadFileFromArchive()
{
    // now lookup the filename and data at that entry
    const File& file = _files[_fileIndex];
    FileContainer& container = *_containers[file.urlIndex];
    ZipHelper& zip = container.zip;
    
    const char* filename = file.name.c_str();
    const auto* entry = zip.zipEntry(filename);
    string fullFilename = entry->filename;
    double timestamp = (double)entry->modificationDate;

    bool isTextureChanged = _showSettings->isFileChanged(filename, timestamp);
    if (!isTextureChanged) {
        return true;
    }
    
// TODO: don't have a version which loads gltf model from memory block
//    bool isModel = isSupportedModelFilename(filename);
//    if (isModel)
//        return [self loadModelFile:filename];
    
    //--------
    
    if (!isSupportedFilename(filename)) {
        return false;
    }
    
    const uint8_t* imageData = nullptr;
    uint64_t imageDataLength = 0;

    // search for main file - can be albedo or normal
    if (!zip.extractRaw(filename, &imageData, imageDataLength)) {
        return false;
    }

    const uint8_t* imageNormalData = nullptr;
    uint64_t imageNormalDataLength = 0;
    
    string normalFilename;
    bool hasNormal = false;
    vector<string> normalFilenames;
    
    TexContentType texContentType = findContentTypeFromFilename(filename);
    if (texContentType == TexContentTypeAlbedo) {
        findPossibleNormalMapFromAlbedoFilename(filename, normalFilenames);
     
        for (const auto& name: normalFilenames) {
            hasNormal = zip.extractRaw(name.c_str(), &imageNormalData,
                                        imageNormalDataLength);
            if (hasNormal) {
                normalFilename = name;
                break;
            }
        }
    }

    //---------------------------

    // files in archive are just offsets into the mmap
    // That's why we can't just pass filenames to the renderer
    KTXImage image;
    KTXImageData imageDataKTX;

    KTXImage imageNormal;
    KTXImageData imageNormalDataKTX;

    if (!imageDataKTX.open(imageData, imageDataLength, image)) {
        return false;
    }

    if (hasNormal && imageNormalDataKTX.open(
                         imageNormalData, imageNormalDataLength, imageNormal)) {
        // shaders only pull from albedo + normal on these texture types
        if (imageNormal.textureType == image.textureType &&
            (imageNormal.textureType == MyMTLTextureType2D ||
             imageNormal.textureType == MyMTLTextureType2DArray)) {
            // hasNormal = true;
        }
        else {
            hasNormal = false;
        }
    }

    //---------------------------------
    
    if (!_delegate.loadTextureFromImage(fullFilename.c_str(), (double)timestamp, image, hasNormal ? &imageNormal : nullptr, true)) {
        return false;
    }
//    Renderer* renderer = (Renderer *)self.delegate;
//    [renderer releaseAllPendingTextures];
//
//    if (![renderer loadTextureFromImage:fullFilename.c_str()
//                              timestamp:(double)timestamp
//                                  image:image
//                            imageNormal:hasNormal ? &imageNormal : nullptr
//                              isArchive:YES]) {
//        return false;
//    }

    //---------------------------------
    
   // NSArray<NSURL*>* urls_ = (NSArray<NSURL*>*)_delegate._urls;
    string archiveURL = _urls[file.urlIndex];
    _archiveName = toFilenameShort(archiveURL.c_str());
    
    return true;
}




void Data::loadFilesFromUrls(vector<string>& urls, bool skipSubdirs)
{
    // Using a member for archives, so limited to one archive in a drop
    // but that's probably okay for now.  Add a separate array of open
    // archives if want > 1.
    
    // copy the existing files list
    string existingFilename;
    if (_fileIndex < (int32_t)_files.size())
        existingFilename = _files[_fileIndex].name;
    
    // Fill this out again
    _files.clear();
    
    // clear pointers
    for (FileContainer* container: _containers)
        delete container;
    _containers.clear();
    
    // this will flatten the list
    int32_t urlIndex = 0;
    
    vector<string> urlsExtracted;
    
    for (const auto& url: urls) {
        // These will flatten out to a list of files
        const char* filename = url.c_str();
        
        if (isSupportedArchiveFilename(filename) &&
            openArchive(filename, urlIndex) &&
            listFilesInArchive(urlIndex))
        {
            urlsExtracted.push_back(filename);
            urlIndex++;
        }
        else if (isDirectory(filename)) {
            
            // this first loads only models, then textures if only those
            listFilesInFolder(url, urlIndex, skipSubdirs);
            
            // could skip if nothing added
            urlsExtracted.push_back(url);
            urlIndex++;
            
            // handle archives within folder
            vector<File> archiveFiles;
            listArchivesInFolder(url, archiveFiles, skipSubdirs);
            
            for (const File& archiveFile: archiveFiles) {
                const char* archiveFilename = archiveFile.name.c_str();
                if (openArchive(archiveFilename, urlIndex) &&
                    listFilesInArchive(urlIndex)) {
                    
                    //NSURL* urlArchive = [NSURL fileURLWithPath:[NSString stringWithUTF8String:archiveFilename]];
                    //[urlsExtracted addObject:urlArchive];
                    urlsExtracted.push_back(archiveFilename);
                    urlIndex++;
                }
                
            }
        }
        else if (isSupportedFilename(filename)
#if USE_GLTF
                 || isSupportedModelFilename(filename)
#endif
                 ) {
            _files.emplace_back(File(filename, urlIndex));
            
            //[urlsExtracted addObject:url];
            urlsExtracted.push_back(filename);
            urlIndex++;
        }
        else if (isSupportedJsonFilename(filename)) {
            _files.emplace_back(File(filename, urlIndex));
            
            //[urlsExtracted addObject:url];
            urlsExtracted.push_back(filename);
            urlIndex++;
        }
        
    }
    
    // sort them by short filename
#if USE_EASTL
    NAMESPACE_STL::quick_sort(_files.begin(), _files.end());
#else
    std::sort(_files.begin(), _files.end());
#endif
    
    // preserve filename before load, and restore that index, by finding
    // that name in refreshed folder list
    _fileIndex = 0;
    if (!existingFilename.empty()) {
        for (uint32_t i = 0; i < _files.size(); ++i) {
            if (_files[i].name == existingFilename) {
                _fileIndex = i;
                break;
            }
        }
    }
    
    // preserve old file selection
    _urls = urlsExtracted;
}

void Data::showEyedropperData(const float2& uv)
{
    string text;
    string tmp;

    float4 c = _showSettings->textureResult;
    int32_t x = _showSettings->textureResultX;
    int32_t y = _showSettings->textureResultY;
    
    // DONE: use these to format the text
    MyMTLPixelFormat format = _showSettings->originalFormat;
    bool isSrgb = isSrgbFormat(format);
    bool isSigned = isSignedFormat(format);

    bool isHdr = isHdrFormat(format);
    bool isFloat = isHdr;

    int32_t numChannels = _showSettings->numChannels;

    bool isNormal = _showSettings->texContentType == TexContentTypeNormal;
    bool isColor = !isNormal;

    bool isDirection = false;
    bool isValue = false;

    if (_showSettings->isEyedropperFromDrawable()) {
        // TODO: could write barycentric, then lookup uv from that
        // then could show the block info.

        // interpret based on shapeChannel, debugMode, etc
        switch (_showSettings->shapeChannel) {
            case ShapeChannelDepth:
                isSigned = false;  // using fract on uv

                isValue = true;
                isFloat = true;
                numChannels = 1;
                break;
            case ShapeChannelUV0:
                isSigned = false;  // using fract on uv

                isValue = true;
                isFloat = true;
                numChannels = 2;  // TODO: fix for 3d uvw
                break;

            case ShapeChannelFaceNormal:
            case ShapeChannelNormal:
            case ShapeChannelTangent:
            case ShapeChannelBitangent:
                isDirection = true;
                numChannels = 3;

                // convert unorm to snnorm
                c = toSnorm(c);
                break;

            case ShapeChannelMipLevel:
                isValue = true;
                isSigned = false;
                isFloat = true;

                // viz is mipNumber as alpha
                numChannels = 1;
                c.r = 4.0 - (c.a * 4.0);
                break;

            default:
                break;
        }

        // TODO: indicate px, mip, etc (f.e. showAll)
        
        // debug mode

        // preview vs. not
    }
    else {
        // this will be out of sync with gpu eval, so may want to only display px
        // from returned lookup this will always be a linear color

        
        // show uv, so can relate to gpu coordinates stored in geometry and find
        // atlas areas
        append_sprintf(text, "uv:%0.3f %0.3f\n",
                       (float)x / _showSettings->imageBoundsX,
                       (float)y / _showSettings->imageBoundsY);

        // pixel at top-level mip
        append_sprintf(text, "px:%d %d\n", x, y);

        // show block num
        int mipLOD = _showSettings->mipNumber;

        int mipX = _showSettings->imageBoundsX;
        int mipY = _showSettings->imageBoundsY;

        mipX = mipX >> mipLOD;
        mipY = mipY >> mipLOD;

        mipX = std::max(1, mipX);
        mipY = std::max(1, mipY);

        mipX = (int32_t)(uv.x * mipX);
        mipY = (int32_t)(uv.y * mipY);

        // TODO: may want to return mip in pixel readback
        // don't have it right now, so don't display if preview is enabled
        if (_showSettings->isPreview)
            mipLOD = 0;

        auto blockDims = blockDimsOfFormat(format);
        if (blockDims.x > 1)
            append_sprintf(text, "bpx: %d %d\n", mipX / blockDims.x,
                           mipY / blockDims.y);

        // TODO: on astc if we have original blocks can run analysis from
        // astc-encoder about each block.

        // show the mip pixel (only if not preview and mip changed)
        if (mipLOD > 0 && !_showSettings->isPreview)
            append_sprintf(text, "mpx: %d %d\n", mipX, mipY);

        // TODO: more criteria here, can have 2 channel PBR metal-roughness
        // also have 4 channel normals where zw store other data.

        bool isDecodeSigned = isSignedFormat(_showSettings->decodedFormat);
        if (isSigned && !isDecodeSigned) {
            c = toSnorm8(c);
        }
    }

    if (isValue) {
        printChannels(tmp, "val: ", c, numChannels, isFloat, isSigned);
        text += tmp;
    }
    else if (isDirection) {
        // print direction
        isFloat = true;
        isSigned = true;

        printChannels(tmp, "dir: ", c, numChannels, isFloat, isSigned);
        text += tmp;
    }
    else if (isNormal) {
        float nx = c.x;
        float ny = c.y;

        // unorm -> snorm
        if (!isSigned) {
            nx = toSnorm8(nx);
            ny = toSnorm8(ny);
        }

        // Note: not clamping nx,ny to < 1 like in shader

        // this is always postive on tan-space normals
        // assuming we're not viewing world normals
        const float maxLen2 = 0.999 * 0.999;
        float len2 = nx * nx + ny * ny;
        if (len2 > maxLen2)
            len2 = maxLen2;

        float nz = sqrt(1.0f - len2);

        // print the underlying color (some nmaps are xy in 4 channels)
        printChannels(tmp, "lin: ", c, numChannels, isFloat, isSigned);
        text += tmp;

        // print direction
        float4 d = float4m(nx, ny, nz, 0.0f);
        isFloat = true;
        isSigned = true;
        printChannels(tmp, "dir: ", d, 3, isFloat, isSigned);
        text += tmp;
    }
    else if (isColor) {
        // DONE: write some print helpers based on float4 and length
        printChannels(tmp, "lin: ", c, numChannels, isFloat, isSigned);
        text += tmp;

        if (isSrgb) {
            // this saturates the value, so don't use for extended srgb
            float4 s = linearToSRGB(c);

            printChannels(tmp, "srg: ", s, numChannels, isFloat, isSigned);
            text += tmp;
        }

        // display the premul values too, but not fully transparent pixels
        if (c.a > 0.0 && c.a < 1.0f) {
            printChannels(tmp, "lnp: ", toPremul(c), numChannels, isFloat, isSigned);
            text += tmp;

            // TODO: do we need the premul srgb color too?
            if (isSrgb) {
                // this saturates the value, so don't use for extended srgb
                float4 s = linearToSRGB(c);

                printChannels(tmp, "srp: ", toPremul(s), numChannels, isFloat,
                              isSigned);
                text += tmp;
            }
        }
    }

    setEyedropperText(text.c_str());

    // TODO: range display of pixels is useful, only showing pixels that fall
    // within a given range, but would need slider then, and determine range of
    // pixels.
    // TODO: Auto-range is also useful for depth (ignore far plane of 0 or 1).

    // TODO: display histogram from compute, bin into buffer counts of pixels

    // DONE: stop clobbering hud text, need another set of labels
    // and a zoom preview of the pixels under the cursor.
    // Otherwise, can't really see the underlying color.

    // TODO: Stuff these on clipboard with a click, or use cmd+C?
}

void Data::setEyedropperText(const char * text)
{
    setTextSlot(kTextSlotEyedropper, text);
}

void Data::setAtlasText(const char * text)
{
    setTextSlot(kTextSlotAtlas, text);
}

string Data::textFromSlots(bool isFileListHidden) const
{
    // combine textSlots
    string text = _textSlots[kTextSlotHud];
    if (!text.empty() && text.back() != '\n')
        text += "\n";
        
    // don't show eyedropper text with table up, it's many lines and overlaps
    if (!isFileListHidden)
    {
        text += _textSlots[kTextSlotEyedropper];
        if (!text.empty() && text.back() != '\n')
            text += "\n";
        
        text += _textSlots[kTextSlotAtlas];
    }
    
    
    return text;
}

void Data::setTextSlot(TextSlot slot, const char* text)
{
    _textSlots[slot] = text;
}

void Data::updateUIAfterLoad()
{
    // TODO: move these to actions, and test their state instead of looking up
    // buttons here and in HandleKey.

    // base on showSettings, hide some fo the buttons
    bool isShowAllHidden =
        _showSettings->totalChunks() <= 1 && _showSettings->mipCount <= 1;

    bool isArrayHidden = _showSettings->arrayCount <= 1;
    bool isFaceSliceHidden =
        _showSettings->faceCount <= 1 && _showSettings->sliceCount <= 1;
    bool isMipHidden = _showSettings->mipCount <= 1;

    bool isJumpToNextHidden = _files.size() <= 1;
    
    bool isJumpToCounterpartHidden = true;
    bool isJumpToPrevCounterpartHidden = true;
    
    if ( _files.size() > 1) {
        isJumpToCounterpartHidden = !hasCounterpart(true);
        isJumpToPrevCounterpartHidden  = !hasCounterpart(false);
    }
    
    bool isRedHidden = _showSettings->numChannels == 0; // models don't show rgba
    bool isGreenHidden = _showSettings->numChannels <= 1;
    bool isBlueHidden = _showSettings->numChannels <= 2 &&
                        _showSettings->texContentType != TexContentTypeNormal;  // reconstruct z = b on normals

    // TODO: also need a hasAlpha for pixels, since many compressed formats like
    // ASTC always have 4 channels but internally store R,RG01,... etc.  Can get
    // more data from swizzle in the props. Often alpha doesn't store anything
    // useful to view.

    // DONE: may want to disable isPremul on block textures that already have
    // premul in data or else premul is applied a second time to the visual

    bool hasAlpha = _showSettings->numChannels >= 3;

    bool isAlphaHidden = !hasAlpha;
    bool isPremulHidden = !hasAlpha;
    bool isCheckerboardHidden = !hasAlpha;

    bool isSignedHidden = !isSignedFormat(_showSettings->originalFormat);
    bool isPlayHidden = !_showSettings->isModel; // only for models
    bool isDiffHidden = _showSettings->isModel; // only for images
    
    _actionPlay->setHidden(isPlayHidden);
    _actionArray->setHidden(isArrayHidden);
    _actionFace->setHidden(isFaceSliceHidden);
    _actionMip->setHidden(isMipHidden);
    _actionShowAll->setHidden(isShowAllHidden);
    
    _actionDiff->setHidden(isDiffHidden);
    _actionItem->setHidden(isJumpToNextHidden);
    _actionPrevItem->setHidden(isJumpToNextHidden);
    
    _actionCounterpart->setHidden(isJumpToCounterpartHidden);
    _actionPrevCounterpart->setHidden(isJumpToPrevCounterpartHidden);
    
    _actionR->setHidden(isRedHidden);
    _actionG->setHidden(isGreenHidden);
    _actionB->setHidden(isBlueHidden);
    _actionA->setHidden(isAlphaHidden);
    
    _actionPremul->setHidden(isPremulHidden);
    _actionSigned->setHidden(isSignedHidden);
    _actionChecker->setHidden(isCheckerboardHidden);
    
    // also need to call after each toggle
    updateUIControlState();
}

void Data::updateUIControlState()
{
    // there is also mixed state, but not using that
    auto On = true;
    auto Off = false;
    
#define toState(x) (x) ? On : Off

    auto showAllState = toState(_showSettings->isShowingAllLevelsAndMips);
    auto premulState = toState(_showSettings->doShaderPremul);
    auto signedState = toState(_showSettings->isSigned);
    auto checkerboardState = toState(_showSettings->isCheckerboardShown);
    auto previewState = toState(_showSettings->isPreview);
    auto gridState = toState(_showSettings->isAnyGridShown());
    auto wrapState = toState(_showSettings->isWrap);
    auto debugState = toState(_showSettings->debugMode != DebugModeNone);
    auto hudState = toState(_showSettings->isHudShown);
    
    TextureChannels &channels = _showSettings->channels;

    auto redState = toState(channels == TextureChannels::ModeR001);
    auto greenState = toState(channels == TextureChannels::Mode0G01);
    auto blueState = toState(channels == TextureChannels::Mode00B1);
    auto alphaState = toState(channels == TextureChannels::ModeAAA1);

    auto arrayState = toState(_showSettings->arrayNumber > 0);
    auto faceState = toState(_showSettings->faceNumber > 0);
    auto mipState = toState(_showSettings->mipNumber > 0);

    auto meshState = toState(_showSettings->meshNumber > 0);
    auto meshChannelState = toState(_showSettings->shapeChannel > 0);
    auto lightingState =
        toState(_showSettings->lightingMode != LightingModeNone);
    auto tangentState = toState(_showSettings->useTangent);

    // TODO: shadow the state on these, so don't have to to go ObjC
    //Renderer* renderer = (Renderer*)self.delegate;
    auto playState = toState(_showSettings->isModel && _showSettings->isPlayAnimations);
    auto verticalState = toState(_showSettings->isVerticalUI);
    auto uiState = toState(_showSettings->isHideUI);
    auto diffState = toState(_showSettings->isDiff);
    
    _actionVertical->setHighlight(verticalState);
    
    // TODO: pass boolean, and change in the call
    _actionPlay->setHighlight(playState);
    _actionHelp->setHighlight(Off);
    _actionInfo->setHighlight(Off);
    _actionHud->setHighlight(hudState);
    
    _actionArray->setHighlight(arrayState);
    _actionFace->setHighlight(faceState);
    _actionMip->setHighlight(mipState);
    
    // these never show check state
    _actionItem->setHighlight(Off);
    _actionPrevItem->setHighlight(Off);
    
    _actionCounterpart->setHighlight(Off);
    _actionPrevCounterpart->setHighlight(Off);
    
    _actionHideUI->setHighlight(uiState); // note below button always off, menu has state
    
    _actionR->setHighlight(redState);
    _actionG->setHighlight(greenState);
    _actionB->setHighlight(blueState);
    _actionA->setHighlight(alphaState);
    
    _actionShowAll->setHighlight(showAllState);
    _actionPreview->setHighlight(previewState);
    _actionDiff->setHighlight(diffState);
    _actionShapeMesh->setHighlight(meshState);
    _actionShapeChannel->setHighlight(meshChannelState);
    _actionLighting->setHighlight(lightingState);
    _actionWrap->setHighlight(wrapState);
    _actionGrid->setHighlight(gridState);
    _actionDebug->setHighlight(debugState);
    _actionTangent->setHighlight(tangentState);
    
    _actionPremul->setHighlight(premulState);
    _actionSigned->setHighlight(signedState);
    _actionChecker->setHighlight(checkerboardState);
}

// TODO: convert to C++ actions, and then call into Base holding all this
// move pan/zoom logic too.  Then use that as start of Win32 kramv.

const Action* Data::actionFromMenu(kram_id menuItem) const
{
    const Action* action = nullptr;
    
    for (const auto& search: _actions) {
        if (search.menuItem == menuItem) {
            action = &search;
            break;
        }
    }
    
    return action;
}

const Action* Data::actionFromButton(kram_id button) const
{
    const Action* action = nullptr;
    
    for (const auto& search: _actions) {
        if (search.button == button) {
            action = &search;
            break;
        }
    }
    
    return action;
}

const Action* Data::actionFromKey(uint32_t keyCode) const
{
    const Action* action = nullptr;
    
    for (const auto& search: _actions) {
        if (search.keyCode == keyCode) {
            action = &search;
            break;
        }
    }
    
    return action;
}

void Data::setLoadedText(string& text)
{
    text = "Loaded ";

    string filename = _showSettings->lastFilename;
    text += toFilenameShort(filename.c_str());

    // archives and file systems have folders, split that off
    string folderName;
    const char* slashPos = strrchr(filename.c_str(), '/');
    if (slashPos != nullptr) {
        folderName = filename.substr(0, slashPos - filename.c_str());
    }

    if (!folderName.empty()) {
        text += " in folder ";
        text += folderName;
    }

    if (!_archiveName.empty()) {
        text += " from archive ";
        text += _archiveName;
    }
}

void Data::initActions()
{
    // Don't reorder without also matching actionPtrs below
    Action actions[] = {
        Action("?", "Help", Key::Slash),
        Action("I", "Info", Key::I),
        Action("H", "Hud", Key::H),
        Action("U", "UI", Key::U),
        Action("V", "UI Vertical", Key::V),

        Action("Q", "Quick Diff", Key::Q), // C/D already taken
        Action("D", "Debug", Key::D),
        Action("G", "Grid", Key::G),
        Action("B", "Checkerboard", Key::B),
        
        Action("", "", Key::A), // sep

        Action("P", "Preview", Key::P),
        Action("W", "Wrap", Key::W),
        Action("8", "Premul", Key::Num8),
        Action("7", "Signed", Key::Num7),
        
        Action("", "", Key::A), // sep

        Action("A", "Show All", Key::A),
        Action("M", "Mip", Key::M),
        Action("F", "Face", Key::F),
        Action("Y", "Array", Key::Y),
        
        Action("↑", "Prev Item", Key::UpArrow),
        Action("↓", "Next Item", Key::DownArrow),
        Action("←", "Prev Counterpart", Key::LeftArrow),
        Action("→", "Next Counterpart", Key::RightArrow),
        
        Action("R", "Reload", Key::R),
        Action("0", "Fit", Key::Num0),

        Action("", "", Key::A), // sep

        Action(" ", "Play", Key::Space),
        Action("6", "Shape UVPreview", Key::Num6),
        Action("S", "Shape", Key::S),
        Action("C", "Shape Channel", Key::C),
        Action("L", "Lighting", Key::L),
        Action("T", "Tangents", Key::T),

        Action("", "", Key::A), // sep

        // make these individual toggles and exclusive toggle off shift
        Action("1", "Red", Key::Num1),
        Action("2", "Green", Key::Num2),
        Action("3", "Blue", Key::Num3),
        Action("4", "Alpha", Key::Num4),
    };
    
    // These have to be in same order as above.  May want to go back to search for text above.
    Action** actionPtrs[] = {
        &_actionHelp,
        &_actionInfo,
        &_actionHud,
        &_actionHideUI,
        &_actionVertical,
       
        &_actionDiff,
        &_actionDebug,
        &_actionGrid,
        &_actionChecker,
        
        &_actionPreview,
        &_actionWrap,
        &_actionPremul,
        &_actionSigned,
        
        &_actionShowAll,
        &_actionMip,
        &_actionFace,
        &_actionArray,
        
        &_actionPrevItem,
        &_actionItem,
        &_actionPrevCounterpart,
        &_actionCounterpart,
        
        &_actionReload,
        &_actionFit,
        
        &_actionPlay,
        &_actionShapeUVPreview,
        &_actionShapeMesh,
        &_actionShapeChannel,
        &_actionLighting,
        &_actionTangent,
        
        &_actionR,
        &_actionG,
        &_actionB,
        &_actionA,
    };

    uint32_t numActions = ArrayCount(actions);
    
    // copy all of them to a vector, and then assign the action ptrs
    for (int32_t i = 0; i < numActions; ++i) {
        Action& action = actions[i];
        const char* icon = action.icon;  // single char
        
        // skip separators
        bool isSeparator = icon[0] == 0;
        if (isSeparator) continue;
        
        _actions.push_back(action);
    }

    // now alias Actions to the vector above
    //assert(_actions.size() == ArrayCount(actionPtrs));
    for (int32_t i = 0; i < _actions.size(); ++i) {
        *(actionPtrs[i]) = &_actions[i];
    }
}

void Data::initDisabledButtons()
{
    // don't want these buttons showing up, menu only
    _actionPrevItem->disableButton();
    _actionItem->disableButton();
    _actionPrevCounterpart->disableButton();
    _actionCounterpart->disableButton();

    _actionHud->disableButton();
    _actionHelp->disableButton();
    _actionHideUI->disableButton();
    _actionVertical->disableButton();
}

void Data::updateEyedropper()
{
    if ((!_showSettings->isHudShown)) {
        return;
    }

    if (_showSettings->imageBoundsX == 0) {
        // TODO: this return will leave old hud text up
        return;
    }

    // getting a lot of repeat cursor locations
    // could have panning underneath cursor to deal with
    if (_showSettings->lastCursorX == _showSettings->cursorX &&
        _showSettings->lastCursorY == _showSettings->cursorY) {
        return;
    }
    
    if (_showSettings->isEyedropperFromDrawable()) {
        _showSettings->lastCursorX = _showSettings->cursorX;
        _showSettings->lastCursorY = _showSettings->cursorY;

        // This just samples from drawable, so no re-render is needed
        // showEyedropperData(float2m(0, 0));
        return;
    }

    // don't wait on renderer to update this matrix
    float4x4 projectionViewModelMatrix =
        computeImageTransform(_showSettings->panX,
                                   _showSettings->panY,
                                   _showSettings->zoom);

    // convert to clip space, or else need to apply additional viewport transform
    float halfX = _showSettings->viewSizeX * 0.5f;
    float halfY = _showSettings->viewSizeY * 0.5f;

    // sometimes get viewSizeX that's scaled by retina, and other times not.
    // account for contentScaleFactor (viewSizeX is 2x bigger than cursorX on
    // retina display) now passing down drawableSize instead of view.bounds.size
    halfX /= (float)_showSettings->viewContentScaleFactor;
    halfY /= (float)_showSettings->viewContentScaleFactor;

    float4 cursor = float4m(_showSettings->cursorX, _showSettings->cursorY, 0.0f, 1.0f);
    
    float4x4 pixelToClipTfm =
    {
        (float4){ halfX,      0, 0, 0 },
        (float4){ 0,     -halfY, 0, 0 },
        (float4){ 0,          0, 1, 0 },
        (float4){ halfX,  halfY, 0, 1 },
    };
    pixelToClipTfm = inverse(pixelToClipTfm);
    
    cursor = pixelToClipTfm * cursor;
    
    //float4 clipPoint;
    //clipPoint.x = (point.x - halfX) / halfX;
    //clipPoint.y = -(point.y - halfY) / halfY;

    // convert point in window to point in texture
    float4x4 mInv = inverse(projectionViewModelMatrix);
    
    float4 pixel = mInv * float4m(cursor.x, cursor.y, 1.0f, 1.0f);
    pixel.xyz /= pixel.w; // in case perspective used

    float ar = _showSettings->imageAspectRatio();
    
    // that's in model space (+/0.5f * ar, +/0.5f), so convert to texture space
    pixel.x = (pixel.x / ar + 0.5f);
    pixel.y = (-pixel.y + 0.5f);

    //pixel.x *= 0.999f;
    //pixel.y *= 0.999f;
    
    float2 uv = pixel.xy;

    // pixels are 0 based
    pixel.x *= _showSettings->imageBoundsX;
    pixel.y *= _showSettings->imageBoundsY;

    // TODO: finish this logic, need to account for gaps too, and then isolate to
    // a given level and mip to sample
    //    if (_showSettings->isShowingAllLevelsAndMips) {
    //        pixel.x *= _showSettings->totalChunks();
    //        pixel.y *= _showSettings->mipCount;
    //    }

    // TODO: clearing out the last px visited makes it hard to gather data
    // put value on clipboard, or allow click to lock the displayed pixel and
    // value. Might just change header to px(last): ...
    string text;

    bool outsideImageBounds =
        pixel.x < 0.0f || pixel.x >= (float)_showSettings->imageBoundsX ||
        pixel.y < 0.0f || pixel.y >= (float)_showSettings->imageBoundsY;
    
    // only display pixel if over image
    if (outsideImageBounds) {
        sprintf(text, "canvas: %d %d\n", (int32_t)pixel.x, (int32_t)pixel.y);
        setEyedropperText(text.c_str());  // ick
        _showSettings->outsideImageBounds = true;
    }
    else {
        // Note: fromView: nil returns isFlipped coordinate, fromView:self flips it
        // back.
        
        int32_t newX = (int32_t)pixel.x;
        int32_t newY = (int32_t)pixel.y;
        
        if (_showSettings->outsideImageBounds ||
            (_showSettings->textureLookupX != newX ||
             _showSettings->textureLookupY != newY)) {
            // Note: this only samples from the original texture via compute shaders
            // so preview mode pixel colors are not conveyed.  But can see underlying
            // data driving preview.
            
            _showSettings->outsideImageBounds = false;
            
            // %.0f rounds the value, but want truncation
            _showSettings->textureLookupX = newX;
            _showSettings->textureLookupY = newY;
            
            // show block num
            int mipLOD = _showSettings->mipNumber;

            int mipX = _showSettings->imageBoundsX;
            int mipY = _showSettings->imageBoundsY;

            mipX = mipX >> mipLOD;
            mipY = mipY >> mipLOD;

            mipX = std::max(1, mipX);
            mipY = std::max(1, mipY);

            mipX = (int32_t)(uv.x * mipX);
            mipY = (int32_t)(uv.y * mipY);

            // Has to be set in other call, not here
            _showSettings->textureLookupMipX = mipX;
            _showSettings->textureLookupMipY = mipY;
            
            // showEyedropperData(uv);
        }
    }
}


bool Data::handleEventAction(const Action* action, bool isShiftKeyDown, ActionState& actionState)
{
    // Some data depends on the texture data (isSigned, isNormal, ..)
    bool isChanged = false;
    bool isStateChanged = false;
    
    // TODO: fix isChanged to only be set when value changes
    // f.e. clamped values don't need to re-render
    string text;
    
    if (action == _actionVertical) {
        _showSettings->isVerticalUI = !_showSettings->isVerticalUI;
        text = _showSettings->isVerticalUI ? "Vert UI" : "Horiz UI";
        
        // just to update toggle state to Off
        isStateChanged = true;
    }
    else if (action == _actionHideUI) {
        // this means no image loaded yet
        if (_noImageLoaded) {
            return true;
        }
        
        _showSettings->isHideUI = !_showSettings->isHideUI;
        text = _showSettings->isHideUI ? "Hide UI" : "Show UI";
        
        // just to update toggle state to Off
        isStateChanged = true;
    }
    
    else if (action == _actionR) {
        if (!action->isHidden) {
            TextureChannels& channels = _showSettings->channels;
            
            if (channels == TextureChannels::ModeR001) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = TextureChannels::ModeR001;
                text = "Mask R001";
            }
            isChanged = true;
        }
        
    }
    else if (action == _actionG) {
        if (!action->isHidden) {
            TextureChannels& channels = _showSettings->channels;
            
            if (channels == TextureChannels::Mode0G01) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = TextureChannels::Mode0G01;
                text = "Mask 0G01";
            }
            isChanged = true;
        }
    }
    else if (action == _actionB) {
        if (!action->isHidden) {
            TextureChannels& channels = _showSettings->channels;
            
            if (channels == TextureChannels::Mode00B1) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = TextureChannels::Mode00B1;
                text = "Mask 00B1";
            }
            
            isChanged = true;
        }
    }
    else if (action == _actionA) {
        if (!action->isHidden) {
            TextureChannels& channels = _showSettings->channels;
            
            if (channels == TextureChannels::ModeAAA1) {
                channels = TextureChannels::ModeRGBA;
                text = "Mask RGBA";
            }
            else {
                channels = TextureChannels::ModeAAA1;
                text = "Mask AAA1";
            }
            
            isChanged = true;
        }
        
    }
    else if (action == _actionPlay) {
        if (!action->isHidden) {
            
            _showSettings->isPlayAnimations = ! _showSettings->isPlayAnimations;
            
            //Renderer* renderer = (Renderer*)self.delegate;
            //renderer.playAnimations = !renderer.playAnimations;
            
            text = _showSettings->isPlayAnimations ? "Play" : "Pause";
            isChanged = true;
        }
    }
    else if (action == _actionShapeUVPreview) {
        
        // toggle state
        _showSettings->isUVPreview = !_showSettings->isUVPreview;
        text = _showSettings->isUVPreview ? "Show UVPreview" : "Hide UvPreview";
        isChanged = true;
        
        _showSettings->uvPreviewFrames = 10;
    }
    
    else if (action == _actionShapeChannel) {
        _showSettings->advanceShapeChannel(isShiftKeyDown);
        
        text = _showSettings->shapeChannelText();
        isChanged = true;
    }
    else if (action == _actionLighting) {
        _showSettings->advanceLightingMode(isShiftKeyDown);
        text = _showSettings->lightingModeText();
        isChanged = true;
    }
    else if (action == _actionTangent) {
        _showSettings->useTangent = !_showSettings->useTangent;
        if (_showSettings->useTangent)
            text = "Vertex Tangents";
        else
            text = "Fragment Tangents";
        isChanged = true;
    }
    else if (action == _actionDebug) {
        _showSettings->advanceDebugMode(isShiftKeyDown);
        text = _showSettings->debugModeText();
        isChanged = true;
    }
    else if (action == _actionHelp) {
        // display the chars for now
        text =
        "1234-rgba, Preview, Debug, A-show all\n"
        "Info, Hud, Reload, 0-fit\n"
        "Checker, Grid\n"
        "Wrap, 8-signed, 9-premul\n"
        "Mip, Face, Y-array\n"
        "↓-next item, →-next counterpart\n"
        "Lighting, S-shape, C-shape channel\n";
        
        // just to update toggle state to Off
        isStateChanged = true;
    }
    
    else if (action == _actionFit) {
        float zoom;
        // fit image or mip
        if (isShiftKeyDown) {
            zoom = 1.0f;
        }
        else {
            // fit to topmost image
            zoom = _showSettings->zoomFit;
        }
        
        // This zoom needs to be checked against zoom limits
        // there's a cap on the zoom multiplier.
        // This is reducing zoom which expands the image.
        zoom *= 1.0f / (1 << _showSettings->mipNumber);
        
        // even if zoom same, still do this since it resets the pan
        _showSettings->zoom = zoom;
        
        _showSettings->panX = 0.0f;
        _showSettings->panY = 0.0f;
        
        text = "Scale Image\n";
        if (doPrintPanZoom) {
            string tmp;
            sprintf(tmp,
                    "Pan %.3f,%.3f\n"
                    "Zoom %.2fx\n",
                    _showSettings->panX, _showSettings->panY, _showSettings->zoom);
            text += tmp;
        }
        
        isChanged = true;
    }
    // reload key (also a quick way to reset the settings)
    else if (action == _actionReload) {
        //bool success =
        _delegate.loadFile();
        
        // reload at actual size
        if (isShiftKeyDown) {
            _showSettings->zoom = 1.0f;
        }
        
        // Name change if image
        if (_showSettings->isModel)
            text = "Reload Model\n";
        else
            text = "Reload Image\n";
        if (doPrintPanZoom) {
            string tmp;
            sprintf(tmp,
                    "Pan %.3f,%.3f\n"
                    "Zoom %.2fx\n",
                    _showSettings->panX, _showSettings->panY, _showSettings->zoom);
            text += tmp;
        }
        
        isChanged = true;
    }
    else if (action == _actionPreview) {
        _showSettings->isPreview = !_showSettings->isPreview;
        isChanged = true;
        text = "Preview ";
        text += _showSettings->isPreview ? "On" : "Off";
    }
    else if (action == _actionDiff) {
        _showSettings->isDiff = !_showSettings->isDiff;
        isChanged = true;
        text = "Diff ";
        text += _showSettings->isPreview ? "On" : "Off";
    }
    // TODO: might switch c to channel cycle, so could just hit that
    // and depending on the content, it cycles through reasonable channel masks
    
    // toggle checkerboard for transparency
    else if (action == _actionChecker) {
        if (action->isHidden) {
            _showSettings->isCheckerboardShown = !_showSettings->isCheckerboardShown;
            isChanged = true;
            text = "Checker ";
            text += _showSettings->isCheckerboardShown ? "On" : "Off";
        }
    }
    
    // toggle pixel grid when magnified above 1 pixel, can happen from mipmap
    // changes too
    else if (action == _actionGrid) {
        static int grid = 0;
        static const int kNumGrids = 7;
        
#define advanceGrid(g, dec) \
grid = (grid + kNumGrids + (dec ? -1 : 1)) % kNumGrids
        
        // if block size is 1, then this shouldn't toggle
        _showSettings->isBlockGridShown = false;
        _showSettings->isAtlasGridShown = false;
        _showSettings->isPixelGridShown = false;
        
        advanceGrid(grid, isShiftKeyDown);
        
        static const uint32_t gridSizes[kNumGrids] = {
            0, 1, 4, 32, 64, 128, 256  // grid sizes
        };
        
        if (grid == 0) {
            sprintf(text, "Grid Off");
        }
        else if (grid == 1) {
            _showSettings->isPixelGridShown = true;
            
            sprintf(text, "Pixel Grid 1x1");
        }
        else if (grid == 2 && _showSettings->blockX > 1) {
            _showSettings->isBlockGridShown = true;
            
            sprintf(text, "Block Grid %dx%d", _showSettings->blockX,
                    _showSettings->blockY);
        }
        else {
            _showSettings->isAtlasGridShown = true;
            
            // want to be able to show altases tht have long entries derived from
            // props but right now just a square grid atlas
            _showSettings->gridSizeX = _showSettings->gridSizeY = gridSizes[grid];
            
            sprintf(text, "Atlas Grid %dx%d", _showSettings->gridSizeX,
                    _showSettings->gridSizeY);
        }
        
        isChanged = true;
    }
    else if (action == _actionShowAll) {
        if (!action->isHidden) {
            // TODO: have drawAllMips, drawAllLevels, drawAllLevelsAndMips
            _showSettings->isShowingAllLevelsAndMips =
            !_showSettings->isShowingAllLevelsAndMips;
            isChanged = true;
            text = "Show All ";
            text += _showSettings->isShowingAllLevelsAndMips ? "On" : "Off";
        }
    }
    
    // toggle hud that shows name and pixel value under the cursor
    // this may require calling setNeedsDisplay on the UILabel as cursor moves
    else if (action == _actionHud) {
        _showSettings->isHudShown = !_showSettings->isHudShown;
        //[self updateHudVisibility];
        // isChanged = true;
        text = "Hud ";
        text += _showSettings->isHudShown ? "On" : "Off";
        isStateChanged = true;
    }
    
    // info on the texture, could request info from lib, but would want to cache
    // that info
    else if (action == _actionInfo) {
        if (_showSettings->isHudShown) {
            
            // also hide the file table, since this can be long
            //[self hideFileTable];
            
            sprintf(text, "%s",
                    isShiftKeyDown ? _showSettings->imageInfoVerbose.c_str()
                    : _showSettings->imageInfo.c_str());
        }
        // just to update toggle state to Off
        isStateChanged = true;
    }
    
    // toggle wrap/clamp
    else if (action == _actionWrap) {
        // TODO: cycle through all possible modes (clamp, repeat, mirror-once,
        // mirror-repeat, ...)
        _showSettings->isWrap = !_showSettings->isWrap;
        isChanged = true;
        text = "Wrap ";
        text += _showSettings->isWrap ? "On" : "Off";
    }
    
    // toggle signed vs. unsigned
    else if (action == _actionSigned) {
        if (!action->isHidden) {
            _showSettings->isSigned = !_showSettings->isSigned;
            isChanged = true;
            text = "Signed ";
            text += _showSettings->isSigned ? "On" : "Off";
        }
    }
    
    // toggle premul alpha vs. unmul
    else if (action == _actionPremul) {
        if (!action->isHidden) {
            _showSettings->doShaderPremul = !_showSettings->doShaderPremul;
            isChanged = true;
            text = "Premul ";
            text += _showSettings->doShaderPremul ? "On" : "Off";
        }
    }
    
    else if (action == _actionItem || action == _actionPrevItem) {
        if (!action->isHidden) {
            // invert shift key for prev, since it's reverse
            if (action == _actionPrevItem) {
                isShiftKeyDown = !isShiftKeyDown;
            }
            
            if (advanceFile(!isShiftKeyDown)) {
                //_hudHidden = true;
                //[self updateHudVisibility];
                //[self setEyedropperText:""];
                
                isChanged = true;
            
                setLoadedText(text);
            }
        }
    }
    
    else if (action == _actionCounterpart || action == _actionPrevCounterpart) {
        if (!action->isHidden) {
            // invert shift key for prev, since it's reverse
            if (action == _actionPrevCounterpart) {
                isShiftKeyDown = !isShiftKeyDown;
            }
            if (advanceCounterpart(!isShiftKeyDown)) {
                //_hudHidden = true;
                //[self updateHudVisibility];
                //[self setEyedropperText:""];
                
                isChanged = true;
                
                setLoadedText(text);
            }
        }
    }
    
    // test out different shapes
    else if (action == _actionShapeMesh) {
        if (_showSettings->meshCount > 1) {
            _showSettings->advanceMeshNumber(isShiftKeyDown);
            text = _showSettings->meshNumberText();
            isChanged = true;
        }
    }
    
    // TODO: should probably have these wrap and not clamp to count limits
    
    // mip up/down
    else if (action == _actionMip) {
        if (_showSettings->mipCount > 1) {
            if (isShiftKeyDown) {
                _showSettings->mipNumber = std::max(_showSettings->mipNumber - 1, 0);
            }
            else {
                _showSettings->mipNumber =
                std::min(_showSettings->mipNumber + 1, _showSettings->mipCount - 1);
            }
            sprintf(text, "Mip %d/%d", _showSettings->mipNumber,
                    _showSettings->mipCount);
            isChanged = true;
        }
    }
    
    else if (action == _actionFace) {
        // cube or cube array, but hit s to pick cubearray
        if (_showSettings->faceCount > 1) {
            if (isShiftKeyDown) {
                _showSettings->faceNumber = std::max(_showSettings->faceNumber - 1, 0);
            }
            else {
                _showSettings->faceNumber =
                std::min(_showSettings->faceNumber + 1, _showSettings->faceCount - 1);
            }
            sprintf(text, "Face %d/%d", _showSettings->faceNumber,
                    _showSettings->faceCount);
            isChanged = true;
        }
    }
    
    else if (action == _actionArray) {
        // slice
        if (_showSettings->sliceCount > 1) {
            if (isShiftKeyDown) {
                _showSettings->sliceNumber = std::max(_showSettings->sliceNumber - 1, 0);
            }
            else {
                _showSettings->sliceNumber =
                std::min(_showSettings->sliceNumber + 1, _showSettings->sliceCount - 1);
            }
            sprintf(text, "Slice %d/%d", _showSettings->sliceNumber,
                    _showSettings->sliceCount);
            isChanged = true;
        }
        // array
        else if (_showSettings->arrayCount > 1) {
            if (isShiftKeyDown) {
                _showSettings->arrayNumber = std::max(_showSettings->arrayNumber - 1, 0);
            }
            else {
                _showSettings->arrayNumber =
                std::min(_showSettings->arrayNumber + 1, _showSettings->arrayCount - 1);
            }
            sprintf(text, "Array %d/%d", _showSettings->arrayNumber,
                    _showSettings->arrayCount);
            isChanged = true;
        }
    }
    else {
        // non-handled action
        return false;
    }
    
    actionState.hudText = text;
    actionState.isChanged = isChanged;
    actionState.isStateChanged = isStateChanged;
    
    return true;
}

// only called on new or modstamp-changed image
void Data::updateImageSettings(const string& fullFilename, KTXImage& image, MyMTLPixelFormat format)
{
    _showSettings->isModel = false;

    // format may be trancoded to gpu-friendly format
    MyMTLPixelFormat originalFormat = image.pixelFormat;

    _showSettings->blockX = image.blockDims().x;
    _showSettings->blockY = image.blockDims().y;

    _showSettings->isSigned = isSignedFormat(format);
    
    TexContentType texContentType = findContentTypeFromFilename(fullFilename.c_str());
    _showSettings->texContentType = texContentType;
    //_showSettings->isSDF = isSDF;

    // textures are already premul, so don't need to premul in shader
    // should really have 3 modes, unmul, default, premul
    bool isPNG = isPNGFilename(fullFilename.c_str());

    _showSettings->isPremul = image.isPremul();
    _showSettings->doShaderPremul = false;
    if (texContentType == TexContentTypeAlbedo && isPNG) {
        _showSettings->doShaderPremul =
            true;  // convert to premul in shader, so can see other channels
    }

    int32_t numChannels = numChannelsOfFormat(originalFormat);
    _showSettings->numChannels = numChannels;

    // TODO: identify if texture holds normal data from the props
    // have too many 4 channel normals that shouldn't swizzle like this
    // kramTextures.py is using etc2rg on iOS for now, and not astc.

    _showSettings->isSwizzleAGToRG = false;

    // For best sdf and normal reconstruct from ASTC or BC3, must use RRR1 and
    // GGGR or RRRG BC1nm multiply r*a in the shader, but just use BC5 anymore.
    //    if (isASTCFormat(originalFormat) && isNormal) {
    //        // channels after = "ag01"
    //        _showSettings->isSwizzleAGToRG = true;
    //    }

    // can derive these from texture queries
    _showSettings->mipCount = (int32_t)image.mipLevels.size();
    _showSettings->faceCount = (image.textureType == MyMTLTextureTypeCube ||
                                image.textureType == MyMTLTextureTypeCubeArray)
                                   ? 6
                                   : 0;
    _showSettings->arrayCount = (int32_t)image.header.numberOfArrayElements;
    _showSettings->sliceCount = (int32_t)image.depth;

    _showSettings->imageBoundsX = (int32_t)image.width;
    _showSettings->imageBoundsY = (int32_t)image.height;
}




float zoom3D = 1.0f;

void Data::updateProjTransform()
{
    // Want to move to always using perspective even for 2d images, but still more math
    // to work out to keep zoom to cursor working.
#if USE_PERSPECTIVE
    float aspect = _showSettings->viewSizeX /  (float)_showSettings->viewSizeY;
    _projectionMatrix = perspective_rhs(90.0f * (M_PI / 180.0f), aspect, 0.1f, 100000.0f, _showSettings->isReverseZ);

    // This was used to reset zoom to a baseline that had a nice zoom.  But little connected to it now.
    // Remember with rotation, the bounds can hit the nearClip.  Note all shapes are 0.5 radius,
    // so at 1 this is 2x to leave gap around the shape for now.
    float shapeHeightInY = 1;
    _showSettings->zoomFit = shapeHeightInY; // / (float)_showSettings->viewSizeY;

#else

    if (_showSettings->isModel) {
        float aspect = _showSettings->viewSizeX /  (float)_showSettings->viewSizeY;
        _projectionMatrix = perspective_rhs(90.0f * (M_PI / 180.0f), aspect, 0.1f, 100000.0f, _showSettings->isReverseZ);

        _showSettings->zoomFit = 1;
    }
    else {
        _projectionMatrix =
            orthographic_rhs(_showSettings->viewSizeX, _showSettings->viewSizeY, 0.1f,
                             100000.0f, _showSettings->isReverseZ);

        // DONE: adjust zoom to fit the entire image to the window
        _showSettings->zoomFit =
            std::min((float)_showSettings->viewSizeX, (float)_showSettings->viewSizeY) /
            std::max(1.0f, std::max((float)_showSettings->imageBoundsX,
                                    (float)_showSettings->imageBoundsY));
        
        static bool useImageAndViewBounds = true;
        if (useImageAndViewBounds) {
            float invWidth = 1.0f / std::max(1.0f, (float)_showSettings->imageBoundsX);
            float invHeight = 1.0f / std::max(1.0f, (float)_showSettings->imageBoundsY);

            // DONE: adjust zoom to fit the entire image to the window
            // the best fit depends on dimension of image and window
            _showSettings->zoomFit =
                std::min( (float)_showSettings->viewSizeX * invWidth,
                          (float)_showSettings->viewSizeY * invHeight);
        }
    }
#endif
}

void Data::resetSomeImageSettings(bool isNewFile)
{
    // only reset these on new texture, but have to revalidate
    if (isNewFile) {
        // then can manipulate this after loading
        _showSettings->mipNumber = 0;
        _showSettings->faceNumber = 0;
        _showSettings->arrayNumber = 0;
        _showSettings->sliceNumber = 0;
        
        _showSettings->channels = TextureChannels::ModeRGBA;
        
        // wish could keep existing setting, but new texture might not
        // be supported debugMode for new texture
        _showSettings->debugMode = DebugMode::DebugModeNone;
        
        _showSettings->shapeChannel = ShapeChannel::ShapeChannelNone;
    }
    else {
        // reloaded file may have different limits
        _showSettings->mipNumber =
        std::min(_showSettings->mipNumber, _showSettings->mipCount);
        _showSettings->faceNumber =
        std::min(_showSettings->faceNumber, _showSettings->faceCount);
        _showSettings->arrayNumber =
        std::min(_showSettings->arrayNumber, _showSettings->arrayCount);
        _showSettings->sliceNumber =
        std::min(_showSettings->sliceNumber, _showSettings->sliceCount);
    }
    
    updateProjTransform();
    
    
    // this controls viewMatrix (global to all visible textures)
    _showSettings->panX = 0.0f;
    _showSettings->panY = 0.0f;
    
    _showSettings->zoom = _showSettings->zoomFit;
    
    // Y is always 1.0 on the plane, so scale to imageBoundsY
    // plane is already a non-uniform size, so can keep uniform scale
    
    // have one of these for each texture added to the viewer
    //float scaleX = MAX(1, _showSettings->imageBoundsX);
    float scaleY = std::max(1, _showSettings->imageBoundsY);
    float scaleX = scaleY;
    float scaleZ = scaleY;
    
    _modelMatrix2D =
    float4x4(float4m(scaleX, scaleY, scaleZ, 1.0f)); // uniform scale
    _modelMatrix2D = _modelMatrix2D *
    matrix4x4_translation(0.0f, 0.0f, -1.0);  // set z=-1 unit back
    
    // uniform scaled 3d primitive
    float scale = scaleY; // MAX(scaleX, scaleY);
    
    // store the zoom into thew view matrix
    // fragment tangents seem to break down at high model scale due to precision
    // differences between worldPos and uv
    //    static bool useZoom3D = false;
    //    if (useZoom3D) {
    //        zoom3D = scale;  // * _showSettings->viewSizeX / 2.0f;
    //        scale = 1.0;
    //    }
    
    _modelMatrix3D = float4x4(float4m(scale, scale, scale, 1.0f));  // uniform scale
    _modelMatrix3D =
    _modelMatrix3D *
    matrix4x4_translation(0.0f, 0.0f, -1.0f);  // set z=-1 unit back
}

void Data::updateTransforms()
{
    // scale
    float zoom = _showSettings->zoom;
    
    // translate
    float4x4 panTransform =
        matrix4x4_translation(-_showSettings->panX, _showSettings->panY, 0.0);

    if (_showSettings->is3DView) {
        _viewMatrix3D = float4x4(float4m(zoom, zoom, 1.0f, 1.0f));  // non-uniform
        _viewMatrix3D = panTransform * _viewMatrix3D;
        
        _viewMatrix = _viewMatrix3D;
        
        // obj specific
        _modelMatrix = _modelMatrix3D;
    }
    else {
        _viewMatrix2D = float4x4(float4m(zoom, zoom, 1.0f, 1.0f));
        _viewMatrix2D = panTransform * _viewMatrix2D;
        
        _viewMatrix = _viewMatrix2D;
        
        // obj specific
        _modelMatrix = _modelMatrix2D;
    }
    
    // viewMatrix should typically be the inverse
    //_viewMatrix = simd_inverse(_viewMatrix);
    
    _projectionViewMatrix = _projectionMatrix * _viewMatrix;
    
    // cache the camera position
    _cameraPosition =
        inverse(_viewMatrix).columns[3].xyz;  // this is all ortho
    
    // obj specific
    _modelMatrixInvScale2 = inverseScaleSquared(_modelMatrix);
    _showSettings->isInverted = _modelMatrixInvScale2.w < 0.0f;
}

float4x4 Data::computeImageTransform(float panX, float panY, float zoom)
{
    // translate
    float4x4 panTransform = matrix4x4_translation(-panX, panY, 0.0);

    // non-uniform scale is okay here, only affects ortho volume
    // setting this to uniform zoom and object is not visible, zoom can be 20x in
    // x and y
    if (_showSettings->is3DView) {
        zoom *= zoom3D;
    }

    float4x4 viewMatrix = float4x4(float4m(zoom, zoom, 1.0f, 1.0f));
    viewMatrix = panTransform * viewMatrix;

    // scale
    if (_showSettings->is3DView) {
        return _projectionMatrix * viewMatrix * _modelMatrix3D;
    }
    else {
        return _projectionMatrix * viewMatrix * _modelMatrix2D;
    }
}


void Data::doZoomMath(float newZoom, float2& newPan)
{
    // transform the cursor to texture coordinate, or clamped version if outside
    float4x4 projectionViewModelMatrix = computeImageTransform(
                                    _showSettings->panX,
                                    _showSettings->panY,
                                    _showSettings->zoom);

    // convert from pixel to clip space
    float halfX = _showSettings->viewSizeX * 0.5f;
    float halfY = _showSettings->viewSizeY * 0.5f;
    
    // sometimes get viewSizeX that's scaled by retina, and other times not.
    // account for contentScaleFactor (viewSizeX is 2x bigger than cursorX on
    // retina display) now passing down drawableSize instead of view.bounds.size
    halfX /= (float)_showSettings->viewContentScaleFactor;
    halfY /= (float)_showSettings->viewContentScaleFactor;
    
    float4x4 viewportMatrix =
    {
        (float4){ halfX,      0, 0, 0 },
        (float4){ 0,     -halfY, 0, 0 },
        (float4){ 0,          0, 1, 0 },
        (float4){ halfX,  halfY, 0, 1 },
    };
    viewportMatrix = inverse(viewportMatrix);
    
    float4 cursor = float4m(_showSettings->cursorX, _showSettings->cursorY, 0.0f, 1.0f);
    
    cursor = viewportMatrix * cursor;
    
    //NSPoint clipPoint;
    //clipPoint.x = (point.x - halfX) / halfX;
    //clipPoint.y = -(point.y - halfY) / halfY;

    // convert point in window to point in model space
    float4x4 mInv = inverse(projectionViewModelMatrix);
    
    float4 pixel = mInv * float4m(cursor.x, cursor.y, 1.0f, 1.0f);
    pixel.xyz /= pixel.w; // in case perspective used

    // allow pan to extend to show all
    float ar = _showSettings->imageAspectRatio();
    float maxX = 0.5f * ar;
    float minY = -0.5f;
    if (_showSettings->isShowingAllLevelsAndMips) {
        maxX += ar * 1.0f * (_showSettings->totalChunks() - 1);
        minY -= 1.0f * (_showSettings->mipCount - 1);
    }

    // X bound may need adjusted for ar ?
    // that's in model space (+/0.5f, +/0.5f), so convert to texture space
    pixel.x = std::clamp(pixel.x, -0.5f * ar, maxX);
    pixel.y = std::clamp(pixel.y, minY, 0.5f);

    // now that's the point that we want to zoom towards
    // No checks on this zoom
    // old - newPosition from the zoom

    // normalized coords to pixel coords
    pixel.x *= _showSettings->imageBoundsX;
    pixel.y *= _showSettings->imageBoundsY;
    
    // this fixes pinch-zoom on cube which are 6:1
    pixel.x /= ar;
    
#if USE_PERSPECTIVE
    // TODO: this doesn't work for perspective
    newPan.x = _showSettings->panX - (_showSettings->zoom - newZoom) * pixel.x;
    newPan.y = _showSettings->panY + (_showSettings->zoom - newZoom) * pixel.y;
#else
    newPan.x = _showSettings->panX - (_showSettings->zoom - newZoom) * pixel.x;
    newPan.y = _showSettings->panY + (_showSettings->zoom - newZoom) * pixel.y;
#endif
}



}  // namespace kram
