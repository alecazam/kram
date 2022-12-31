// kram - Copyright 2020-2022 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import "KramRenderer.h"

//@import ModelIO;
#import <ModelIO/ModelIO.h>
#import <TargetConditionals.h>

// Include header shared between C code here, which executes Metal API commands,
// and .metal files
#import "KramLoader.h"
#import "KramShaders.h"
#include "KramViewerBase.h"

// c interface to signposts similar to dtrace on macOS/iOS
#include <os/signpost.h>
#include <mutex> // for recursive_mutex

using mymutex = std::recursive_mutex;
using mylock = std::unique_lock<mymutex>;

os_log_t gLogKramv = os_log_create("com.ba.kramv", "");

class Signpost
{
public:
    Signpost(const char* name)
        : _name(name), _ended(false)
    {
        if (os_signpost_enabled(gLogKramv)) // pretty much always true
            os_signpost_interval_begin(gLogKramv, OS_SIGNPOST_ID_EXCLUSIVE, "kram", "%s", _name);
        else
            _ended = true;
    }
    
    ~Signpost()
    {
        stop();
    }
    
    void stop()
    {
        if (!_ended) {
            os_signpost_interval_end(gLogKramv, OS_SIGNPOST_ID_EXCLUSIVE, "kram", "%s", _name);
            _ended = true;
        }
    }
    
private:
    const char* _name;
    bool _ended;
};


#if USE_GLTF

// TODO: make part of renderer
static mymutex gModelLock;

// patch this into GLTFRenderer, so can use kram to load ktx/2 and png files
// doesn't support jpg, hdr, or exr files.  Can't yet load ktx2 w/basis.

@interface KramGLTFTextureLoader : NSObject <IGLTFMTLTextureLoader>
- (instancetype)initWithLoader:(KramLoader*)loader;
- (id<MTLTexture> _Nullable)newTextureWithContentsOfURL:(NSURL *)url options:(NSDictionary * _Nullable)options error:(NSError **)error;
- (id<MTLTexture> _Nullable)newTextureWithData:(NSData *)data options:(NSDictionary * _Nullable)options error:(NSError **)error;
@end

@interface KramGLTFTextureLoader ()
@property (nonatomic, strong) KramLoader* loader;
@end

@implementation KramGLTFTextureLoader

- (instancetype)initWithLoader:(KramLoader*)loader
{
    if ((self = [super init])) {
        _loader = loader;
    }
    return self;
}

// TODO: this ignores options and error.  Default png loading may need to request srgb.
- (id<MTLTexture> _Nullable)newTextureWithContentsOfURL:(NSURL *)url options:(NSDictionary * _Nullable)options error:(NSError * __autoreleasing *)error
{
    return [_loader loadTextureFromURL:url originalFormat:nil];
}

// TODO: this ignores options and error.  Default png loading may need to request srgb.
- (id<MTLTexture> _Nullable)newTextureWithData:(NSData *)data options:(NSDictionary * _Nullable)options error:(NSError * __autoreleasing *)error
{
    return [_loader loadTextureFromData:data originalFormat:nil];
}

@end

#endif


static const NSUInteger MaxBuffersInFlight = 3;

using namespace kram;
using namespace simd;

// Capture what we need to build the renderPieplines, without needing view
struct ViewFramebufferData {
    MTLPixelFormat colorPixelFormat = MTLPixelFormatInvalid;
    MTLPixelFormat depthStencilPixelFormat = MTLPixelFormatInvalid;
    uint32_t sampleCount = 0;
};

@implementation Renderer {
    dispatch_semaphore_t _inFlightSemaphore;
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;

    id<MTLBuffer> _dynamicUniformBuffer[MaxBuffersInFlight];

    id<MTLRenderPipelineState> _pipelineState1DArray;
    id<MTLRenderPipelineState> _pipelineStateImage;
    id<MTLRenderPipelineState> _pipelineStateImageArray;
    id<MTLRenderPipelineState> _pipelineStateCube;
    id<MTLRenderPipelineState> _pipelineStateCubeArray;
    id<MTLRenderPipelineState> _pipelineStateVolume;

    id<MTLRenderPipelineState> _pipelineStateDrawLines;
    
    id<MTLComputePipelineState> _pipelineState1DArrayCS;
    id<MTLComputePipelineState> _pipelineStateImageCS;
    id<MTLComputePipelineState> _pipelineStateImageArrayCS;
    id<MTLComputePipelineState> _pipelineStateCubeCS;
    id<MTLComputePipelineState> _pipelineStateCubeArrayCS;
    id<MTLComputePipelineState> _pipelineStateVolumeCS;

    id<MTLDepthStencilState> _depthStateFull;
    id<MTLDepthStencilState> _depthStateNone;

    
    MTLVertexDescriptor* _mtlVertexDescriptor;

    // TODO: Array< id<MTLTexture> > _textures;
    id<MTLTexture> _colorMap;
    id<MTLTexture> _normalMap;
    id<MTLTexture> _lastDrawableTexture;

    // border is a better edge sample, but at edges it filters in the transparent
    // color around the border which is undesirable.  It would be better if the hw
    // did clamp to edge until uv outside 0 to 1.  This results in having to inset
    // the uv by 0.5 px to avoid this artifact, but on small texturs that are 4x4,
    // a 1 px inset is noticeable.

    id<MTLSamplerState> _colorMapSamplerNearestWrap;
    id<MTLSamplerState> _colorMapSamplerNearestBorder;
    id<MTLSamplerState> _colorMapSamplerNearestEdge;

    id<MTLSamplerState> _colorMapSamplerFilterWrap;
    id<MTLSamplerState> _colorMapSamplerFilterBorder;
    id<MTLSamplerState> _colorMapSamplerFilterEdge;

    // id<MTLTexture> _sampleRT;
    id<MTLTexture> _sampleComputeTex;
    id<MTLTexture> _sampleRenderTex;

    uint8_t _uniformBufferIndex;

   
    // float _rotation;
    KramLoader* _loader;
    MTKMesh* _mesh;

    MDLVertexDescriptor *_mdlVertexDescriptor;

    MTKMesh* _meshRect;
    MTKMesh* _meshBox;
    MTKMesh* _meshSphere;
    MTKMesh* _meshSphereMirrored;
    // MTKMesh* _meshCylinder;
    MTKMesh* _meshCapsule;
    MTKMeshBufferAllocator *_metalAllocator;

    id<MTLLibrary> _shaderLibrary;
    NSURL* _metallibFileURL;
    NSDate* _metallibFileDate;
    ViewFramebufferData _viewFramebuffer;

    ShowSettings* _showSettings;
    Data* _data;
    
#if USE_GLTF
    KramGLTFTextureLoader* _textureLoader;
    id<GLTFBufferAllocator> _bufferAllocator;
    GLTFMTLRenderer* _gltfRenderer;
    GLTFAsset* _asset; // only 1 for now
    double _animationTime;
    
    id<MTLTexture> _environmentTexture;
    bool _environmentNeedsUpdate;
    
    NSURLSession* _urlSession;
#endif

}

@synthesize playAnimations;

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view
                                    settings:(nonnull ShowSettings *)settings
                                    data:(nonnull Data*)data
{
    self = [super init];
    if (self) {
        _showSettings = settings;
        _data = data;
        _device = view.device;

        _loader = [KramLoader new];
        _loader.device = _device;

        _metalAllocator = [[MTKMeshBufferAllocator alloc] initWithDevice:_device];

        _inFlightSemaphore = dispatch_semaphore_create(MaxBuffersInFlight);
        [self _loadMetalWithView:view];
        [self _loadAssets];
        
#if USE_GLTF
        _bufferAllocator = [[GLTFMTLBufferAllocator alloc] initWithDevice:_device];
        _gltfRenderer = [[GLTFMTLRenderer alloc] initWithDevice:_device];
        
        // This aliases the existing kram loader, can handle png, ktx, ktx2
        _textureLoader = [[KramGLTFTextureLoader alloc] initWithLoader:_loader];
        _gltfRenderer.textureLoader = _textureLoader;
        
        // load the environment from a cube map for now
        // runs this after _shaderLibrary established above
        _gltfRenderer.lightingEnvironment = [[GLTFMTLLightingEnvironment alloc] initWithLibrary: _shaderLibrary];
        
        //NSURL* environmentURL = [[NSBundle mainBundle] URLForResource:@"piazza_san_marco" withExtension:@"ktx"];
        NSURL* environmentURL = [[NSBundle mainBundle] URLForResource:@"tropical_beach" withExtension:@"ktx"];
        _environmentTexture = [_loader loadTextureFromURL:environmentURL originalFormat:nil];
        _environmentNeedsUpdate = true;
#endif

    }

    return self;
}

- (void)_createSamplers
{
    MTLSamplerDescriptor *samplerDescriptor = [MTLSamplerDescriptor new];
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterNearest;

    samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.label = @"colorMapSamplerNearestWrap";

    _colorMapSamplerNearestWrap =
        [_device newSamplerStateWithDescriptor:samplerDescriptor];

    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.label = @"colorMapSamplerNearestBorder";

    _colorMapSamplerNearestBorder =
        [_device newSamplerStateWithDescriptor:samplerDescriptor];

    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.label = @"colorMapSamplerNearestEdge";

    _colorMapSamplerNearestEdge =
        [_device newSamplerStateWithDescriptor:samplerDescriptor];

    // -----

    // these are for preview mode
    // use the mips, and specify linear for min/mag for SDF case
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;
    samplerDescriptor.maxAnisotropy = 4;  // 1,2,4,8,16 are choices

    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.label = @"colorMapSamplerFilterBorder";

    _colorMapSamplerFilterBorder =
        [_device newSamplerStateWithDescriptor:samplerDescriptor];

    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.label = @"colorMapSamplerFilterEdge";

    _colorMapSamplerFilterEdge =
        [_device newSamplerStateWithDescriptor:samplerDescriptor];

    samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.label = @"colorMapSamplerBilinearWrap";

    _colorMapSamplerFilterWrap =
        [_device newSamplerStateWithDescriptor:samplerDescriptor];
}

