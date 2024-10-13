// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

//@import Foundation;
//@import MetalKit;
#import <Foundation/NSURL.h>
#import <MetalKit/MetalKit.h>

#include "KramLib.h"
#import "KramShaders.h" // for TextureChannels

// Turn on GLTF loading support for 3d models.  This relies on Warren Moore's first GLTFKit
// which only handles import and synchronous loading.
#define USE_GLTF 1

// Could use ModelIO on macOS/iOS to support usd files.  Adding here for completeness.
// But would prefer cross-platform C++ solution.
#define USE_USD 0

// Only use a perspective transform for models/images, otherwise perspective only used for models
#define USE_PERSPECTIVE 0

#if USE_GLTF
#import "GLTF/GLTF.h"
#import "GLTFMTL/GLTFMTL.h"
//@import GLTF;
//@import GLTFMTL;
#endif

namespace kram {
class ShowSettings;
class Data;
class KTXImage;
} //namespace kram

// Need renderer to be able to call back up to view to update hud.
@protocol MyMTKViewUpdates <NSObject>
- (void)updateEyedropperText;
@end

// Our platform independent renderer class.   Implements the MTKViewDelegate
// protocol which
//   allows it to accept per-frame update and drawable resize callbacks.
#if USE_GLTF
@interface Renderer : NSObject <MTKViewDelegate, GLTFAssetLoadingDelegate>
#else
@interface Renderer : NSObject <MTKViewDelegate>
#endif

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view
                                    settings:
                                        (nonnull kram::ShowSettings *)settings
                                        data:(nonnull kram::Data *)data;

- (BOOL)loadTextureFromImage:(nonnull const char *)fullFilenameString
                   timestamp:(double)timestamp
                       image:(kram::KTXImage &)image
                 imageNormal:(nullable kram::KTXImage *)imageNormal
                   imageDiff:(nullable kram::KTXImage *)imageDiff
                   isArchive:(BOOL)isArchive;

- (BOOL)loadTexture:(nonnull NSURL *)url;

- (BOOL)hotloadShaders:(nonnull const char *)filename;

// unload textures and gltf model textures
- (void)releaseAllPendingTextures;

// load a gltf model
- (BOOL)loadModel:(nonnull const char *)url;

// unload gltf model
- (void)unloadModel;

// called from view and renderer in render loop
- (void)updateAnimationState:(nonnull MTKView *)view;

// So caller can respond to completed callback
- (void)setEyedropperDelegate:(nullable id)delegate;

// Can enable frame pacing for VRR
- (void)setFramePacingEnabled:(bool)enable;

// can play animations in gltf models
@property(nonatomic) BOOL playAnimations;

// can toggle on/off srgb if that is psosible
@property(nonatomic) BOOL isToggleView;

// true if a toggle is present
@property(nonatomic) BOOL hasToggleView;

@end
