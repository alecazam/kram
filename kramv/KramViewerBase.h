// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#include <cstdint>

#include "KramLib.h"  // for MyMTLPixelFormat
//#include <string>
//#include <simd/simd.h>

// All portable C++ code.

namespace kram {

using namespace NAMESPACE_STL;
using namespace SIMD_NAMESPACE;

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

struct Atlas
{
    string name;
    float x,y,w,h;
    float u,v; // padding - to both or just left or right?
    bool isVertical;
    uint32_t level;
    
    float4 rect() const { return float4m(x,y,w,h); }
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
    
    bool isHideUI = false;
    bool isVerticalUI = true;
    
    bool isPlayAnimations = false;
    
    // Can get a dump of perf (mostly loading a decode/transcode perf)
    bool isPerf = false;
    
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

    //bool isNormal = false;
    bool isSigned = false;
    bool isPremul = false; // copy of whether image.isPremul()
    bool doShaderPremul = false; // needed for png which only holds unmul
    bool isSwizzleAGToRG = false;
    //bool isSDF = false;
    TexContentType texContentType = TexContentTypeUnknown;
    
    // this mode shows the content with lighting or with bilinear/mips active
    bool isPreview = false;

    // Can collapse 3d to 2d and overlay the uv
    bool isUVPreview = false;
    
    uint32_t uvPreviewFrames = 0;
    float uvPreviewStep = 1.0f / 10.0f;
    float uvPreview = 0.0f;
    
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
    bool isSRGBShown = false;

    // whether to use normal to tangent (false), or vertex tangents (true)
    bool useTangent = true;

    // draw with reverseZ to better match perspective
    bool isReverseZ = true;

    // image vs. gltf model
    bool isModel = false;
    
    // if diff texture available, can show diff against source
    bool isDiff = false;
    
    // currently loading the diff texture if found, this slows loads
    bool hasDiffTexture = false;
    
    // can sample from drawable or from single source texture
    bool isEyedropperFromDrawable();

    // can have up to 5 channels (xyz as xy, 2 other channels)
    int32_t numChannels = 0;

    // this could be boundary of all visible images, so that pan doesn't go flying
    // off to nowhere
    int32_t imageBoundsX = 0;  // px
    int32_t imageBoundsY = 0;  // px

    bool outsideImageBounds = false;
    
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

    bool isInverted = false;

    // cached on load, raw info about the texture from libkram
    string imageInfo;
    string imageInfoVerbose;

    // format before any transcode to supported formats
    MyMTLPixelFormat originalFormat;
    MyMTLPixelFormat decodedFormat;

    string windowTitleString(const char* filename) const;

    void advanceMeshNumber(bool decrement);
    void advanceDebugMode(bool decrement);
    void advanceShapeChannel(bool decrement);
    void advanceLightingMode(bool decrement);

    const char *meshNumberText() const;
    const char *shapeChannelText() const;
    const char *debugModeText() const;
    const char *lightingModeText() const;
    
    const char *meshNumberName(uint32_t meshNumber) const;
    
    void updateUVPreviewState();
    
    float imageAspectRatio() const {
        float ar = 1.0f;
        if (meshNumber == 0 && !isModel && imageBoundsY > 0)
            ar = imageBoundsX / (float)imageBoundsY;
        return ar;
    }
    
    bool isFileNew(const char* fullFilename) const {
        return lastFilename != fullFilename;
    }
    bool isFileChanged(const char* fullFilename, double timestamp) const {
        // Note that modstamp can change, but content data hash may be the same
        return isFileNew(fullFilename) || (timestamp != lastTimestamp);
    }
    
    string lastFilename;
    double lastTimestamp = 0.0;

    int32_t meshNumber = 0;
    int32_t meshCount = 5;
    
