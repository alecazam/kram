// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import "KramRenderer.h"

#import <TargetConditionals.h>

#if __has_feature(modules)
@import simd;
@import ModelIO;
#else
#import <simd/simd.h>
#import <ModelIO/ModelIO.h>
#endif

// Include header shared between C code here, which executes Metal API commands, and .metal files
#import "KramShaders.h"
#import "KramLoader.h"

#include "KTXImage.h"
#include "Kram.h"
#include "KramViewerBase.h"

static const NSUInteger MaxBuffersInFlight = 3;

using namespace kram;
using namespace simd;

@implementation Renderer
{
    dispatch_semaphore_t _inFlightSemaphore;
    id <MTLDevice> _device;
    id <MTLCommandQueue> _commandQueue;

    id <MTLBuffer> _dynamicUniformBuffer[MaxBuffersInFlight];
    
    id <MTLRenderPipelineState> _pipelineState1DArray;
    id <MTLRenderPipelineState> _pipelineStateImage;
    id <MTLRenderPipelineState> _pipelineStateImageArray;
    id <MTLRenderPipelineState> _pipelineStateCube;
    id <MTLRenderPipelineState> _pipelineStateCubeArray;
    id <MTLRenderPipelineState> _pipelineStateVolume;
    
    id <MTLComputePipelineState> _pipelineState1DArrayCS;
    id <MTLComputePipelineState> _pipelineStateImageCS;
    id <MTLComputePipelineState> _pipelineStateImageArrayCS;
    id <MTLComputePipelineState> _pipelineStateCubeCS;
    id <MTLComputePipelineState> _pipelineStateCubeArrayCS;
    id <MTLComputePipelineState> _pipelineStateVolumeCS;
    
    id <MTLDepthStencilState> _depthStateFull;
    id <MTLDepthStencilState> _depthStateNone;
   
    MTLVertexDescriptor *_mtlVertexDescriptor;

    // TODO: Array< id<MTLTexture> > _textures;
    id <MTLTexture> _colorMap;
    //id <MTLTexture> _colorMapView;
    
    id <MTLSamplerState> _colorMapSamplerWrap;
    id <MTLSamplerState> _colorMapSamplerClamp;
    
    id <MTLSamplerState> _colorMapSamplerBilinearWrap;
    id <MTLSamplerState> _colorMapSamplerBilinearClamp;
    
    //id<MTLTexture> _sampleRT;
    id<MTLTexture> _sampleTex;
    
    uint8_t _uniformBufferIndex;

    float4x4 _projectionMatrix;
    float4x4 _viewMatrix;
    float4x4 _modelMatrix;

    //float _rotation;
    KramLoader *_loader;
    MTKMesh *_mesh;
    
    
    ShowSettings* _showSettings;
}