- (void)_createVertexDescriptor
{
    _mtlVertexDescriptor = [[MTLVertexDescriptor alloc] init];

    _mtlVertexDescriptor.attributes[VertexAttributePosition].format =
        MTLVertexFormatFloat3;
    _mtlVertexDescriptor.attributes[VertexAttributePosition].offset = 0;
    _mtlVertexDescriptor.attributes[VertexAttributePosition].bufferIndex =
        BufferIndexMeshPosition;

    _mtlVertexDescriptor.attributes[VertexAttributeTexcoord].format =
        MTLVertexFormatFloat2;  // TODO: compress
    _mtlVertexDescriptor.attributes[VertexAttributeTexcoord].offset = 0;
    _mtlVertexDescriptor.attributes[VertexAttributeTexcoord].bufferIndex =
        BufferIndexMeshUV0;

    _mtlVertexDescriptor.attributes[VertexAttributeNormal].format =
        MTLVertexFormatFloat3;  // TODO: compress
    _mtlVertexDescriptor.attributes[VertexAttributeNormal].offset = 0;
    _mtlVertexDescriptor.attributes[VertexAttributeNormal].bufferIndex =
        BufferIndexMeshNormal;

    _mtlVertexDescriptor.attributes[VertexAttributeTangent].format =
        MTLVertexFormatFloat4;  // TODO: compress
    _mtlVertexDescriptor.attributes[VertexAttributeTangent].offset = 0;
    _mtlVertexDescriptor.attributes[VertexAttributeTangent].bufferIndex =
        BufferIndexMeshTangent;

    //_mtlVertexDescriptor.layouts[BufferIndexMeshPosition].stepRate = 1;
    //_mtlVertexDescriptor.layouts[BufferIndexMeshPosition].stepFunction =
    //MTLVertexStepFunctionPerVertex;

    _mtlVertexDescriptor.layouts[BufferIndexMeshPosition].stride = 3 * 4;
    _mtlVertexDescriptor.layouts[BufferIndexMeshUV0].stride = 2 * 4;
    _mtlVertexDescriptor.layouts[BufferIndexMeshNormal].stride = 3 * 4;
    _mtlVertexDescriptor.layouts[BufferIndexMeshTangent].stride = 4 * 4;

    //-----------------------
    // for ModelIO
    _mdlVertexDescriptor =
        MTKModelIOVertexDescriptorFromMetal(_mtlVertexDescriptor);

    _mdlVertexDescriptor.attributes[VertexAttributePosition].name =
        MDLVertexAttributePosition;
    _mdlVertexDescriptor.attributes[VertexAttributeTexcoord].name =
        MDLVertexAttributeTextureCoordinate;
    _mdlVertexDescriptor.attributes[VertexAttributeNormal].name =
        MDLVertexAttributeNormal;
    _mdlVertexDescriptor.attributes[VertexAttributeTangent].name =
        MDLVertexAttributeTangent;
}

- (void)_loadMetalWithView:(nonnull MTKView *)view
{
    /// Load Metal state objects and initialize renderer dependent view properties

    // Important to set color space, or colors are wrong.  Why doesn't one of these work (or the default)
    // false is good for srgb -> rgba16f
    // true is good for non-srgb -> rgba16f
    CGColorSpaceRef viewColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGBLinear);
    //bool pickOne = false;
    // pickOne ? kCGColorSpaceSRGB : kCGColorSpaceLinearSRGB);
    view.colorspace = viewColorSpace;
    
    view.colorPixelFormat = MTLPixelFormatRGBA16Float;
    view.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    view.sampleCount = 1;

    _viewFramebuffer.colorPixelFormat = view.colorPixelFormat;
    _viewFramebuffer.depthStencilPixelFormat = view.depthStencilPixelFormat;
    _viewFramebuffer.sampleCount = view.sampleCount;

    [self _createVertexDescriptor];

    // first time use the default library, if reload is called then use different
    // library
    _shaderLibrary = [_device newDefaultLibrary];

    [self _createRenderPipelines];

    //-----------------------

    MTLDepthStencilDescriptor *depthStateDesc =
        [[MTLDepthStencilDescriptor alloc] init];
    depthStateDesc.depthCompareFunction = _showSettings->isReverseZ
                                              ? MTLCompareFunctionGreaterEqual
                                              : MTLCompareFunctionLessEqual;
    depthStateDesc.depthWriteEnabled = YES;
    _depthStateFull = [_device newDepthStencilStateWithDescriptor:depthStateDesc];

    depthStateDesc.depthCompareFunction = _showSettings->isReverseZ
                                              ? MTLCompareFunctionGreaterEqual
                                              : MTLCompareFunctionLessEqual;
    depthStateDesc.depthWriteEnabled = NO;
    _depthStateNone = [_device newDepthStencilStateWithDescriptor:depthStateDesc];

    for (NSUInteger i = 0; i < MaxBuffersInFlight; i++) {
        _dynamicUniformBuffer[i] =
            [_device newBufferWithLength:sizeof(Uniforms)
                                 options:MTLResourceStorageModeShared];

        _dynamicUniformBuffer[i].label = @"UniformBuffer";
    }

    _commandQueue = [_device newCommandQueue];

    [self _createSamplers];

    //-----------------------

    [self _createComputePipelines];

    [self _createSampleRender];
}

- (BOOL)hotloadShaders:(const char *)filename
{
    _metallibFileURL =
        [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];

    NSError* err = nil;
    NSDate* fileDate = nil;
    [_metallibFileURL getResourceValue:&fileDate
                                forKey:NSURLContentModificationDateKey
                                 error:&err];

    // only reload if the metallib changed timestamp, otherwise default.metallib
    // has most recent copy
    if (err != nil || [_metallibFileDate isEqualToDate:fileDate]) {
        return NO;
    }
    _metallibFileDate = fileDate;

    // Now dynamically load the metallib
    NSData *dataNS = [NSData dataWithContentsOfURL:_metallibFileURL
                                           options:NSDataReadingMappedIfSafe
                                             error:&err];
    if (dataNS == nil) {
        return NO;
    }
    dispatch_data_t data = dispatch_data_create(dataNS.bytes, dataNS.length,
                                                dispatch_get_main_queue(),
                                                DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    id<MTLLibrary> shaderLibrary = [_device newLibraryWithData:data error:&err];
    if (err != nil) {
        return NO;
    }
    _shaderLibrary = shaderLibrary;

    // rebuild the shaders and pipelines that use the shader
    [self _createRenderPipelines];

    [self _createComputePipelines];

    [self _createSampleRender];

    return YES;
}

- (id<MTLComputePipelineState>)_createComputePipeline:(const char *)name
{
    NSString* nameNS = [NSString stringWithUTF8String:name];
    NSError* error = nil;
    id<MTLFunction> computeFunction = [_shaderLibrary newFunctionWithName:nameNS];

    id<MTLComputePipelineState> pipe;
    if (computeFunction) {
        computeFunction.label = nameNS;

        pipe = [_device newComputePipelineStateWithFunction:computeFunction
                                                      error:&error];
    }

    if (!pipe) {
        KLOGE("kramv", "Failed to create compute pipeline state for %s, error %s",
              name, error ? error.localizedDescription.UTF8String : "");
        return nil;
    }

    return pipe;
}

- (void)_createComputePipelines
{
    _pipelineStateImageCS = [self _createComputePipeline:"SampleImageCS"];
    _pipelineStateImageArrayCS =
        [self _createComputePipeline:"SampleImageArrayCS"];
    _pipelineStateVolumeCS = [self _createComputePipeline:"SampleVolumeCS"];
    _pipelineStateCubeCS = [self _createComputePipeline:"SampleCubeCS"];
    _pipelineStateCubeArrayCS = [self _createComputePipeline:"SampleCubeArrayCS"];
    _pipelineState1DArrayCS =
        [self _createComputePipeline:"SampleImage1DArrayCS"];
}

- (id<MTLRenderPipelineState>)_createRenderPipeline:(const char *)vs
                                                 fs:(const char *)fs
{
    NSString* vsNameNS = [NSString stringWithUTF8String:vs];
    NSString* fsNameNS = [NSString stringWithUTF8String:fs];

    id<MTLFunction> vertexFunction;
    id<MTLFunction> fragmentFunction;

    MTLRenderPipelineDescriptor* pipelineStateDescriptor =
        [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = fsNameNS;
    pipelineStateDescriptor.sampleCount = _viewFramebuffer.sampleCount;
    pipelineStateDescriptor.vertexDescriptor = _mtlVertexDescriptor;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat =
        _viewFramebuffer.colorPixelFormat;

    // Note: blending is disabled on color target, all blending done in shader
    // since have checkerboard and other stuff to blend against.
    
    // TODO: could drop these for images, but want a 3D preview of content
    // or might make these memoryless.
    pipelineStateDescriptor.depthAttachmentPixelFormat =
        _viewFramebuffer.depthStencilPixelFormat;
    pipelineStateDescriptor.stencilAttachmentPixelFormat =
        _viewFramebuffer.depthStencilPixelFormat;

    NSError* error = NULL;

    //-----------------------

    vertexFunction = [_shaderLibrary newFunctionWithName:vsNameNS];
    fragmentFunction = [_shaderLibrary newFunctionWithName:fsNameNS];

    id<MTLRenderPipelineState> pipe;

    if (vertexFunction && fragmentFunction) {
        vertexFunction.label = vsNameNS;
        fragmentFunction.label = fsNameNS;

        pipelineStateDescriptor.vertexFunction = vertexFunction;
        pipelineStateDescriptor.fragmentFunction = fragmentFunction;

        pipe = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor
                                                       error:&error];
    }

    if (!pipe) {
        KLOGE("kramv", "Failed to create render pipeline state for %s, error %s",
              fs, error ? error.description.UTF8String : "");
        return nil;
    }

    return pipe;
}

- (void)_createRenderPipelines
{
    _pipelineStateImage = [self _createRenderPipeline:"DrawImageVS"
                                                   fs:"DrawImagePS"];
    _pipelineStateImageArray = [self _createRenderPipeline:"DrawImageVS"
                                                        fs:"DrawImageArrayPS"];
    _pipelineState1DArray = [self _createRenderPipeline:"DrawImageVS"
                                                     fs:"Draw1DArrayPS"];
    _pipelineStateCube = [self _createRenderPipeline:"DrawCubeVS"
                                                  fs:"DrawCubePS"];
    _pipelineStateCubeArray = [self _createRenderPipeline:"DrawCubeVS"
                                                       fs:"DrawCubeArrayPS"];
    _pipelineStateVolume = [self _createRenderPipeline:"DrawVolumeVS"
                                                    fs:"DrawVolumePS"];
    
    _pipelineStateDrawLines = [self _createRenderPipeline:"DrawLinesVS"
                                                       fs:"DrawLinesPS"];
     
}

- (void)_createSampleRender
{
    {
        // writing to this texture
        MTLTextureDescriptor* textureDesc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                         width:1
                                        height:1
                                     mipmapped:NO];

        textureDesc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        textureDesc.storageMode = MTLStorageModeManaged;
        _sampleComputeTex = [_device newTextureWithDescriptor:textureDesc];
    }

    {
        // this must match drawable format due to using a blit to copy pixel out of
        // drawable
        MTLTextureDescriptor* textureDesc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                         width:1
                                        height:1
                                     mipmapped:NO];
        // textureDesc.usage = MTLTextureUsageShaderWrite |
        // MTLTextureUsageShaderRead;
        textureDesc.storageMode = MTLStorageModeManaged;

        _sampleRenderTex = [_device newTextureWithDescriptor:textureDesc];
    }
}