    const Atlas* lastAtlas = nullptr; // Might move to index
    vector<Atlas> atlas;
};

float4x4 matrix4x4_translation(float tx, float ty, float tz);

float4x4 perspective_rhs(float fovyRadians, float aspect, float nearZ, float
                         farZ, bool isReverseZ);

float4x4 orthographic_rhs(float width, float height, float nearZ, float farZ,
                          bool isReverseZ);

float4x4 matrix4x4_rotation(float radians, vector_float3 axis);

void printChannels(string &tmp, const string &label, float4 c,
                   int32_t numChannels, bool isFloat, bool isSigned);


enum Key {
    A = 0x00,
    S = 0x01,
    D = 0x02,
    F = 0x03,
    H = 0x04,
    G = 0x05,
    Z = 0x06,
    X = 0x07,
    C = 0x08,
    V = 0x09,
    B = 0x0B,
    Q = 0x0C,
    W = 0x0D,
    E = 0x0E,
    R = 0x0F,
    Y = 0x10,
    T = 0x11,
    O = 0x1F,
    U = 0x20,
    I = 0x22,
    P = 0x23,
    L = 0x25,
    J = 0x26,
    K = 0x28,
    N = 0x2D,
    M = 0x2E,

    // https://eastmanreference.com/complete-list-of-applescript-key-codes
    Num1 = 0x12,
    Num2 = 0x13,
    Num3 = 0x14,
    Num4 = 0x15,
    Num5 = 0x17,
    Num6 = 0x16,
    Num7 = 0x1A,
    Num8 = 0x1C,
    Num9 = 0x19,
    Num0 = 0x1D,

    LeftBrace = 0x21,
    RightBrace = 0x1E,

    LeftBracket = 0x21,
    RightBracket = 0x1E,

    Quote = 0x27,
    Semicolon = 0x29,
    Backslash = 0x2A,
    Comma = 0x2B,
    Slash = 0x2C,

    LeftArrow = 0x7B,
    RightArrow = 0x7C,
    DownArrow = 0x7D,
    UpArrow = 0x7E,
    
    Space = 0x31,
    Escape = 0x35,
};

using kram_id = void*;

// This makes dealing with ui much simpler
class Action {
public:
    Action(const char* icon_, const char* tip_, Key keyCode_)
        : icon(icon_), tip(tip_), keyCode(keyCode_) {}
    
    const char* icon;
    const char* tip;

    // Note these are not ref-counted, but AppKit already does
    kram_id button; // NSButton*
    kram_id menuItem; // NSMenuItem*
    Key keyCode;
    
    bool isHighlighted = false;
    bool isHidden = false;
    bool isButtonDisabled = false;
    
    // This have platform impl
    void setHighlight(bool enable);
    void setHidden(bool enable);
    void disableButton();
};

// This is an open archive
struct FileContainer {
    // allow zip files to be dropped and opened, and can advance through bundle
    // content.
    
    // TODO: Add FileHelper if acrhive file is networked, but would require
    // full load to memory.
    
    ZipHelper zip;
    MmapHelper zipMmap;
};

struct ActionState
{
    string hudText;
    bool isChanged;
    bool isStateChanged;
};

enum TextSlot
{
    kTextSlotHud,
    kTextSlotEyedropper,
    kTextSlotAtlas,
    
    kTextSlotCount // not a slot
};

struct File {
public:
    File(const char* name_, int32_t urlIndex_);
    
    // Note: not sorting by urlIndex currently
    bool operator <(const File& rhs) const
    {
        // sort by shortname
        int compare = strcasecmp(nameShort.c_str(), rhs.nameShort.c_str());
        if ( compare != 0 )
            return compare < 0;
        
        // if equal, then sort by longname
        return strcasecmp(name.c_str(), rhs.name.c_str()) < 0;
    }
    
public:
    string name;
    int32_t urlIndex;
    string nameShort; // would alias name, but too risky
};

// This allows wrapping all the ObjC stuff
struct DataDelegate
{
    bool loadFile(bool clear = false);
    
    bool loadModelFile(const char* filename);
   
    bool loadTextureFromImage(const char* fullFilename, double timestamp, KTXImage& image, KTXImage* imageNormal, KTXImage* imageDiff, bool isArchive);
    
public:
    kram_id view; // MyMTKView*
};

struct Data {
    Data();
    ~Data();
    
    void clearAtlas();
    bool loadAtlasFile(const char* filename);
    bool listFilesInArchive(int32_t urlIndex);
    bool openArchive(const char * zipFilename, int32_t urlIndex);