-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view settings:(nonnull ShowSettings*)settings
{
    self = [super init];
    if(self)
    {
        _showSettings = settings;
        
        _device = view.device;
        
        _loader = [KramLoader new];
        _loader.device = _device;
        
        _inFlightSemaphore = dispatch_semaphore_create(MaxBuffersInFlight);
        [self _loadMetalWithView:view];
        [self _loadAssets];
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
    samplerDescriptor.label = @"colorMapSamplerWrap";
    
    _colorMapSamplerWrap = [_device newSamplerStateWithDescriptor:samplerDescriptor];
    
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.label = @"colorMapSamplerClamp";
   
    _colorMapSamplerClamp = [_device newSamplerStateWithDescriptor:samplerDescriptor];
    
    // these are for preview mode
    // use the mips, and specify linear for min/mag for SDF case
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;
    samplerDescriptor.label = @"colorMapSamplerBilinearClamp";
   
    _colorMapSamplerBilinearClamp = [_device newSamplerStateWithDescriptor:samplerDescriptor];
    
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.label = @"colorMapSamplerBilinearWrap";
    
    _colorMapSamplerBilinearWrap = [_device newSamplerStateWithDescriptor:samplerDescriptor];
}
    
- (void)_loadMetalWithView:(nonnull MTKView *)view
{
    /// Load Metal state objects and initialize renderer dependent view properties

    view.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    //view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB; // TODO: adjust this to draw srgb or not, prefer RGBA
    
    // have a mix of linear color and normals, don't want srgb conversion until displayed
    view.colorPixelFormat = MTLPixelFormatRGBA16Float;
    
    view.sampleCount = 1;

    _mtlVertexDescriptor = [[MTLVertexDescriptor alloc] init];

    _mtlVertexDescriptor.attributes[VertexAttributePosition].format = MTLVertexFormatFloat3;
    _mtlVertexDescriptor.attributes[VertexAttributePosition].offset = 0;
    _mtlVertexDescriptor.attributes[VertexAttributePosition].bufferIndex = BufferIndexMeshPositions;

    _mtlVertexDescriptor.attributes[VertexAttributeTexcoord].format = MTLVertexFormatFloat2;
    _mtlVertexDescriptor.attributes[VertexAttributeTexcoord].offset = 0;
    _mtlVertexDescriptor.attributes[VertexAttributeTexcoord].bufferIndex = BufferIndexMeshUV0;

    _mtlVertexDescriptor.layouts[BufferIndexMeshPositions].stride = 12;
    //_mtlVertexDescriptor.layouts[BufferIndexMeshPositions].stepRate = 1;
    //_mtlVertexDescriptor.layouts[BufferIndexMeshPositions].stepFunction = MTLVertexStepFunctionPerVertex;

    _mtlVertexDescriptor.layouts[BufferIndexMeshUV0].stride = 8;
    //_mtlVertexDescriptor.layouts[BufferIndexMeshUV0].stepRate = 1;
    //_mtlVertexDescriptor.layouts[BufferIndexMeshUV0].stepFunction = MTLVertexStepFunctionPerVertex;

    [self _createRenderPipelines:view];
    
    //-----------------------
   
    MTLDepthStencilDescriptor *depthStateDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStateDesc.depthCompareFunction = _showSettings->isReverseZ ? MTLCompareFunctionGreaterEqual : MTLCompareFunctionLessEqual;
    depthStateDesc.depthWriteEnabled = YES;
    _depthStateFull = [_device newDepthStencilStateWithDescriptor:depthStateDesc];

    depthStateDesc.depthCompareFunction = _showSettings->isReverseZ ? MTLCompareFunctionGreaterEqual : MTLCompareFunctionLessEqual;
    depthStateDesc.depthWriteEnabled = NO;
    _depthStateNone = [_device newDepthStencilStateWithDescriptor:depthStateDesc];
    
    for(NSUInteger i = 0; i < MaxBuffersInFlight; i++)
    {
        _dynamicUniformBuffer[i] = [_device newBufferWithLength:sizeof(Uniforms)
                                                        options:MTLResourceStorageModeShared];

        _dynamicUniformBuffer[i].label = @"UniformBuffer";
    }

    _commandQueue = [_device newCommandQueue];
    
    [self _createSamplers];
    
    //-----------------------
   
    [self _createComputePipelines];
   
    [self _createSampleRender];
}

- (void)_createComputePipelines
{
    id<MTLLibrary> defaultLibrary = [_device newDefaultLibrary];

    NSError *error = NULL;
    id<MTLFunction> computeFunction;
    
    //-----------------------
   
    computeFunction = [defaultLibrary newFunctionWithName:@"SampleImageCS"];
    _pipelineStateImageCS = [_device newComputePipelineStateWithFunction:computeFunction error:&error];
    if (!_pipelineStateImageCS)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }
    
    computeFunction = [defaultLibrary newFunctionWithName:@"SampleImageArrayCS"];
    _pipelineStateImageArrayCS = [_device newComputePipelineStateWithFunction:computeFunction error:&error];
    if (!_pipelineStateImageArrayCS)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }
    
    computeFunction = [defaultLibrary newFunctionWithName:@"SampleVolumeCS"];
    _pipelineStateVolumeCS = [_device newComputePipelineStateWithFunction:computeFunction error:&error];
    if (!_pipelineStateVolumeCS)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }
    
    computeFunction = [defaultLibrary newFunctionWithName:@"SampleCubeCS"];
    _pipelineStateCubeCS = [_device newComputePipelineStateWithFunction:computeFunction error:&error];
    if (!_pipelineStateCubeCS)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }
    
    computeFunction = [defaultLibrary newFunctionWithName:@"SampleCubeArrayCS"];
    _pipelineStateCubeArrayCS = [_device newComputePipelineStateWithFunction:computeFunction error:&error];
    if (!_pipelineStateCubeArrayCS)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }
    
    computeFunction = [defaultLibrary newFunctionWithName:@"SampleImage1DArrayCS"];
    _pipelineState1DArrayCS = [_device newComputePipelineStateWithFunction:computeFunction error:&error];
    if (!_pipelineState1DArrayCS)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }
}