- (MTKMesh *)_createMeshAsset:(const char *)name
                      mdlMesh:(MDLMesh *)mdlMesh
                     doFlipUV:(bool)doFlipUV
{
    NSError* error = nil;

    mdlMesh.vertexDescriptor = _mdlVertexDescriptor;

    // ModelIO has the uv going counterclockwise on sphere/cylinder, but not on
    // the box. And it also has a flipped bitangent.w.

    // flip the u coordinate
    if (doFlipUV) {
        id<MDLMeshBuffer> uvs = mdlMesh.vertexBuffers[BufferIndexMeshUV0];
        MDLMeshBufferMap* uvsMap = [uvs map];

        packed_float2* uvData = (packed_float2 *)uvsMap.bytes;

        for (uint32_t i = 0; i < mdlMesh.vertexCount; ++i) {
            auto &uv = uvData[i];

            uv.x = 1.0f - uv.x;
        }
    }

    [mdlMesh
        addOrthTanBasisForTextureCoordinateAttributeNamed:
            MDLVertexAttributeTextureCoordinate
                                     normalAttributeNamed:MDLVertexAttributeNormal
                                    tangentAttributeNamed:
                                        MDLVertexAttributeTangent];

    // DONE: flip the bitangent.w sign here, and remove the flip in the shader
    bool doFlipBitangent = true;
    if (doFlipBitangent) {
        id<MDLMeshBuffer> uvs = mdlMesh.vertexBuffers[BufferIndexMeshTangent];
        MDLMeshBufferMap* uvsMap = [uvs map];
        packed_float4* uvData = (packed_float4 *)uvsMap.bytes;

        for (uint32_t i = 0; i < mdlMesh.vertexCount; ++i) {
            //            if (uvData[i].w != -1.0f && uvData[i].w != 1.0f) {
            //                int bp = 0;
            //                bp = bp;
            //            }

            uvData[i].w = -uvData[i].w;
        }
    }

    // now set it into mtk mesh
    MTKMesh* mesh = [[MTKMesh alloc] initWithMesh:mdlMesh
                                           device:_device
                                            error:&error];
    mesh.name = [NSString stringWithUTF8String:name];

    // these range names may only show up when looking at geometry in capture
    // These don't seem to appear as the buffer name that is suballocated from
    {
        // name the vertex range on the vb
        MTKMeshBuffer* pos = mesh.vertexBuffers[BufferIndexMeshPosition];
        MTKMeshBuffer* uvs = mesh.vertexBuffers[BufferIndexMeshUV0];
        MTKMeshBuffer* normals = mesh.vertexBuffers[BufferIndexMeshNormal];
        MTKMeshBuffer* tangents = mesh.vertexBuffers[BufferIndexMeshTangent];

        [pos.buffer addDebugMarker:@"Pos"
                             range:NSMakeRange(pos.offset, pos.length)];
        [uvs.buffer addDebugMarker:@"UV" range:NSMakeRange(uvs.offset, uvs.length)];
        [normals.buffer addDebugMarker:@"Nor"
                                 range:NSMakeRange(normals.offset, normals.length)];
        [tangents.buffer
            addDebugMarker:@"Tan"
                     range:NSMakeRange(tangents.offset, tangents.length)];

        // This seems to already be named "ellisoid-Indices",
        // need to do for ib as well
        for (MTKSubmesh* submesh in mesh.submeshes) {
            [submesh.indexBuffer.buffer
                addDebugMarker:mesh.name
                         range:NSMakeRange(submesh.indexBuffer.offset,
                                           submesh.indexBuffer.length)];
        }
    }

    if (!mesh || error) {
        KLOGE("kramv", "Error creating MetalKit mesh %s",
              error.localizedDescription.UTF8String);
        return nil;
    }

    return mesh;
}

// why isn't this defined in simd lib?
struct packed_float3 {
    float x, y, z;
};


- (void)releaseAllPendingTextures
{
    @autoreleasepool {
        [_loader releaseAllPendingTextures];
        
        // also release the model and cached textures in the renderer
        [self unloadModel];
    }
}

- (void)updateAnimationState:(nonnull MTKView*)view
{
    bool animateDisplay = self.playAnimations;
    
    // animate the uvPreviw until it reaches endPoint, no scrubber yet
    _showSettings->updateUVPreviewState();
    
    if (_showSettings->uvPreviewFrames > 0) {
        _showSettings->uvPreviewFrames--;
        animateDisplay = true;
    }
    
    view.enableSetNeedsDisplay = !animateDisplay;
    view.paused = !animateDisplay;
}


- (void)updateModelSettings:(const string &)fullFilename
{
    _showSettings->isModel = true;
    _showSettings->numChannels = 0; // hides rgba
    
    // don't want any scale on view, or as little as possible
    _showSettings->imageBoundsX = 1;
    _showSettings->imageBoundsY = 1;
    
    BOOL isNewFile = YES;
    [self resetSomeImageSettings:isNewFile];
}

- (BOOL)loadModel:(nonnull const char*)filename
{
    NSURL* fileURL =
        [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];

#if USE_GLTF

        
    
        // TODO: move to async version of this, many of these load slow
        // but is there a way to cancel the load.  Or else move to cgltf which is faster.
        // see GLTFKit2.

#define DO_ASYNC 0
#if DO_ASYNC
        // [GLTFAsset loadAssetWithURL:url bufferAllocator:_bufferAllocator delegate:self];
#else
    @autoreleasepool {
        GLTFAsset* newAsset = [[GLTFAsset alloc] initWithURL:fileURL bufferAllocator:_bufferAllocator];

        if (!newAsset) {
            return NO;
        }

        // tie into delegate callback
        [self assetWithURL:fileURL didFinishLoading:newAsset];
    }
#endif

    // Can't really report YES to caller, since it may fail to load async
    return YES;
#else
    return NO;
#endif
}

- (void)unloadModel
{
#if USE_GLTF
    _asset = nil;
    _animationTime = 0.0;
    [_gltfRenderer releaseAllResources];
#endif
}


    
- (void)_createMeshRect:(float)aspectRatioXToY
{
    // This is a box that's smashed down to a thin 2d z plane, can get very close to it
    // due to the thinness of the volume without nearZ intersect
    
    /// Load assets into metal objects
    MDLMesh *mdlMesh;

    mdlMesh = [MDLMesh newBoxWithDimensions:(vector_float3){aspectRatioXToY, 1, 0.001}
                                   segments:(vector_uint3){1, 1, 1}
                               geometryType:MDLGeometryTypeTriangles
                              inwardNormals:NO
                                  allocator:_metalAllocator];

    // for some reason normals are all n = 1,0,0 which doesn't make sense on a box
    // for the side that is being viewed.
    
    // only one of these for now, but really should store per image
    _meshRect = [self _createMeshAsset:"MeshRect" mdlMesh:mdlMesh doFlipUV:false];
}