    bool hasCounterpart(bool increment);
    bool advanceCounterpart(bool increment);
    bool advanceFile(bool increment);

    bool findFilename(const string& filename);
    bool findFilenameShort(const string& filename);
    const File* findFileShort(const string& filename);
    const Atlas* findAtlasAtUV(float2 uv);
    bool isArchive() const;
    bool loadFile();
    
    bool handleEventAction(const Action* action, bool isShiftKeyDown, ActionState& actionState);
    void updateUIAfterLoad();
    void updateUIControlState();

    const Action* actionFromMenu(kram_id menuItem) const;
    const Action* actionFromButton(kram_id button) const;
    const Action* actionFromKey(uint32_t keyCodes) const;

    void setLoadedText(string& text);
    void setFailedText(const string& filename, string& text);
    
    void initActions();
    vector<Action>& actions() { return _actions; }
    void initDisabledButtons();

    string textFromSlots(bool isFileListHidden) const;
    void setTextSlot(TextSlot slot, const char* text);

    void loadFilesFromUrls(vector<string>& urls, bool skipSubdirs);
    void listArchivesInFolder(const string& archiveFilename, vector<File>& archiveFiles, bool skipSubdirs);
    void listFilesInFolder(const string& folderFilename, int32_t urlIndex, bool skipSubdirs);

    // See these to split off ObjC code
    DataDelegate _delegate;
    
    void updateEyedropper();
    
    float4x4 computeImageTransform(float panX, float panY, float zoom);
    void updateProjTransform();
    void resetSomeImageSettings(bool isNewFile);
    void updateImageSettings(const string& fullFilename, KTXImage& image, MyMTLPixelFormat format);

    void doZoomMath(float newZoom, float2& newPan);

    void setPerfDirectory(const char* directory);
    
private:
    bool loadFileFromArchive();

public:
    void showEyedropperData(const float2& uv);
    void setEyedropperText(const char * text);
    void setAtlasText(const char * text);
    void updateTransforms();
   
    //----------------
    float4x4 _projectionMatrix;
    
    float4x4 _projectionViewMatrix;
    float3 _cameraPosition;
    
    float4x4 _viewMatrix;
    float4x4 _viewMatrix2D;
    float4x4 _viewMatrix3D;

    // object specific
    float4x4 _modelMatrix;
    float4 _modelMatrixInvScale2;
    float4x4 _modelMatrix2D;
    float4x4 _modelMatrix3D;

    //----------------
    
    vector<string> _textSlots;
    ShowSettings* _showSettings = nullptr;

    bool _noImageLoaded = true;
    string _archiveName; // archive or blank
    
    // folders and archives and multi-drop files are filled into this
    vector<File> _files;
    int32_t _fileIndex = 0;
   
    // One of these per url in _urlss
    vector<FileContainer*> _containers;
    vector<string> _urls;
    
    Action* _actionPlay;
    Action* _actionShapeUVPreview;
    Action* _actionHelp;
    Action* _actionInfo;
    Action* _actionHud;
    Action* _actionShowAll;
    
    Action* _actionPreview;
    Action* _actionWrap;
    Action* _actionPremul;
    Action* _actionSigned;
    Action* _actionSrgb;
    Action* _actionPerf;
    
    Action* _actionDiff;
    Action* _actionDebug;
    Action* _actionGrid;
    Action* _actionChecker;
    Action* _actionHideUI;
    Action* _actionVertical;
    
    Action* _actionMip;
    Action* _actionFace;
    Action* _actionArray;
    Action* _actionItem;
    Action* _actionPrevItem;
    Action* _actionCounterpart;
    Action* _actionPrevCounterpart;
    Action* _actionReload;
    Action* _actionFit;
    
    Action* _actionShapeMesh;
    Action* _actionShapeChannel;
    Action* _actionLighting;
    Action* _actionTangent;
    
    Action* _actionR;
    Action* _actionG;
    Action* _actionB;
    Action* _actionA;
    
    vector<Action> _actions;
};

bool isSupportedModelFilename(const char* filename);
bool isSupportedArchiveFilename(const char* filename);
bool isSupportedJsonFilename(const char* filename);

//extern bool doPrintPanZoom;

}  // namespace kram