- (void)_createRenderPipelines:(MTKView*)view
{
    id<MTLLibrary> defaultLibrary = [_device newDefaultLibrary];

    id <MTLFunction> vertexFunction;
    id <MTLFunction> fragmentFunction;
    
    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = @"DrawImagePipeline";
    pipelineStateDescriptor.sampleCount = view.sampleCount;
    pipelineStateDescriptor.vertexDescriptor = _mtlVertexDescriptor;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    
    // TODO: could drop these for images, but want a 3D preview of content
    // or might make these memoryless.
    pipelineStateDescriptor.depthAttachmentPixelFormat = view.depthStencilPixelFormat;
    pipelineStateDescriptor.stencilAttachmentPixelFormat = view.depthStencilPixelFormat;

    NSError *error = NULL;
    
    //-----------------------
   
    vertexFunction = [defaultLibrary newFunctionWithName:@"DrawImageVS"];
    fragmentFunction = [defaultLibrary newFunctionWithName:@"DrawImagePS"];
    pipelineStateDescriptor.vertexFunction = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    
    _pipelineStateImage = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineStateImage)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }

    //-----------------------
   
    vertexFunction = [defaultLibrary newFunctionWithName:@"DrawImageVS"]; // reused
    fragmentFunction = [defaultLibrary newFunctionWithName:@"DrawImageArrayPS"];
    pipelineStateDescriptor.vertexFunction = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    
    _pipelineStateImageArray = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineStateImageArray)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }

    //-----------------------
   
    vertexFunction = [defaultLibrary newFunctionWithName:@"DrawImageVS"];
    fragmentFunction = [defaultLibrary newFunctionWithName:@"Draw1DArrayPS"];
    pipelineStateDescriptor.vertexFunction = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    
    _pipelineState1DArray = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineState1DArray)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }
    
    //-----------------------
   
    vertexFunction = [defaultLibrary newFunctionWithName:@"DrawCubeVS"];
    fragmentFunction = [defaultLibrary newFunctionWithName:@"DrawCubePS"];
    pipelineStateDescriptor.vertexFunction = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    
    _pipelineStateCube = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineStateCube)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }
    
    //-----------------------
   
    vertexFunction = [defaultLibrary newFunctionWithName:@"DrawCubeVS"]; // reused
    fragmentFunction = [defaultLibrary newFunctionWithName:@"DrawCubeArrayPS"];
    pipelineStateDescriptor.vertexFunction = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    
    _pipelineStateCubeArray = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineStateCubeArray)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }
    
    //-----------------------
    
    vertexFunction = [defaultLibrary newFunctionWithName:@"DrawVolumeVS"];
    fragmentFunction = [defaultLibrary newFunctionWithName:@"DrawVolumePS"];
    pipelineStateDescriptor.vertexFunction = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    
    _pipelineStateVolume = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    if (!_pipelineStateVolume)
    {
        NSLog(@"Failed to create pipeline state, error %@", error);
    }
}

- (void)_createSampleRender
{
    // writing to this texture
    MTLTextureDescriptor* textureDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:1 height:1 mipmapped:NO];
    
    textureDesc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
    textureDesc.storageMode = MTLStorageModeManaged;
    _sampleTex = [_device newTextureWithDescriptor:textureDesc];
}

- (void)_loadAssets
{
    /// Load assets into metal objects

    NSError *error = nil;

    MTKMeshBufferAllocator *metalAllocator = [[MTKMeshBufferAllocator alloc]
                                              initWithDevice: _device];

#if 1 // TODO: replace box with fsq or fst, or use thin box for perspective/rotation
    MDLMesh *mdlMesh = [MDLMesh newBoxWithDimensions:(vector_float3){1, 1, 1}
                                            segments:(vector_uint3){1, 1, 1}
                                        geometryType:MDLGeometryTypeTriangles
                                       inwardNormals:NO
                                           allocator:metalAllocator];
    
#endif
    
    MDLVertexDescriptor *mdlVertexDescriptor =
    MTKModelIOVertexDescriptorFromMetal(_mtlVertexDescriptor);

    mdlVertexDescriptor.attributes[VertexAttributePosition].name  = MDLVertexAttributePosition;
    mdlVertexDescriptor.attributes[VertexAttributeTexcoord].name  = MDLVertexAttributeTextureCoordinate;

    mdlMesh.vertexDescriptor = mdlVertexDescriptor;

    _mesh = [[MTKMesh alloc] initWithMesh:mdlMesh
                                   device:_device
                                    error:&error];
    _mesh.name = @"BoxMesh";
    
    if(!_mesh || error)
    {
        NSLog(@"Error creating MetalKit mesh %@", error.localizedDescription);
    }
}