- (void)_loadAssets
{
    /// Load assets into metal objects

    MDLMesh* mdlMesh;

    mdlMesh = [MDLMesh newBoxWithDimensions:(vector_float3){1, 1, 1}
                                   segments:(vector_uint3){1, 1, 1}
                               geometryType:MDLGeometryTypeTriangles
                              inwardNormals:NO
                                  allocator:_metalAllocator];

    _meshBox = [self _createMeshAsset:"MeshBox" mdlMesh:mdlMesh doFlipUV:false];

    // The sphere/cylinder shapes are v increasing in -Y, and u increasing
    // conterclockwise, u is the opposite direction to the cube/plane, so need to
    // flip those coords I think this has also flipped the tangents the wrong way,
    // but building tangents after flipping u direction doesn't flip the
    // bitangent.  So bitangent.w is flipped. For sanity, Tangent is increasing u,
    // and Bitangent is increasing v.

    // All prims are viewed with +Y, not +Z up

    mdlMesh = [MDLMesh newEllipsoidWithRadii:(vector_float3){0.5, 0.5, 0.5}
                              radialSegments:16
                            verticalSegments:16
                                geometryType:MDLGeometryTypeTriangles
                               inwardNormals:NO
                                  hemisphere:NO
                                   allocator:_metalAllocator];

    float angle = M_PI * 0.5;
    float2 cosSin = float2m(cos(angle), sin(angle));

    {
        mdlMesh.vertexDescriptor = _mdlVertexDescriptor;

        id<MDLMeshBuffer> posBuffer =
            mdlMesh.vertexBuffers[BufferIndexMeshPosition];
        MDLMeshBufferMap* posMap = [posBuffer map];
        packed_float3* posData = (packed_float3 *)posMap.bytes;

        id<MDLMeshBuffer> normalBuffer =
            mdlMesh.vertexBuffers[BufferIndexMeshNormal];
        MDLMeshBufferMap* normalsMap = [normalBuffer map];
        packed_float3* normalData = (packed_float3 *)normalsMap.bytes;

        // vertexCount reports 306, but vertex 289+ are garbage
        uint32_t numVertices = 289;  // mdlMesh.vertexCount

        for (uint32_t i = 0; i < numVertices; ++i) {
            {
                auto &pos = posData[i];

                // dumb rotate about Y-axis
                auto copy = pos;

                pos.x = copy.x * cosSin.x - copy.z * cosSin.y;
                pos.z = copy.x * cosSin.y + copy.z * cosSin.x;
            }

            {
                auto &normal = normalData[i];
                auto copy = normal;
                normal.x = copy.x * cosSin.x - copy.z * cosSin.y;
                normal.z = copy.x * cosSin.y + copy.z * cosSin.x;
            }
        }

        // Hack - knock out all bogus tris from ModelIO that lead to garbage tris
        for (uint32_t i = numVertices; i < mdlMesh.vertexCount; ++i) {
            auto &pos = posData[i];
            pos.x = NAN;
        }
    }

    _meshSphere = [self _createMeshAsset:"MeshSphere"
                                 mdlMesh:mdlMesh
                                doFlipUV:true];

    mdlMesh = [MDLMesh newEllipsoidWithRadii:(vector_float3){0.5, 0.5, 0.5}
                              radialSegments:16
                            verticalSegments:16
                                geometryType:MDLGeometryTypeTriangles
                               inwardNormals:NO
                                  hemisphere:NO
                                   allocator:_metalAllocator];

    // ModelIO has the uv going counterclockwise on sphere/cylinder, but not on
    // the box. And it also has a flipped bitangent.w.

    // flip the u coordinate
    bool doFlipUV = true;
    if (doFlipUV) {
        mdlMesh.vertexDescriptor = _mdlVertexDescriptor;

        id<MDLMeshBuffer> uvsBuffer = mdlMesh.vertexBuffers[BufferIndexMeshUV0];
        MDLMeshBufferMap* uvsMap = [uvsBuffer map];
        packed_float2* uvData = (packed_float2 *)uvsMap.bytes;

        // this is all aos

        id<MDLMeshBuffer> posBuffer =
            mdlMesh.vertexBuffers[BufferIndexMeshPosition];
        MDLMeshBufferMap* posMap = [posBuffer map];
        packed_float3 *posData = (packed_float3 *)posMap.bytes;

        id<MDLMeshBuffer> normalsBuffe =
            mdlMesh.vertexBuffers[BufferIndexMeshNormal];
        MDLMeshBufferMap* normalsMap = [normalsBuffe map];
        packed_float3* normalData = (packed_float3 *)normalsMap.bytes;

        // vertexCount reports 306, but vertex 289+ are garbage
        uint32_t numVertices = 289;  // mdlMesh.vertexCount

        for (uint32_t i = 0; i < numVertices; ++i) {
            {
                auto &pos = posData[i];

                // dumb rotate about Y-axis
                auto copy = pos;
                pos.x = copy.x * cosSin.x - copy.z * cosSin.y;
                pos.z = copy.x * cosSin.y + copy.z * cosSin.x;
            }

            {
                auto &normal = normalData[i];
                auto copy = normal;
                normal.x = copy.x * cosSin.x - copy.z * cosSin.y;
                normal.z = copy.x * cosSin.y + copy.z * cosSin.x;
            }

            auto &uv = uvData[i];

            //            if (uv.x < 0.0 || uv.x > 1.0) {
            //                int bp = 0;
            //                bp = bp;
            //            }

            // this makes it counterclockwise 0 to 1
            float x = uv.x;

            x = 1.0f - x;

            // -1 to 1 counterclockwise
            x = 2.0f * x - 1.0f;

            if (x <= 0) {
                // now -1 to 0 is 0 to 1 clockwise with 1 in back
                x = 1.0f + x;
            }
            else {
                // 0 to 1, now 1 to 0 with 1 in back
                x = 1.0f - x;
            }

            uv.x = x;
        }

        // Hack - knock out all bogus tris from ModelIO that lead to garbage tris
        for (uint32_t i = numVertices; i < mdlMesh.vertexCount; ++i) {
            auto &pos = posData[i];
            pos.x = NAN;
        }

        // TODO: may need to flip tangent on the inverted side
        // otherwise lighting is just wrong, but tangents generated in
        // _createMeshAsset move that here, and flip the tangents in the loop
    }

    _meshSphereMirrored = [self _createMeshAsset:"MeshSphereMirrored"
                                         mdlMesh:mdlMesh
                                        doFlipUV:false];

    // this maps 1/3rd of texture to the caps, and just isn't a very good uv
    // mapping, using capsule instead
    //    mdlMesh = [MDLMesh newCylinderWithHeight:1.0
    //                                       radii:(vector_float2){0.5, 0.5}
    //                                            radialSegments:16
    //                                        verticalSegments:1
    //                                        geometryType:MDLGeometryTypeTriangles
    //                                       inwardNormals:NO
    //                                           allocator:_metalAllocator];
    //
    //    _meshCylinder = [self _createMeshAsset:"MeshCylinder" mdlMesh:mdlMesh
    //    doFlipUV:true];

    mdlMesh = [MDLMesh newCapsuleWithHeight:1.0
                                      radii:(vector_float2){1.0f/3.0f, 1.0f/3.0f} // circle
                             // vertical cap subtracted from height
                             radialSegments:16
                           verticalSegments:1
                         hemisphereSegments:16
                               geometryType:MDLGeometryTypeTriangles
                              inwardNormals:NO
                                  allocator:_metalAllocator];

    _meshCapsule = [self _createMeshAsset:"MeshCapsule"
                                  mdlMesh:mdlMesh
                                 doFlipUV:true];

    // this will get set based on sahpe
    _mesh = nil;
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


- (BOOL)loadTextureFromImage:(nonnull const char *)fullFilenameString
                   timestamp:(double)timestamp
                       image:(kram::KTXImage &)image
                 imageNormal:(kram::KTXImage *)imageNormal
                   isArchive:(BOOL)isArchive
{
    // image can be decoded to rgba8u if platform can't display format natively
    // but still want to identify blockSize from original format
    string fullFilename = fullFilenameString;
    const char* filenameShort = toFilenameShort(fullFilename.c_str());
    
    bool isTextureNew = _showSettings->isFileNew(fullFilename.c_str());
    bool isTextureChanged = _showSettings->isFileChanged(fullFilename.c_str(), timestamp);
    
    if (isTextureChanged) {
        // synchronously cpu upload from ktx file to buffer, with eventual gpu blit
        // from buffer to returned texture.  TODO: If buffer is full, then something
        // needs to keep KTXImage and data alive.  This load may also decode the
        // texture to RGBA8.

        MTLPixelFormat originalFormatMTL = MTLPixelFormatInvalid;
        id<MTLTexture> texture = [_loader loadTextureFromImage:image
                                                originalFormat:&originalFormatMTL
                                                          name:filenameShort];
        if (!texture) {
            return NO;
        }

        // hacking in the normal texture here, so can display them together during
        // preview
        id<MTLTexture> normalTexture;
        if (imageNormal) {
            normalTexture = [_loader loadTextureFromImage:*imageNormal
                                           originalFormat:nil
                                                     name:filenameShort];
            if (!normalTexture) {
                return NO;
            }
        }

        // if archive contained png, then it's been converted to ktx
        // so the info below may not reflect original data
        // Would need original png data to look at header
        // This is only info on image, not on imageNormal

        bool isPNG = isPNGFilename(fullFilename.c_str());
        if (!isArchive && isPNG) {
            _showSettings->imageInfo = kramInfoToString(fullFilename, false);
            _showSettings->imageInfoVerbose = kramInfoToString(fullFilename, true);
        }
        else {
            _showSettings->imageInfo =
                kramInfoKTXToString(fullFilename, image, false);
            _showSettings->imageInfoVerbose =
                kramInfoKTXToString(fullFilename, image, true);
        }

        _showSettings->originalFormat = (MyMTLPixelFormat)originalFormatMTL;
        _showSettings->decodedFormat = (MyMTLPixelFormat)texture.pixelFormat;

        _showSettings->lastFilename = fullFilename;
        _showSettings->lastTimestamp = timestamp;

        @autoreleasepool {
            _colorMap = texture;
            _normalMap = normalTexture;
        }

        // this is the actual format, may have been decoded
        MyMTLPixelFormat format = (MyMTLPixelFormat)_colorMap.pixelFormat;
       _data->updateImageSettings(fullFilename, image, format);
    }

    [self resetSomeImageSettings:isTextureNew];

    return YES;
}

- (BOOL)loadTexture:(nonnull NSURL *)url
{
    string fullFilename = url.path.UTF8String;

    // can use this to pull, or use fstat on FileHelper
    NSDate* fileDate = nil;
    NSError* error = nil;
    [url getResourceValue:&fileDate
                   forKey:NSURLContentModificationDateKey
                    error:&error];

    // DONE: tie this to url and modstamp differences
    double timestamp = fileDate.timeIntervalSince1970;
    
    bool isTextureNew = _showSettings->isFileNew(fullFilename.c_str());
    bool isTextureChanged = _showSettings->isFileChanged(fullFilename.c_str(), timestamp);
    
    // image can be decoded to rgba8u if platform can't display format natively
    // but still want to identify blockSize from original format
    if (isTextureChanged) {
        // TODO: hold onto these, so can reference block data
        KTXImage image;
        KTXImageData imageData;

        if (![_loader loadImageFromURL:url image:image imageData:imageData]) {
            return NO;
        }

        const char* filenameShort = toFilenameShort(fullFilename.c_str());
        
        MTLPixelFormat originalFormatMTL = MTLPixelFormatInvalid;
        id<MTLTexture> texture = [_loader loadTextureFromImage:image
                                                originalFormat:&originalFormatMTL
                                                          name:filenameShort];
        if (!texture) {
            return NO;
        }

        // This doesn't look for or load corresponding normal map, but should

        // this is not the png data, but info on converted png to ktx level
        // But this avoids loading the image 2 more times
        // Size of png is very different than decompressed or recompressed ktx
        bool isPNG = isPNGFilename(fullFilename.c_str());
        if (isPNG) {
            _showSettings->imageInfo = kramInfoToString(fullFilename, false);
            _showSettings->imageInfoVerbose = kramInfoToString(fullFilename, true);
        }
        else {
            _showSettings->imageInfo =
                kramInfoKTXToString(fullFilename, image, false);
            _showSettings->imageInfoVerbose =
                kramInfoKTXToString(fullFilename, image, true);
        }

        _showSettings->originalFormat = (MyMTLPixelFormat)originalFormatMTL;
        _showSettings->decodedFormat = (MyMTLPixelFormat)texture.pixelFormat;

        _showSettings->lastFilename = fullFilename;
        _showSettings->lastTimestamp = timestamp;

        @autoreleasepool {
            _colorMap = texture;
            _normalMap = nil;
        }

        MyMTLPixelFormat format = (MyMTLPixelFormat)_colorMap.pixelFormat;
        _data->updateImageSettings(fullFilename, image, format);
    }

    [self resetSomeImageSettings:isTextureNew];

    return YES;
}


- (void)resetSomeImageSettings:(BOOL)isNewFile
{
    _data->resetSomeImageSettings(isNewFile);
    
    // the rect is ar:1 for images
    float aspectRatioXtoY = _showSettings->imageAspectRatio();
    [self _createMeshRect:aspectRatioXtoY];
}



- (void)_updateGameState
{
    /// Update any game state before encoding rendering commands to our drawable

    Uniforms &uniforms =
        *(Uniforms *)_dynamicUniformBuffer[_uniformBufferIndex].contents;

    uniforms.isNormal = _showSettings->texContentType == TexContentTypeNormal;
    uniforms.doShaderPremul = _showSettings->doShaderPremul;
    uniforms.isSigned = _showSettings->isSigned;
    uniforms.isSwizzleAGToRG = _showSettings->isSwizzleAGToRG;

    uniforms.isSDF = _showSettings->texContentType == TexContentTypeSDF;
    uniforms.numChannels = _showSettings->numChannels;
    uniforms.lightingMode = (ShaderLightingMode)_showSettings->lightingMode;

    MyMTLTextureType textureType = MyMTLTextureType2D;
    MyMTLPixelFormat textureFormat = MyMTLPixelFormatInvalid;
    if (_colorMap) {
        textureType = (MyMTLTextureType)_colorMap.textureType;
        textureFormat = (MyMTLPixelFormat)_colorMap.pixelFormat;
    }

    uniforms.isCheckerboardShown = _showSettings->isCheckerboardShown;

    // addressing mode
    bool isCube = (textureType == MyMTLTextureTypeCube ||
                   textureType == MyMTLTextureTypeCubeArray);
    bool doWrap = !isCube && _showSettings->isWrap;
    bool doEdge = !doWrap;
    bool doZero = !doEdge;
    uniforms.isWrap = doWrap ? _showSettings->isWrap : false;

    uniforms.isPreview = _showSettings->isPreview;

    uniforms.isNormalMapPreview = false;
    if (uniforms.isPreview) {
        uniforms.isNormalMapPreview = uniforms.isNormal || (_normalMap != nil);

        if (_normalMap != nil) {
            uniforms.isNormalMapSigned =
                isSignedFormat((MyMTLPixelFormat)_normalMap.pixelFormat);
            uniforms.isNormalMapSwizzleAGToRG = false;  // TODO: need a prop for this
        }
    }

    // a few things to fix before enabling this
    uniforms.useTangent = _showSettings->useTangent;

    uniforms.gridX = 0;
    uniforms.gridY = 0;

    if (_showSettings->isPixelGridShown) {
        uniforms.gridX = 1;
        uniforms.gridY = 1;
    }
    else if (_showSettings->isBlockGridShown) {
        if (_showSettings->blockX > 1) {
            uniforms.gridX = _showSettings->blockX;
            uniforms.gridY = _showSettings->blockY;
        }
    }
    else if (_showSettings->isAtlasGridShown) {
        uniforms.gridX = _showSettings->gridSizeX;
        uniforms.gridY = _showSettings->gridSizeY;
    }

    // no debug mode when preview kicks on, make it possible to toggle back and
    // forth more easily
    uniforms.debugMode = (ShaderDebugMode)_showSettings->debugMode;
    uniforms.shapeChannel = (ShaderShapeChannel)_showSettings->shapeChannel;
    uniforms.channels = (ShaderTextureChannels)_showSettings->channels;

    // turn these off in preview mode, but they may be useful?
    if (_showSettings->isPreview) {
        uniforms.debugMode = ShaderDebugMode::ShDebugModeNone;
        uniforms.shapeChannel = ShaderShapeChannel::ShShapeChannelNone;
    }

    // crude shape experiment
    _showSettings->is3DView = true;
    switch (_showSettings->meshNumber) {
        case 0:
            _mesh = _meshRect;
            _showSettings->is3DView = false;
            break;
        case 1:
            _mesh = _meshBox;
            break;
        case 2:
            _mesh = _meshSphere;
            break;
        case 3:
            _mesh = _meshSphereMirrored;
            break;
        // case 3: _mesh = _meshCylinder; break;
        case 4:
            _mesh = _meshCapsule;
            break;
    }
    uniforms.is3DView = _showSettings->is3DView;

    // on small textures can really see missing pixel (3 instead of 4 pixels)
    // so only do this on the sphere/capsule which wrap-around uv space
    uniforms.isInsetByHalfPixel = false;
    if (_showSettings->meshNumber >= 2 && doZero) {
        uniforms.isInsetByHalfPixel = true;
    }

    _data->updateTransforms();
    
    // this is an animated effect, that overlays the shape uv wires over the image
    uniforms.isUVPreview = _showSettings->uvPreview > 0.0;
    uniforms.uvPreview = _showSettings->uvPreview;
   
    uniforms.projectionViewMatrix = _data->_projectionViewMatrix;
    uniforms.cameraPosition = _data->_cameraPosition;
   
    // This is per object
    uniforms.modelMatrix = _data->_modelMatrix;
    uniforms.modelMatrixInvScale2 = _data->_modelMatrixInvScale2;

    //_rotation += .01;
}

- (void)_setUniformsLevel:(UniformsLevel &)uniforms mipLOD:(int32_t)mipLOD
{
    uniforms.mipLOD = mipLOD;

    uniforms.arrayOrSlice = 0;
    uniforms.face = 0;

    uniforms.textureSize = float4m(0.0f);
    MyMTLTextureType textureType = MyMTLTextureType2D;
    if (_colorMap) {
        textureType = (MyMTLTextureType)_colorMap.textureType;
        uniforms.textureSize =
            float4m(_colorMap.width, _colorMap.height, 1.0f / _colorMap.width,
                    1.0f / _colorMap.height);
    }

    // TODO: set texture specific uniforms, but using single _colorMap for now
    switch (textureType) {
        case MyMTLTextureType2D:
            // nothing
            break;
        case MyMTLTextureType3D:
            uniforms.arrayOrSlice = _showSettings->sliceNumber;
            break;
        case MyMTLTextureTypeCube:
            uniforms.face = _showSettings->faceNumber;
            break;

        case MyMTLTextureTypeCubeArray:
            uniforms.face = _showSettings->faceNumber;
            uniforms.arrayOrSlice = _showSettings->arrayNumber;
            break;
        case MyMTLTextureType2DArray:
            uniforms.arrayOrSlice = _showSettings->arrayNumber;
            break;
        case MyMTLTextureType1DArray:
            uniforms.arrayOrSlice = _showSettings->arrayNumber;
            break;

        default:
            break;
    }
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool {
        // Per frame updates here

        // update per frame state
        [self updateAnimationState:view];
        
        // TODO: move this out, needs to get called off mouseMove, but don't want to
        // call drawMain
        [self drawSample];

        // decrement count, proceeds if sema >= 0 afterwards
        Signpost postWait("waitOnSemaphore");
        dispatch_semaphore_wait(_inFlightSemaphore, DISPATCH_TIME_FOREVER);
        postWait.stop();
        
        _uniformBufferIndex = (_uniformBufferIndex + 1) % MaxBuffersInFlight;

        id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
        commandBuffer.label = @"MyCommand";

        __block dispatch_semaphore_t block_sema = _inFlightSemaphore;
        
        #if USE_GLTF
                GLTFMTLRenderer* gltfRenderer = _gltfRenderer;
                [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> /* buffer */) {
                    [gltfRenderer signalFrameCompletion];
        
                    // increment count
                    dispatch_semaphore_signal(block_sema);
                }];
        
        #else
                [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> /* buffer */) {
                    // increment count
                    dispatch_semaphore_signal(block_sema);
                }];
        #endif

        [self _updateGameState];

        // use to autogen mipmaps if needed, might eliminate this since it's always
        // box filter
        // TODO: do mips via kram instead, but was useful for pow-2 mip comparisons.

        // also use to readback pixels
        // also use for async texture upload
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
        if (blitEncoder) {
            Signpost postUpload("uploadTextures");
            blitEncoder.label = @"MyBlitEncoder";
            [_loader uploadTexturesIfNeeded:blitEncoder commandBuffer:commandBuffer];
            [blitEncoder endEncoding];
        }

        Signpost postDraw("Draw");
        [self drawMain:commandBuffer
                  view:view];
        postDraw.stop();
        
        // hold onto this for sampling from it via eyedropper
        id<CAMetalDrawable> drawable = view.currentDrawable;
        _lastDrawableTexture = drawable.texture;

        // These are equivalent
        // [commandBuffer presentDrawable:view.currentDrawable];
        [commandBuffer addScheduledHandler:^(id<MTLCommandBuffer> cmdBuf) {
            Signpost postPresent("presentDrawble");
            [drawable present];
        }];

        [commandBuffer commit];
    }
}

