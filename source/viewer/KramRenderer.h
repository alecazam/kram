// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#if __has_feature(modules)
@import Foundaton;
@import simd;
@import MetalKit;
#else
#import <MetalKit/MetalKit.h>
#import <Foundation/NSURL.h>
#import <simd/simd.h>
#endif

#include <string>

#import "KramShaders.h" // for TextureChannels

// Our platform independent renderer class.   Implements the MTKViewDelegate protocol which
//   allows it to accept per-frame update and drawable resize callbacks.
@interface Renderer : NSObject <MTKViewDelegate>

-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view;

- (BOOL)loadTexture:(nonnull NSURL *)url;

- (simd::float4x4)computeImageTransform:(float)panX panY:(float)panY zoom:(float)zoom;

@end

struct ShowSettings {
    // Can mask various channels (r/g/b/a only, vs. all), may also add toggle of channel
    TextureChannels channels;
    
    // These control which texture is viewed in single texture mode
    int mipLOD = 0;
    int maxLOD = 1;
    
    int faceNumber = 0;
    int faceCount = 0;
    
    int arrayNumber = 0;
    int arrayCount = 0;
    
    int sliceNumber = 0;
    int sliceCount = 0;
    
    // DONE: hook all these up to shader and view
    bool isHudShown = true;
   
    // transparency checkboard under the image
    bool isCheckerboardShown = false;
   
    // draw a 1x1 or blockSize grid, note ASTC has non-square grid sizes
    bool isPixelGridShown = false;
    bool isBlockGridShown = false;
   
    // expands uv from [0,1] to [0,2] in shader to see the repeat pattern
    bool isWrap = false;
    
    bool isNormal = false;
    bool isSigned = false;
    bool isPremul = false; // needed for png which only holds unmul
    bool isSwizzleAGToRG = false;
    bool isSDF = false;
    
    // this mode shows the content with lighting or with bilinear/mips active
    bool isPreview = false;
    
    // TODO: Might eliminate this, since mips are either built with or without srgb
    // and disabling with a MTLView caused many flags to have to be set on MTLTexture
    //bool isSRGBShown = true;
    
    // draw with reverseZ to better match perspective
    bool isReverseZ = true;
    
    // can have up to 5 channels (xyz as xy, 2 other channels)
    int numChannels = 0;
    
    // this could be boundary of all visible images, so that pan doesn't go flying off to nowhere
    int imageBoundsX = 0; // px
    int imageBoundsY = 0; // px
 
    // size of the block, uses in block grid drawing
    int blockX = 1;
    int blockY = 1;
    
    // for eyedropper, lookup this pixel value, and return it to CPU
    int textureLookupX = 0;
    int textureLookupY = 0;
    
    int textureResultX = 0;
    int textureResultY = 0;
    simd::float4 textureResult;
    
    // size of the view and its contentScaleFactor
    int viewSizeX = 1; // px
    int viewSizeY = 1; // px
    float viewContentScaleFactor = 1.0f;
    
    // cursor is in view coordinates, but doesn't include contentScaleFactor
    int cursorX = 0;
    int cursorY = 0;
    
    // these control the view transform, zoomFit fits the image vertically to he view bound
    float zoomFit = 1.0f;
    float zoom = 0.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    
    DebugMode debugMode = DebugModeNone;
    
    simd::float4x4 projectionViewModelMatrix;
    
    // cached on load, raw info about the texture from libkram
    std::string imageInfo;
    
    // format before any transcode to supported formats
    MTLPixelFormat originalFormat;
};

extern ShowSettings gShowSettings;
extern bool isReverseZ;