- (BOOL)loadTextureFromData:(const string&)fullFilename timestamp:(double)timestamp imageData:(nonnull const uint8_t*)imageData imageDataLength:(uint64_t)imageDataLength
{
    // image can be decoded to rgba8u if platform can't display format natively
    // but still want to identify blockSize from original format
    bool isTextureChanged =
        (fullFilename != _showSettings->lastFilename) ||
        (timestamp != _showSettings->lastTimestamp);
    
    if (isTextureChanged) {
        // synchronously cpu upload from ktx file to texture
        MTLPixelFormat originalFormatMTL = MTLPixelFormatInvalid;
        id<MTLTexture> texture = [_loader loadTextureFromData:imageData imageDataLength:imageDataLength originalFormat:&originalFormatMTL];
        if (!texture) {
            return NO;
        }
        
        // archive shouldn't contain png, so only support ktx/ktx2 here
        // TODO: have loader return KTXImage instead of parsing it again
        // then can decode blocks in kramv
        
        KTXImage sourceImage;
        if (!sourceImage.open(imageData,imageDataLength)) {
            return NO;
        }
       
        _showSettings->imageInfo = kramInfoKTXToString(fullFilename, sourceImage, false);
        _showSettings->imageInfoVerbose = kramInfoKTXToString(fullFilename, sourceImage, true);
       
        _showSettings->originalFormat = (MyMTLPixelFormat)originalFormatMTL;
        
        _showSettings->lastFilename = fullFilename;
        _showSettings->lastTimestamp = timestamp;
        
        @autoreleasepool {
            _colorMap = texture;
        }
    }
    
    return [self loadTextureImpl:fullFilename isTextureChanged:isTextureChanged];
}
    
- (BOOL)loadTexture:(nonnull NSURL *)url
{
    string fullFilename = [url.path UTF8String];
    
    // can use this to pull, or use fstat on FileHelper
    NSDate *fileDate = nil;
    NSError *error = nil;
    [url getResourceValue:&fileDate forKey:NSURLContentModificationDateKey error:&error];
    
    // DONE: tie this to url and modstamp differences
    double timestamp = fileDate.timeIntervalSince1970;
    bool isTextureChanged =
        (fullFilename != _showSettings->lastFilename) ||
        (timestamp != _showSettings->lastTimestamp);
    
    // image can be decoded to rgba8u if platform can't display format natively
    // but still want to identify blockSize from original format
    if (isTextureChanged) {
        // synchronously cpu upload from ktx file to texture
        MTLPixelFormat originalFormatMTL = MTLPixelFormatInvalid;
        id<MTLTexture> texture = [_loader loadTextureFromURL:url originalFormat:&originalFormatMTL];
        if (!texture) {
            return NO;
        }
        
        _showSettings->imageInfo = kramInfoToString(fullFilename, false);
        _showSettings->imageInfoVerbose = kramInfoToString(fullFilename, true);
        
        _showSettings->originalFormat = (MyMTLPixelFormat)originalFormatMTL;
        _showSettings->decodedFormat = (MyMTLPixelFormat)texture.pixelFormat;
        
        _showSettings->lastFilename = fullFilename;
        _showSettings->lastTimestamp = timestamp;
        
        @autoreleasepool {
            _colorMap = texture;
        }
    }
    
    return [self loadTextureImpl:fullFilename isTextureChanged:isTextureChanged];
}