#if USE_GLTF

static GLTFBoundingSphere GLTFBoundingSphereFromBox2(const GLTFBoundingBox b) {
    GLTFBoundingSphere s;
    float3 center = 0.5f * (b.minPoint + b.maxPoint);
    float r = simd::distance(b.maxPoint, center);
    
    s.center = center;
    s.radius = r;
    return s;
}
#endif


- (void)drawMain:(id<MTLCommandBuffer>)commandBuffer
            view:(nonnull MTKView *)view
{
    // Delay getting the currentRenderPassDescriptor until absolutely needed. This
    // avoids
    //   holding onto the drawable and blocking the display pipeline any longer
    //   than necessary
    MTLRenderPassDescriptor* renderPassDescriptor = nil;
    
    // This retrieval can take 20ms+ when gpu is busy
    Signpost post("nextDrawable");
    renderPassDescriptor = view.currentRenderPassDescriptor;
    post.stop();
    
    if (renderPassDescriptor == nil) {
        return;
    }

    if (_colorMap == nil
#if USE_GLTF
        && _asset == nil
#endif
    )
    {
        // this will clear target
        id<MTLRenderCommandEncoder> renderEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];

        if (renderEncoder) {
            renderEncoder.label = @"MainRender";
            [renderEncoder endEncoding];
        }

        return;
    }

