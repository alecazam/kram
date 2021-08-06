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

#if USE_EASTL
#include "EASTL/string.h"
#else
#include <string>
#endif

#import "KramShaders.h" // for TextureChannels

namespace kram {
    class ShowSettings;
    class KTXImage;
}

// Our platform independent renderer class.   Implements the MTKViewDelegate protocol which
//   allows it to accept per-frame update and drawable resize callbacks.
@interface Renderer : NSObject <MTKViewDelegate>

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view settings:(nonnull kram::ShowSettings*)settings;

- (BOOL)loadTextureFromImage:(nonnull const char*)fullFilenameString
                   timestamp:(double)timestamp
                       image:(kram::KTXImage&)image
                 imageNormal:(nullable kram::KTXImage*)imageNormal
                   isArchive:(BOOL)isArchive;


- (BOOL)loadTexture:(nonnull NSURL *)url;

- (simd::float4x4)computeImageTransform:(float)panX panY:(float)panY zoom:(float)zoom;

- (BOOL)hotloadShaders:(nonnull const char*)filename;

@end