- (BOOL)loadTextureImpl:(const string&)fullFilename isTextureChanged:(BOOL)isTextureChanged
{
    if (isTextureChanged) {
        Int2 blockDims = blockDimsOfFormat(_showSettings->originalFormat);
        _showSettings->blockX = blockDims.x;
        _showSettings->blockY = blockDims.y;
    }
    
    id<MTLTexture> texture = _colorMap;
    
    MyMTLPixelFormat format = (MyMTLPixelFormat)texture.pixelFormat;
    MyMTLPixelFormat originalFormat = _showSettings->originalFormat;
    
    // based on original or transcode?
    _showSettings->isSigned = isSignedFormat(format);
    
    // need a way to get at KTXImage, but would need to keep mmap alive
    // this doesn't handle normals that are ASTC, so need more data from loader
    string fullFilenameCopy = fullFilename;

    // this is so unreadable
    string filename = toLower(fullFilenameCopy);

    // could cycle between rrr1 and r001.
    int32_t numChannels = numChannelsOfFormat(originalFormat);
    
    // set title to filename, chop this to just file+ext, not directory
    string filenameShort = filename;
    const char* filenameSlash = strrchr(filenameShort.c_str(), '/');
    if (filenameSlash != nullptr) {
        filenameShort = filenameSlash + 1;
    }
    
    // now chop off the extension
    filenameShort = filenameShort.substr(0, filenameShort.find_last_of("."));
    
    bool isAlbedo = false;
    bool isNormal = false;
    bool isSDF = false;
    
    // note that decoded textures are 3/4 channel even though they are normal/sdf originally, so test those first
    if (numChannels == 2 || endsWith(filenameShort, "-n") || endsWith(filenameShort, "_normal")) {
        isNormal = true;
    }
    else if (numChannels == 1 || endsWith(filenameShort, "-sdf")) {
        isSDF = true;
    }
    else if (numChannels == 3 || numChannels == 4 || endsWith(filenameShort, "-a") || endsWith(filenameShort, "_basecolor")) {
        isAlbedo = true;
    }
    
    _showSettings->isNormal = isNormal;
    _showSettings->isSDF = isSDF;
    
    // textures are already premul, so don't need to premul in shader
    // should really have 3 modes, unmul, default, premul
    _showSettings->isPremul = false;
    if (isAlbedo && endsWithExtension(filename.c_str(), ".png")) {
        _showSettings->isPremul = true; // convert to premul in shader, so can see other channels
    }
        
    if (isNormal || isSDF) {
        _showSettings->isPremul = false;
    }
        
    _showSettings->numChannels = numChannels;
    
    // TODO: identify if texture holds normal data from the props
    // have too many 4 channel normals that shouldn't swizzle like this
    // kramTextures.py is using etc2rg on iOS for now, and not astc.
    
    _showSettings->isSwizzleAGToRG = false;

//    if (isASTCFormat(originalFormat) && isNormal) {
//        // channels after = "ag01"
//        _showSettings->isSwizzleAGToRG = true;
//    }
        
    // then can manipulate this after loading
    _showSettings->mipLOD = 0;
    _showSettings->faceNumber = 0;
    _showSettings->arrayNumber = 0;
    _showSettings->sliceNumber = 0;
    
    // can derive these from texture queries
    _showSettings->maxLOD = (int32_t)texture.mipmapLevelCount;
    _showSettings->faceCount = (texture.textureType == MTLTextureTypeCube ||
                               texture.textureType == MTLTextureTypeCubeArray) ? 6 : 0;
    _showSettings->arrayCount = (int32_t)texture.arrayLength;
    _showSettings->sliceCount = (int32_t)texture.depth;
    
    _showSettings->channels = TextureChannels::ModeRGBA;
    
    _showSettings->imageBoundsX = (int32_t)texture.width;
    _showSettings->imageBoundsY = (int32_t)texture.height;
    
    [self updateViewTransforms];
    
    // this controls viewMatrix (global to all visible textures)
    _showSettings->panX = 0.0f;
    _showSettings->panY = 0.0f;
    
    _showSettings->zoom = _showSettings->zoomFit;
    
    // wish could keep existing setting, but new texture might not
    // be supported debugMode for new texture
    _showSettings->debugMode = DebugMode::DebugModeNone;
    
    // have one of these for each texture added to the viewer
    float scaleX = MAX(1, texture.width);
    float scaleY = MAX(1, texture.height);
    _modelMatrix = float4x4(float4m(scaleX, scaleY, 1.0f, 1.0f));
    _modelMatrix = _modelMatrix * matrix4x4_translation(0.0f, 0.0f, -1.0);
    
    return YES;
}

- (float4x4)computeImageTransform:(float)panX panY:(float)panY zoom:(float)zoom {
    // translate
    float4x4 panTransform = matrix4x4_translation(-panX, panY, 0.0);
    
    // scale
    float4x4 viewMatrix = float4x4(float4m(zoom, zoom, 1.0f, 1.0f));
    viewMatrix = panTransform * viewMatrix;
    
    return _projectionMatrix * viewMatrix * _modelMatrix;
}

- (void)_updateGameState
{
    /// Update any game state before encoding rendering commands to our drawable

    Uniforms& uniforms = *(Uniforms*)_dynamicUniformBuffer[_uniformBufferIndex].contents;

    uniforms.isNormal = _showSettings->isNormal;
    uniforms.isPremul = _showSettings->isPremul;
    uniforms.isSigned = _showSettings->isSigned;
    uniforms.isSwizzleAGToRG = _showSettings->isSwizzleAGToRG;
    
    uniforms.isSDF = _showSettings->isSDF;
    uniforms.numChannels = _showSettings->numChannels;
    
    MyMTLTextureType textureType = MyMTLTextureType2D;
    MyMTLPixelFormat textureFormat = MyMTLPixelFormatInvalid;
    if (_colorMap) {
        textureType = (MyMTLTextureType)_colorMap.textureType;
        textureFormat = (MyMTLPixelFormat)_colorMap.pixelFormat;
    }
    
    uniforms.isCheckerboardShown = _showSettings->isCheckerboardShown;
    bool canWrap = true;
    if (textureType == MyMTLTextureTypeCube || textureType == MyMTLTextureTypeCubeArray) {
        canWrap = false;
    }
    
    uniforms.isWrap = canWrap ? _showSettings->isWrap : false;
    
    uniforms.isPreview = _showSettings->isPreview;
    
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
    
    // no debug mode when preview kicks on, make it possible to toggle back and forth more easily
    uniforms.debugMode = _showSettings->isPreview ? ShaderDebugMode::ShDebugModeNone : (ShaderDebugMode)_showSettings->debugMode;
    uniforms.channels = (ShaderTextureChannels)_showSettings->channels;

    // translate
    float4x4 panTransform = matrix4x4_translation(-_showSettings->panX, _showSettings->panY, 0.0);
    
    // scale
    _viewMatrix = float4x4(float4m(_showSettings->zoom, _showSettings->zoom, 1.0f, 1.0f));
    _viewMatrix = panTransform * _viewMatrix;
    
    // viewMatrix should typically be the inverse
    //_viewMatrix = simd_inverse(_viewMatrix);
    
    float4x4 projectionViewMatrix = _projectionMatrix * _viewMatrix;
    
    uniforms.projectionViewMatrix = projectionViewMatrix;

    // works when only one texture, but switch to projectViewMatrix
    uniforms.modelMatrix = _modelMatrix;
    
    // this was stored so view could use it, but now that code calcs the transform via computeImageTransform
    _showSettings->projectionViewModelMatrix = projectionViewMatrix * _modelMatrix;
    
   
    
    //_rotation += .01;
}