#if USE_GLTF
    {
        mylock lock(gModelLock);
    
        if (_asset) {
            
            // TODO: needs to be done in the render loop, since it must run compute
            // This runs compute to generate radiance/irradiance in mip levels
            // Also an equirect version for a 2d image
            if (_environmentNeedsUpdate) {
                if (_environmentTexture.textureType == MTLTextureTypeCube)
                    [_gltfRenderer.lightingEnvironment generateFromCubeTexture:_environmentTexture commandBuffer:commandBuffer];
                else
                    [_gltfRenderer.lightingEnvironment generateFromEquirectTexture:_environmentTexture commandBuffer:commandBuffer];
                
                _environmentNeedsUpdate = false;
            }
        }
    }
#endif

    
    // Final pass rendering code here
    id<MTLRenderCommandEncoder> renderEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    if (!renderEncoder) {
        return;
    }

    renderEncoder.label = @"MainRender";

    // set raster state
    [renderEncoder setFrontFacingWinding:_showSettings->isInverted
                                             ? MTLWindingClockwise
                                             : MTLWindingCounterClockwise];
    [renderEncoder setCullMode:MTLCullModeBack];
    [renderEncoder setDepthStencilState:_depthStateFull];

    bool drawShape = true;
    
    #if USE_GLTF
    {
        mylock lock(gModelLock);

        if (_asset) {
            drawShape = false;
    
            // update animations
            if (self.playAnimations) {
                _animationTime += 1.0/60.0;
    
                NSTimeInterval maxAnimDuration = 0;
                for (GLTFAnimation* animation in _asset.animations) {
                    for (GLTFAnimationChannel* channel in animation.channels) {
                        if (channel.duration > maxAnimDuration) {
                            maxAnimDuration = channel.duration;
                        }
                    }
                }
            
                NSTimeInterval animTime = fmod(_animationTime, maxAnimDuration);
    
                for (GLTFAnimation* animation in _asset.animations) {
                    [animation runAtTime:animTime];
                }
            }
            
            // regularization scales the model to 1 unit dimension, may animate out of this box
            // just a scale to diameter 1, and translate back from center and viewer z
            GLTFBoundingSphere bounds = GLTFBoundingSphereFromBox2(_asset.defaultScene.approximateBounds);
            float invScale = (bounds.radius > 0) ? (0.5 / (bounds.radius)) : 1.0;
            float4x4 centerScale = float4x4(float4m(invScale,invScale,invScale,1));
            float4x4 centerTranslation = matrix_identity_float4x4;
            centerTranslation.columns[3] = vector4(-bounds.center, 1.0f);
            float4x4 regularizationMatrix = centerScale * centerTranslation;
    
            // incorporate the rotation now
            Uniforms &uniforms =
                *(Uniforms *)_dynamicUniformBuffer[_uniformBufferIndex].contents;
    
            regularizationMatrix = regularizationMatrix * uniforms.modelMatrix;
    
            // TODO: be able to pass regularization to affect root of modelMatrix tree,
            // do not modify viewMatrix here since that messes with world space.
    
            // set the view and projection matrix
            _gltfRenderer.viewMatrix = _data->_viewMatrix * regularizationMatrix;
            _gltfRenderer.projectionMatrix = _data->_projectionMatrix;
    
            RenderScope drawModelScope( renderEncoder, "DrawModel" );
            [_gltfRenderer renderScene:_asset.defaultScene commandBuffer:commandBuffer commandEncoder:renderEncoder];
        }
    }
    #endif
    
    if (drawShape) {
        RenderScope drawShapeScope( renderEncoder, "DrawShape" );
        
        // set the mesh shape
        for (NSUInteger bufferIndex = 0; bufferIndex < _mesh.vertexBuffers.count;
             bufferIndex++) {
            MTKMeshBuffer *vertexBuffer = _mesh.vertexBuffers[bufferIndex];
            if ((NSNull *)vertexBuffer != [NSNull null]) {
                [renderEncoder setVertexBuffer:vertexBuffer.buffer
                                        offset:vertexBuffer.offset
                                       atIndex:bufferIndex];
            }
        }

        //---------------------------------------
        // figure out the sampler

        id<MTLSamplerState> sampler;

        MyMTLTextureType textureType = (MyMTLTextureType)_colorMap.textureType;

        bool isCube = (textureType == MyMTLTextureTypeCube ||
                       textureType == MyMTLTextureTypeCubeArray);
        bool doWrap = !isCube && _showSettings->isWrap;
        bool doEdge = !doWrap;

        if (_showSettings->isPreview) {
            sampler = doWrap ? _colorMapSamplerFilterWrap
                             : (doEdge ? _colorMapSamplerFilterEdge
                                       : _colorMapSamplerFilterBorder);
        }
        else {
            sampler = doWrap ? _colorMapSamplerNearestWrap
                             : (doEdge ? _colorMapSamplerNearestEdge
                                       : _colorMapSamplerNearestBorder);
        }

        //---------------------------------------
        // for (texture in _textures) // TODO: setup
        // if (_colorMap)
        {
            // TODO: set texture specific uniforms, but using single _colorMap for now
            switch (_colorMap.textureType) {
                case MTLTextureType1DArray:
                    [renderEncoder setRenderPipelineState:_pipelineState1DArray];
                    break;

                case MTLTextureType2D:
                    [renderEncoder setRenderPipelineState:_pipelineStateImage];
                    break;

                case MTLTextureType2DArray:
                    [renderEncoder setRenderPipelineState:_pipelineStateImageArray];
                    break;

                case MTLTextureType3D:
                    [renderEncoder setRenderPipelineState:_pipelineStateVolume];
                    break;
                case MTLTextureTypeCube:
                    [renderEncoder setRenderPipelineState:_pipelineStateCube];
                    break;
                case MTLTextureTypeCubeArray:
                    [renderEncoder setRenderPipelineState:_pipelineStateCubeArray];
                    break;

                default:
                    break;
            }

            id<MTLBuffer> uniformBuffer = _dynamicUniformBuffer[_uniformBufferIndex];
            [renderEncoder setVertexBuffer:uniformBuffer
                                    offset:0
                                   atIndex:BufferIndexUniforms];

            [renderEncoder setFragmentBuffer:uniformBuffer
                                      offset:0
                                     atIndex:BufferIndexUniforms];

            // set the texture up
            [renderEncoder setFragmentTexture:_colorMap atIndex:TextureIndexColor];

            // setup normal map
            if (_normalMap && _showSettings->isPreview) {
                [renderEncoder setFragmentTexture:_normalMap atIndex:TextureIndexNormal];
            }

            UniformsLevel uniformsLevel;
            uniformsLevel.drawOffset = float2m(0.0f);
            uniformsLevel.passNumber = kPassDefault;
            
            if (_showSettings->isPreview) {
                // upload this on each face drawn, since want to be able to draw all
                // mips/levels at once
                [self _setUniformsLevel:uniformsLevel mipLOD:_showSettings->mipNumber];

                [renderEncoder setVertexBytes:&uniformsLevel
                                       length:sizeof(uniformsLevel)
                                      atIndex:BufferIndexUniformsLevel];

                [renderEncoder setFragmentBytes:&uniformsLevel
                                         length:sizeof(uniformsLevel)
                                        atIndex:BufferIndexUniformsLevel];

                // use exisiting lod, and mip
                [renderEncoder setFragmentSamplerState:sampler atIndex:SamplerIndexColor];

                for (MTKSubmesh* submesh in _mesh.submeshes) {
                    [renderEncoder drawIndexedPrimitives:submesh.primitiveType
                                              indexCount:submesh.indexCount
                                               indexType:submesh.indexType
                                             indexBuffer:submesh.indexBuffer.buffer
                                       indexBufferOffset:submesh.indexBuffer.offset];
                }
            }
            else if (_showSettings->isShowingAllLevelsAndMips) {
                int32_t w = _colorMap.width;
                int32_t h = _colorMap.height;
                // int32_t d = _colorMap.depth;

                // gap the contact sheet, note this 2 pixels is scaled on small textures
                // by the zoom
                int32_t gap =
                    _showSettings
                        ->showAllPixelGap;  // * _showSettings->viewContentScaleFactor;

                for (int32_t mip = 0; mip < _showSettings->mipCount; ++mip) {
                    // upload this on each face drawn, since want to be able to draw all
                    // mips/levels at once
                    
                    [self _setUniformsLevel:uniformsLevel mipLOD:mip];
                    
                    if (mip == 0) {
                        uniformsLevel.drawOffset.y = 0.0f;
                    }
                    else {
                        // all mips draw at top mip size currently
                        uniformsLevel.drawOffset.y -= h + gap;
                    }
                    
                    // this its ktxImage.totalChunks()
                    int32_t numLevels = _showSettings->totalChunks();
                    
                    for (int32_t level = 0; level < numLevels; ++level) {
                        RenderScope drawLevelScope( renderEncoder, "DrawLevel" );
                        
                        if (isCube) {
                            uniformsLevel.face = level % 6;
                            uniformsLevel.arrayOrSlice = level / 6;
                        }
                        else {
                            uniformsLevel.arrayOrSlice = level;
                        }
                        
                        // advance x across faces/slices/array elements, 1d array and 2d thin
                        // array are weird though.
                        if (level == 0) {
                            uniformsLevel.drawOffset.x = 0.0f;
                        }
                        else {
                            uniformsLevel.drawOffset.x += w + gap;
                        }
                        
                        [renderEncoder setVertexBytes:&uniformsLevel
                                               length:sizeof(uniformsLevel)
                                              atIndex:BufferIndexUniformsLevel];
                        
                        [renderEncoder setFragmentBytes:&uniformsLevel
                                                 length:sizeof(uniformsLevel)
                                                atIndex:BufferIndexUniformsLevel];
                        
                        // force lod, and don't mip
                        [renderEncoder setFragmentSamplerState:sampler
                                                   lodMinClamp:mip
                                                   lodMaxClamp:mip + 1
                                                       atIndex:SamplerIndexColor];
                        
                        // TODO: since this isn't a preview, have mode to display all faces
                        // and mips on on screen faces and arrays and slices go across in a
                        // row, and mips are displayed down from each of those in a column
                        
                        for (MTKSubmesh* submesh in _mesh.submeshes) {
                            [renderEncoder drawIndexedPrimitives:submesh.primitiveType
                                                      indexCount:submesh.indexCount
                                                       indexType:submesh.indexType
                                                     indexBuffer:submesh.indexBuffer.buffer
                                               indexBufferOffset:submesh.indexBuffer.offset];
                        }
                    }
                }
                
                for (int32_t mip = 0; mip < _showSettings->mipCount; ++mip) {
                    // upload this on each face drawn, since want to be able to draw all
                    // mips/levels at once
                    
                    [self _setUniformsLevel:uniformsLevel mipLOD:mip];

                    if (mip == 0) {
                        uniformsLevel.drawOffset.y = 0.0f;
                    }
                    else {
                        // all mips draw at top mip size currently
                        uniformsLevel.drawOffset.y -= h + gap;
                    }

                    // this its ktxImage.totalChunks()
                    int32_t numLevels = _showSettings->totalChunks();

                    for (int32_t level = 0; level < numLevels; ++level) {
                        if (isCube) {
                            uniformsLevel.face = level % 6;
                            uniformsLevel.arrayOrSlice = level / 6;
                        }
                        else {
                            uniformsLevel.arrayOrSlice = level;
                        }
                        
                        // advance x across faces/slices/array elements, 1d array and 2d thin
                        // array are weird though.
                        if (level == 0) {
                            uniformsLevel.drawOffset.x = 0.0f;
                        }
                        else {
                            uniformsLevel.drawOffset.x += w + gap;
                        }
                        
                        [renderEncoder setVertexBytes:&uniformsLevel
                                               length:sizeof(uniformsLevel)
                                              atIndex:BufferIndexUniformsLevel];
                        
//                        [renderEncoder setFragmentBytes:&uniformsLevel
//                                                 length:sizeof(uniformsLevel)
//                                                atIndex:BufferIndexUniformsLevel];
                        
                        // force lod, and don't mip
//                        [renderEncoder setFragmentSamplerState:sampler
//                                                   lodMinClamp:mip
//                                                   lodMaxClamp:mip + 1
//                                                       atIndex:SamplerIndexColor];
//                        
                        [self drawAtlas:renderEncoder];
                    }
                }
            }
            else {
                int32_t mip = _showSettings->mipNumber;

                // upload this on each face drawn, since want to be able to draw all
                // mips/levels at once
                [self _setUniformsLevel:uniformsLevel mipLOD:mip];

                [renderEncoder setVertexBytes:&uniformsLevel
                                       length:sizeof(uniformsLevel)
                                      atIndex:BufferIndexUniformsLevel];

                [renderEncoder setFragmentBytes:&uniformsLevel
                                         length:sizeof(uniformsLevel)
                                        atIndex:BufferIndexUniformsLevel];

                // force lod, and don't mip
                [renderEncoder setFragmentSamplerState:sampler
                                           lodMinClamp:mip
                                           lodMaxClamp:mip + 1
                                               atIndex:SamplerIndexColor];

                // TODO: since this isn't a preview, have mode to display all faces and
                // mips on on screen faces and arrays and slices go across in a row, and
                // mips are displayed down from each of those in a column

                for (MTKSubmesh* submesh in _mesh.submeshes) {
                    [renderEncoder drawIndexedPrimitives:submesh.primitiveType
                                              indexCount:submesh.indexCount
                                               indexType:submesh.indexType
                                             indexBuffer:submesh.indexBuffer.buffer
                                       indexBufferOffset:submesh.indexBuffer.offset];
                }
                
                // Draw uv wire overlay
                if (_showSettings->is3DView && _showSettings->uvPreview > 0.0) {
                    // need to force color in shader or it's still sampling texture
                    // also need to add z offset
                    
                    RenderScope drawUVPreviewScope( renderEncoder, "DrawUVPreview" );
                    
                    [renderEncoder setTriangleFillMode:MTLTriangleFillModeLines];
                    
                    // only applies to tris, not points/lines, pushes depth away (towards 0), after clip
                    // affects reads/tests and writes.  Could also add in vertex shader.
                    // depthBias * 2^(exp(max abs(z) in primitive) - r) + slopeScale * maxSlope
                    [renderEncoder setDepthBias:0.015 slopeScale:3.0 clamp: 0.02];
                    
                    uniformsLevel.passNumber = kPassUVPreview;
                    
                    [renderEncoder setVertexBytes:&uniformsLevel
                                           length:sizeof(uniformsLevel)
                                          atIndex:BufferIndexUniformsLevel];

                    [renderEncoder setFragmentBytes:&uniformsLevel
                                             length:sizeof(uniformsLevel)
                                            atIndex:BufferIndexUniformsLevel];

                    for (MTKSubmesh* submesh in _mesh.submeshes) {
                        [renderEncoder drawIndexedPrimitives:submesh.primitiveType
                                                  indexCount:submesh.indexCount
                                                   indexType:submesh.indexType
                                                 indexBuffer:submesh.indexBuffer.buffer
                                           indexBufferOffset:submesh.indexBuffer.offset];
                    }
                    
                    uniformsLevel.passNumber = kPassDefault;
                    
                    // restore state, even though this isn't a true state shadow
                    [renderEncoder setDepthBias:0.0 slopeScale:0.0 clamp:0.0];
                    [renderEncoder setTriangleFillMode:MTLTriangleFillModeFill];
                    
                }
                
                [self drawAtlas:renderEncoder];
            }
        }
    }

    [renderEncoder endEncoding];

    // TODO: run any post-processing on each texture visible as fsq
    // TODO: environment map preview should be done as fsq
}