- (void)_setUniformsLevel:(UniformsLevel&)uniforms mipLOD:(int32_t)mipLOD
{
    uniforms.mipLOD = mipLOD;
    
    uniforms.arrayOrSlice = 0;
    uniforms.face  = 0;

    MyMTLTextureType textureType = MyMTLTextureType2D;
    if (_colorMap) {
        textureType = (MyMTLTextureType)_colorMap.textureType;
    }
    
    // TODO: set texture specific uniforms, but using single _colorMap for now
    switch(textureType) {
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
        /// Per frame updates here

        // TODO: move this out, needs to get called off mouseMove, but don't want to call drawMain
        [self drawSample];
        
        dispatch_semaphore_wait(_inFlightSemaphore, DISPATCH_TIME_FOREVER);

        _uniformBufferIndex = (_uniformBufferIndex + 1) % MaxBuffersInFlight;

        id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
        commandBuffer.label = @"MyCommand";

        __block dispatch_semaphore_t block_sema = _inFlightSemaphore;
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> /* buffer */)
         {
             dispatch_semaphore_signal(block_sema);
         }];

        [self _updateGameState];
        
        // use to autogen mipmaps if needed, might eliminate this since it's always box filter
        // TODO: do mips via kram instead, but was useful for pow-2 mip comparisons.
        
        // also use to readback pixels
        // also use for async texture upload
        bool needsBlit = _loader.isMipgenNeeded && _colorMap.mipmapLevelCount > 1;
        if (needsBlit) {
            id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
            blitEncoder.label = @"MyBlitEncoder";
            
            // autogen mips will include srgb conversions, so toggling srgb on/off isn't quite correct
            if (_loader.mipgenNeeded) {
                [blitEncoder generateMipmapsForTexture:_colorMap];
                
                _loader.mipgenNeeded = NO;
            }
        
            [blitEncoder endEncoding];
        }
        
        
        [self drawMain:commandBuffer view:view];
        
        [commandBuffer presentDrawable:view.currentDrawable];
        [commandBuffer commit];
    }
}