class RenderScope
{
public:
    RenderScope(id encoder_, const char* name)
        : encoder(encoder_)
    {
        id<MTLRenderCommandEncoder> enc = (id<MTLRenderCommandEncoder>)encoder;
        [enc pushDebugGroup: [NSString stringWithUTF8String: name]];
    }
    
    void close()
    {
        if (encoder) {
            id<MTLRenderCommandEncoder> enc = (id<MTLRenderCommandEncoder>)encoder;
            [enc popDebugGroup];
            encoder = nil;
        }
    }
    
    ~RenderScope()
    {
        close();
    }
private:
    id encoder;
};

- (void)drawAtlas:(nonnull id<MTLRenderCommandEncoder>)renderEncoder {
    // draw last since this changes pipeline state
    if (_showSettings->is3DView && _showSettings->atlas.empty())
        return;
    
    //if (!_showSettings->drawAtlas)
    //    return;
        
    RenderScope drawAtlasScope( renderEncoder, "DrawAtlas" );
    
    [renderEncoder setTriangleFillMode:MTLTriangleFillModeLines];
    [renderEncoder setDepthBias:5.0 slopeScale:0.0 clamp: 0.0];
    [renderEncoder setCullMode:MTLCullModeNone];
    
    [renderEncoder setRenderPipelineState:_pipelineStateDrawLines];
    
    // TODO: draw line strip with prim reset
    // need atlas data in push constants or in vb
    
    // TOOO: also need to hover name or show names on canvas
    
//                    [renderEncoder setVertexBytes:&uniformsLevel
//                                           length:sizeof(uniformsLevel)
//                                          atIndex:BufferIndexUniformsLevel];

    UniformsDebug uniformsDebug;
    
    for (const Atlas& atlas: _showSettings->atlas) {
        // not accounting for slice
        uniformsDebug.rect = float4m(atlas.x, atlas.y, atlas.w, atlas.h);
        
        
        [renderEncoder setVertexBytes:&uniformsDebug
                               length:sizeof(uniformsDebug)
                              atIndex:BufferIndexUniformsDebug];
        
        // this will draw diagonal
        for (MTKSubmesh* submesh in _mesh.submeshes) {
            [renderEncoder drawIndexedPrimitives:submesh.primitiveType
                                      indexCount:submesh.indexCount
                                       indexType:submesh.indexType
                                     indexBuffer:submesh.indexBuffer.buffer
                               indexBufferOffset:submesh.indexBuffer.offset];
        }
    }
    
    // restore state, even though this isn't a true state shadow
    [renderEncoder setCullMode:MTLCullModeBack];
    [renderEncoder setDepthBias:0.0 slopeScale:0.0 clamp:0.0];
    [renderEncoder setTriangleFillMode:MTLTriangleFillModeFill];
}

// want to run samples independent of redrawing the main view
- (void)drawSample
{
    if (_colorMap == nil) {
        return;
    }

    // this can occur during a resize
    if (!_lastDrawableTexture)
        return;

    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    if (!commandBuffer)
        return;

    commandBuffer.label = @"MyCommand";

    // this reads directly from compressed texture via a compute shader
    int32_t textureLookupX = _showSettings->textureLookupX;
    int32_t textureLookupY = _showSettings->textureLookupY;
    
    bool isDrawableBlit = _showSettings->isEyedropperFromDrawable();

    // TODO: only don't blit for plane + no debug or shape
    // otherwise want the pixel under the cursor, but this may include grid mixed
    // in and other debug overlays
    if (isDrawableBlit) {
        MTLOrigin srcOrigin =
            MTLOriginMake(_showSettings->cursorX, _showSettings->cursorY, 0);
        srcOrigin.x *= _showSettings->viewContentScaleFactor;
        srcOrigin.y *= _showSettings->viewContentScaleFactor;

        if ((srcOrigin.x >= 0 && srcOrigin.x < _lastDrawableTexture.width) &&
            (srcOrigin.y >= 0 && srcOrigin.y < _lastDrawableTexture.height)) {
            // Note: here we don't know the uv in original texture, would have to
            // write that out to another texture.  Also on shapes, texel may not
            // change but lighting might.

            // can simply blit the color out of the render buffer
            id<MTLBlitCommandEncoder> blitCommandEncoder =
                [commandBuffer blitCommandEncoder];
            if (blitCommandEncoder) {
                [blitCommandEncoder copyFromTexture:_lastDrawableTexture
                                        sourceSlice:0
                                        sourceLevel:0
                                       sourceOrigin:srcOrigin
                                         sourceSize:MTLSizeMake(1, 1, 1)
                                          toTexture:_sampleRenderTex
                                   destinationSlice:0
                                   destinationLevel:0
                                  destinationOrigin:MTLOriginMake(0, 0, 0)];
                [blitCommandEncoder synchronizeResource:_sampleRenderTex];
                [blitCommandEncoder endEncoding];
            }
        }
    }
    else {
        int32_t textureLookupMipX = _showSettings->textureLookupMipX;
        int32_t textureLookupMipY = _showSettings->textureLookupMipY;

        [self drawSamples:commandBuffer
                  lookupX:textureLookupMipX
                  lookupY:textureLookupMipY];

        // Synchronize the managed texture.
        id<MTLBlitCommandEncoder> blitCommandEncoder =
            [commandBuffer blitCommandEncoder];
        if (blitCommandEncoder) {
            [blitCommandEncoder synchronizeResource:_sampleComputeTex];
            [blitCommandEncoder endEncoding];
        }
    }

    // After synchonization, copy value back to the cpu
    id<MTLTexture> texture =
        isDrawableBlit ? _sampleRenderTex : _sampleComputeTex;

    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        if (buffer.error != nil) {
            return;
        }
        // only 1 pixel in the texture right now
        float4 data;

        // copy from texture back to CPU, might be easier using MTLBuffer.contents
        MTLRegion region = {
            {0, 0, 0},  // MTLOrigin
            {1, 1, 1}   // MTLSize
        };

        if (isDrawableBlit) {
            half4 data16f;
            [texture getBytes:&data16f bytesPerRow:8 fromRegion:region mipmapLevel:0];
            data = toFloat4(data16f);
        }
        else {
            [texture getBytes:&data bytesPerRow:16 fromRegion:region mipmapLevel:0];
        }

        // return the value at the sample
        self->_showSettings->textureResult = data;
        self->_showSettings->textureResultX = textureLookupX;
        self->_showSettings->textureResultY = textureLookupY;
        
        // TODO: This completed handler runs long after the hud has updated
        // so need to invalidate the hud.  So the pixel location is out of date.
        
        // printf("Color %f %f %f %f\n", data.x, data.y, data.z, data.w);
    }];

    [commandBuffer commit];
}

- (void)drawSamples:(id<MTLCommandBuffer>)commandBuffer
            lookupX:(int32_t)lookupX
            lookupY:(int32_t)lookupY
{
    // Final pass rendering code here
    id<MTLComputeCommandEncoder> renderEncoder =
        [commandBuffer computeCommandEncoder];
    if (!renderEncoder)
        return;

    renderEncoder.label = @"SampleCompute";

    RenderScope drawShapeScope( renderEncoder, "DrawShape" );
    
    UniformsCS uniforms;
    uniforms.uv.x = lookupX;
    uniforms.uv.y = lookupY;

    uniforms.face = _showSettings->faceNumber;
    uniforms.arrayOrSlice = _showSettings->arrayNumber;
    if (_showSettings->sliceNumber) {
        uniforms.arrayOrSlice = _showSettings->sliceNumber;
    }
    uniforms.mipLOD = _showSettings->mipNumber;

    // run compute here, don't need a shape
    switch (_colorMap.textureType) {
        case MTLTextureType1DArray:
            [renderEncoder setComputePipelineState:_pipelineState1DArrayCS];
            break;

        case MTLTextureType2D:
            [renderEncoder setComputePipelineState:_pipelineStateImageCS];
            break;

        case MTLTextureType2DArray:
            [renderEncoder setComputePipelineState:_pipelineStateImageArrayCS];
            break;

        case MTLTextureType3D:
            [renderEncoder setComputePipelineState:_pipelineStateVolumeCS];
            break;
        case MTLTextureTypeCube:
            [renderEncoder setComputePipelineState:_pipelineStateCubeCS];
            break;
        case MTLTextureTypeCubeArray:
            [renderEncoder setComputePipelineState:_pipelineStateCubeArrayCS];
            break;

        default:
            break;
    }

    // input and output texture
    [renderEncoder setTexture:_colorMap
                      atIndex:TextureIndexColor];

    [renderEncoder setTexture:_sampleComputeTex atIndex:TextureIndexSamples];

    [renderEncoder setBytes:&uniforms
                     length:sizeof(UniformsCS)
                    atIndex:BufferIndexUniformsCS];

    // sample and copy back pixels off the offset
    [renderEncoder dispatchThreads:MTLSizeMake(1, 1, 1)
             threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];

    [renderEncoder endEncoding];
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    // Don't crashing trying to readback from the cached drawable during a resize.
    _lastDrawableTexture = nil;

    /// Respond to drawable size or orientation changes here
    _showSettings->viewSizeX = size.width;
    _showSettings->viewSizeY = size.height;

    // TODO: only set this when size changes, but for now keep setting here and
    // adjust zoom
    CGFloat framebufferScale = view.window.screen.backingScaleFactor
                                   ? view.window.screen.backingScaleFactor
                                   : NSScreen.mainScreen.backingScaleFactor;

    _showSettings->viewContentScaleFactor = framebufferScale;

    _data->updateProjTransform();
    
#if USE_GLTF
    _gltfRenderer.drawableSize = size;
    _gltfRenderer.colorPixelFormat = view.colorPixelFormat;
    _gltfRenderer.depthStencilPixelFormat = view.depthStencilPixelFormat;
#endif
    
    _data->updateProjTransform();
}

#if USE_GLTF
// @protocol GLTFAssetLoadingDelegate
- (void)assetWithURL:(NSURL *)assetURL requiresContentsOfURL:(NSURL *)url completionHandler:(void (^)(NSData *_Nullable, NSError *_Nullable))completionHandler
{
    // This can handle remote assets
    NSURLSessionDataTask *task = [_urlSession dataTaskWithURL:url
                                                        completionHandler:^(NSData *data, NSURLResponse *response, NSError *error)
    {
        completionHandler(data, error);
    }];
    
    [task resume];
}

- (void)assetWithURL:(NSURL *)assetURL didFinishLoading:(GLTFAsset *)asset
{
    mylock lock(gModelLock);
    
    _asset = asset;
    
    _animationTime = 0.0;
    
    string fullFilename = assetURL.path.UTF8String;
    [self updateModelSettings:fullFilename];
}

- (void)assetWithURL:(NSURL *)assetURL didFailToLoadWithError:(NSError *)error;
{
    // TODO: display this error to the user
    NSLog(@"Asset load failed with error: %@", error);
}
#endif



@end