- (void)drawMain:(id<MTLCommandBuffer>)commandBuffer view:(nonnull MTKView *)view {
    /// Delay getting the currentRenderPassDescriptor until absolutely needed. This avoids
    ///   holding onto the drawable and blocking the display pipeline any longer than necessary
    MTLRenderPassDescriptor* renderPassDescriptor = view.currentRenderPassDescriptor;

    if (renderPassDescriptor == nil) {
        return;
    }
    if (_colorMap == nil) {
        // this will clear target
        id<MTLRenderCommandEncoder> renderEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"MainRender";
        [renderEncoder endEncoding];
        
        return;
    }
    
    /// Final pass rendering code here
    id<MTLRenderCommandEncoder> renderEncoder =
    [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    renderEncoder.label = @"MainRender";

    // set raster state
    [renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [renderEncoder setCullMode:MTLCullModeBack];
    [renderEncoder setDepthStencilState:_depthStateFull];

    [renderEncoder pushDebugGroup:@"DrawBox"];

    // set the mesh shape
    for (NSUInteger bufferIndex = 0; bufferIndex < _mesh.vertexBuffers.count; bufferIndex++)
    {
        MTKMeshBuffer *vertexBuffer = _mesh.vertexBuffers[bufferIndex];
        if((NSNull*)vertexBuffer != [NSNull null])
        {
            [renderEncoder setVertexBuffer:vertexBuffer.buffer
                                    offset:vertexBuffer.offset
                                   atIndex:bufferIndex];
        }
    }

    //for (texture in _textures) // TODO: setup
    //if (_colorMap)
    {
        // TODO: set texture specific uniforms, but using single _colorMap for now
        bool canWrap = true;
        
        switch(_colorMap.textureType) {
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
                canWrap = false;
                
                break;
            case MTLTextureTypeCubeArray:
                canWrap = false;
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
        id<MTLTexture> texture = _colorMap;
        [renderEncoder setFragmentTexture:texture
                                  atIndex:TextureIndexColor];

        
        
        UniformsLevel uniformsLevel;
        uniformsLevel.drawOffset = float2m(0.0f);
        
        if (_showSettings->isPreview) {
            // upload this on each face drawn, since want to be able to draw all mips/levels at once
            [self _setUniformsLevel:uniformsLevel mipLOD:_showSettings->mipLOD];
            
            [renderEncoder setVertexBytes:&uniformsLevel
                                    length:sizeof(uniformsLevel)
                                   atIndex:BufferIndexUniformsLevel];

            [renderEncoder setFragmentBytes:&uniformsLevel
                                      length:sizeof(uniformsLevel)
                                     atIndex:BufferIndexUniformsLevel];
            
            // use exisiting lod, and mip
            [renderEncoder setFragmentSamplerState:
                                  (canWrap && _showSettings->isWrap) ? _colorMapSamplerBilinearWrap : _colorMapSamplerBilinearClamp
                                  atIndex:SamplerIndexColor];
            
            for(MTKSubmesh *submesh in _mesh.submeshes)
            {
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
            //int32_t d = _colorMap.depth;
                        
            MyMTLTextureType textureType = MyMTLTextureType2D;
            if (_colorMap) {
                textureType = (MyMTLTextureType)_colorMap.textureType;
            }
            
            bool isCube = false;
            if (textureType == MyMTLTextureTypeCube || textureType == MyMTLTextureTypeCubeArray) {
                isCube = true;
            }
            
            // gap the contact sheet, note this 2 pixels is scaled on small textures by the zoom
            int32_t gap = _showSettings->showAllPixelGap; // * _showSettings->viewContentScaleFactor;
            
            for (int32_t mip = 0; mip < _showSettings->maxLOD; ++mip) {
                
                // upload this on each face drawn, since want to be able to draw all mips/levels at once
                [self _setUniformsLevel:uniformsLevel mipLOD:mip];
                
                if (mip == 0) {
                    uniformsLevel.drawOffset.y = 0.0f;
                }
                else {
                    // all mips draw at top mip size currently
                    uniformsLevel.drawOffset.y -= h + gap;
                }
                
                // this its ktxImage.totalChunks()
                int32_t numLevels =  _showSettings->totalChunks();
                
                for (int32_t level = 0; level < numLevels; ++level) {
                    
                    if (isCube) {
                        uniformsLevel.face = level % 6;
                        uniformsLevel.arrayOrSlice = level / 6;
                    }
                    else {
                        uniformsLevel.arrayOrSlice = level;
                    }
                    
                    // advance x across faces/slices/array elements, 1d array and 2d thin array are weird though.
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
                    [renderEncoder setFragmentSamplerState:
                                          (canWrap && _showSettings->isWrap) ? _colorMapSamplerWrap : _colorMapSamplerClamp
                                          lodMinClamp:mip
                                          lodMaxClamp:mip + 1
                                          atIndex:SamplerIndexColor];
                

                    // TODO: since this isn't a preview, have mode to display all faces and mips on on screen
                    // faces and arrays and slices go across in a row,
                    // and mips are displayed down from each of those in a column
                    
                    for(MTKSubmesh *submesh in _mesh.submeshes)
                    {
                        [renderEncoder drawIndexedPrimitives:submesh.primitiveType
                                                  indexCount:submesh.indexCount
                                                   indexType:submesh.indexType
                                                 indexBuffer:submesh.indexBuffer.buffer
                                           indexBufferOffset:submesh.indexBuffer.offset];
                    }
                }
            }
        }
        else {
            int32_t mip = _showSettings->mipLOD;
            
            // upload this on each face drawn, since want to be able to draw all mips/levels at once
            [self _setUniformsLevel:uniformsLevel mipLOD:mip];
            
            [renderEncoder setVertexBytes:&uniformsLevel
                                    length:sizeof(uniformsLevel)
                                   atIndex:BufferIndexUniformsLevel];

            [renderEncoder setFragmentBytes:&uniformsLevel
                                      length:sizeof(uniformsLevel)
                                     atIndex:BufferIndexUniformsLevel];
            
            // force lod, and don't mip
            [renderEncoder setFragmentSamplerState:
                                  (canWrap && _showSettings->isWrap) ? _colorMapSamplerWrap : _colorMapSamplerClamp
                                  lodMinClamp:mip
                                  lodMaxClamp:mip + 1
                                  atIndex:SamplerIndexColor];
        

            // TODO: since this isn't a preview, have mode to display all faces and mips on on screen
            // faces and arrays and slices go across in a row,
            // and mips are displayed down from each of those in a column
            
            for(MTKSubmesh *submesh in _mesh.submeshes)
            {
                [renderEncoder drawIndexedPrimitives:submesh.primitiveType
                                          indexCount:submesh.indexCount
                                           indexType:submesh.indexType
                                         indexBuffer:submesh.indexBuffer.buffer
                                   indexBufferOffset:submesh.indexBuffer.offset];
            }
        }
    }
    
    [renderEncoder popDebugGroup];

    [renderEncoder endEncoding];
    
    // TODO: run any post-processing on each texture visible as fsw
    // TODO: environment map preview should be done as fsq
}

// want to run samples independent of redrawing the main view
- (void)drawSample
{
    // Note: this is failing when running via Cmake
    bool doSample = true;
    if (!doSample) {
        return;
    }
    if (_colorMap == nil) {
        return;
    }
    
    id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    commandBuffer.label = @"MyCommand";

    int32_t textureLookupX = _showSettings->textureLookupX;
    int32_t textureLookupY = _showSettings->textureLookupY;
    
    int32_t textureLookupMipX = _showSettings->textureLookupMipX;
    int32_t textureLookupMipY = _showSettings->textureLookupMipY;
    
    [self drawSamples:commandBuffer lookupX:textureLookupMipX lookupY:textureLookupMipY];
    
    // Synchronize the managed texture.
    id <MTLBlitCommandEncoder> blitCommandEncoder = [commandBuffer blitCommandEncoder];
    [blitCommandEncoder synchronizeResource:_sampleTex];
    [blitCommandEncoder endEncoding];

    // After synchonization, copy value back to the cpu
    id<MTLTexture> texture = _sampleTex;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> /* buffer */)
    {
        // only 1 pixel in the texture right now
        float4 data;
        
        // copy from texture back to CPU, might be easier using MTLBuffer.contents
        MTLRegion region = {
            { 0, 0, 0 }, // MTLOrigin
            { 1, 1, 1 }  // MTLSize
        };
        
        [texture getBytes:&data bytesPerRow:16 fromRegion:region mipmapLevel:0];
        
        // return the value at the sample
        _showSettings->textureResult = data;
        _showSettings->textureResultX = textureLookupX;
        _showSettings->textureResultY = textureLookupY;
        
        //printf("Color %f %f %f %f\n", data.x, data.y, data.z, data.w);
    }];
    
    [commandBuffer commit];
}


- (void)drawSamples:(id<MTLCommandBuffer>)commandBuffer lookupX:(int32_t)lookupX lookupY:(int32_t)lookupY {
    
    // Final pass rendering code here
    id<MTLComputeCommandEncoder> renderEncoder = [commandBuffer computeCommandEncoder];
    renderEncoder.label = @"SampleCompute";

    [renderEncoder pushDebugGroup:@"DrawBox"];

    UniformsCS uniforms;
    uniforms.uv.x = lookupX;
    uniforms.uv.y = lookupY;
    
    uniforms.face = _showSettings->faceNumber;
    uniforms.arrayOrSlice = _showSettings->arrayNumber;
    if (_showSettings->sliceNumber) {
        uniforms.arrayOrSlice = _showSettings->sliceNumber;
    }
    uniforms.mipLOD = _showSettings->mipLOD;
    
    // run compute here, don't need a shape
    switch(_colorMap.textureType) {
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
    
    [renderEncoder setTexture:_sampleTex
                      atIndex:TextureIndexSamples];
    
    [renderEncoder setBytes:&uniforms length:sizeof(UniformsCS) atIndex:BufferIndexUniformsCS];
    
    // sample and copy back pixels off the offset
    [renderEncoder dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
    
    [renderEncoder popDebugGroup];
    [renderEncoder endEncoding];
}


- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    /// Respond to drawable size or orientation changes here
    _showSettings->viewSizeX = size.width;
    _showSettings->viewSizeY = size.height;
    
    // TODO: only set this when size changes, but for now keep setting here and adjust zoom
    CGFloat framebufferScale = view.window.screen.backingScaleFactor ? view.window.screen.backingScaleFactor : NSScreen.mainScreen.backingScaleFactor;
    
    _showSettings->viewContentScaleFactor = framebufferScale;
    
    [self updateViewTransforms];
}

- (void)updateViewTransforms {
    
    //float aspect = size.width / (float)size.height;
    //_projectionMatrix = perspective_rhs(45.0f * (M_PI / 180.0f), aspect, 0.1f, 100.0f);
    _projectionMatrix = orthographic_rhs(_showSettings->viewSizeX, _showSettings->viewSizeY, 0.1f, 100.0f, _showSettings->isReverseZ);
    
    // DONE: adjust zoom to fit the entire image to the window
    _showSettings->zoomFit = MIN((float)_showSettings->viewSizeX,  (float)_showSettings->viewSizeY) /
        MAX(1, MAX((float)_showSettings->imageBoundsX, (float)_showSettings->imageBoundsY));
    
    // already using drawableSize which includes scale
    // TODO: remove contentScaleFactor of view, this can be 1.0 to 2.0f
    // why does this always report 2x even when I change monitor res.
    //_showSettings->zoomFit /= _showSettings->viewContentScaleFactor;
}


@end
