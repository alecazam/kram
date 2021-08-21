// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#import "KramRenderer.h"

#import <TargetConditionals.h>

#import <ModelIO/ModelIO.h>

// Include header shared between C code here, which executes Metal API commands, and .metal files
#import "KramShaders.h"
#import "KramLoader.h"

#include "KramViewerBase.h"

static const NSUInteger MaxBuffersInFlight = 3;

using namespace kram;
using namespace simd;

// Capture what we need to build the renderPieplines, without needing view
struct ViewFramebufferData {
    MTLPixelFormat colorPixelFormat = MTLPixelFormatInvalid;
    MTLPixelFormat depthStencilPixelFormat = MTLPixelFormatInvalid;
    uint32_t sampleCount = 0;
};

@implementation Renderer
{
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
    
    id<MTLComputePipelineState> _pipelineState1DArrayCS;
    id<MTLComputePipelineState> _pipelineStateImageCS;
    id<MTLComputePipelineState> _pipelineStateImageArrayCS;
    id<MTLComputePipelineState> _pipelineStateCubeCS;
    id<MTLComputePipelineState> _pipelineStateCubeArrayCS;
    id<MTLComputePipelineState> _pipelineStateVolumeCS;
    
    id<MTLDepthStencilState> _depthStateFull;
    id<MTLDepthStencilState> _depthStateNone;
   
    MTLVertexDescriptor *_mtlVertexDescriptor;

    // TODO: Array< id<MTLTexture> > _textures;
    id<MTLTexture> _colorMap;
    id<MTLTexture> _normalMap;
    id<MTLTexture> _lastDrawableTexture;
    
    // border is a better edge sample, but at edges it filters in the transparent color
    // around the border which is undesirable.  It would be better if the hw did
    // clamp to edge until uv outside 0 to 1.  This results in having to inset the uv by 0.5 px
    // to avoid this artifact, but on small texturs that are 4x4, a 1 px inset is noticeable.
    
    id<MTLSamplerState> _colorMapSamplerNearestWrap;
    id<MTLSamplerState> _colorMapSamplerNearestBorder;
    id<MTLSamplerState> _colorMapSamplerNearestEdge;
    
    id<MTLSamplerState> _colorMapSamplerFilterWrap;
    id<MTLSamplerState> _colorMapSamplerFilterBorder;
    id<MTLSamplerState> _colorMapSamplerFilterEdge;
    
    //id<MTLTexture> _sampleRT;
    id<MTLTexture> _sampleComputeTex;
    id<MTLTexture> _sampleRenderTex;
   
    uint8_t _uniformBufferIndex;

    float4x4 _projectionMatrix;
    
    // 2d versions
    float4x4 _viewMatrix;
    float4x4 _modelMatrix;
    
    // 3d versions
    float4x4 _viewMatrix3D;
    float4x4 _modelMatrix3D;

    //float _rotation;
    KramLoader *_loader;
    MTKMesh *_mesh;
    
    MDLVertexDescriptor *_mdlVertexDescriptor;
    
    //MTKMesh *_meshPlane; // really a thin gox
    MTKMesh *_meshBox;
    MTKMesh *_meshSphere;
    MTKMesh *_meshSphereMirrored;
    //MTKMesh *_meshCylinder;
    MTKMesh *_meshCapsule;
    MTKMeshBufferAllocator *_metalAllocator;
    
    id<MTLLibrary> _shaderLibrary;
    NSURL* _metallibFileURL;
    NSDate* _metallibFileDate;
    ViewFramebufferData _viewFramebuffer;
    
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
        
        _metalAllocator = [[MTKMeshBufferAllocator alloc] initWithDevice: _device];

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
    samplerDescriptor.label = @"colorMapSamplerNearestWrap";
    
    _colorMapSamplerNearestWrap = [_device newSamplerStateWithDescriptor:samplerDescriptor];
    
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.label = @"colorMapSamplerNearestBorder";
   
    _colorMapSamplerNearestBorder = [_device newSamplerStateWithDescriptor:samplerDescriptor];
    
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.label = @"colorMapSamplerNearsetEdge";
    
    _colorMapSamplerNearestEdge = [_device newSamplerStateWithDescriptor:samplerDescriptor];
    
    // -----
    
    // these are for preview mode
    // use the mips, and specify linear for min/mag for SDF case
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterLinear;
    samplerDescriptor.maxAnisotropy = 4; // 1,2,4,8,16 are choices
    
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToBorderColor;
    samplerDescriptor.label = @"colorMapSamplerFilterBorder";
   
    _colorMapSamplerFilterBorder = [_device newSamplerStateWithDescriptor:samplerDescriptor];
    
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.label = @"colorMapSamplerFilterEdge";
   
    _colorMapSamplerFilterEdge = [_device newSamplerStateWithDescriptor:samplerDescriptor];
    
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeRepeat;
    samplerDescriptor.label = @"colorMapSamplerBilinearWrap";
    
    _colorMapSamplerFilterWrap = [_device newSamplerStateWithDescriptor:samplerDescriptor];
}
    
- (void)_createVertexDescriptor
{
    _mtlVertexDescriptor = [[MTLVertexDescriptor alloc] init];

    _mtlVertexDescriptor.attributes[VertexAttributePosition].format = MTLVertexFormatFloat3;
    _mtlVertexDescriptor.attributes[VertexAttributePosition].offset = 0;
    _mtlVertexDescriptor.attributes[VertexAttributePosition].bufferIndex = BufferIndexMeshPosition;

    _mtlVertexDescriptor.attributes[VertexAttributeTexcoord].format = MTLVertexFormatFloat2; // TODO: compress
    _mtlVertexDescriptor.attributes[VertexAttributeTexcoord].offset = 0;
    _mtlVertexDescriptor.attributes[VertexAttributeTexcoord].bufferIndex = BufferIndexMeshUV0;

    _mtlVertexDescriptor.attributes[VertexAttributeNormal].format = MTLVertexFormatFloat3; // TODO: compress
    _mtlVertexDescriptor.attributes[VertexAttributeNormal].offset = 0;
    _mtlVertexDescriptor.attributes[VertexAttributeNormal].bufferIndex = BufferIndexMeshNormal;

    _mtlVertexDescriptor.attributes[VertexAttributeTangent].format = MTLVertexFormatFloat4; // TODO: compress
    _mtlVertexDescriptor.attributes[VertexAttributeTangent].offset = 0;
    _mtlVertexDescriptor.attributes[VertexAttributeTangent].bufferIndex = BufferIndexMeshTangent;
   
    //_mtlVertexDescriptor.layouts[BufferIndexMeshPosition].stepRate = 1;
    //_mtlVertexDescriptor.layouts[BufferIndexMeshPosition].stepFunction = MTLVertexStepFunctionPerVertex;

    _mtlVertexDescriptor.layouts[BufferIndexMeshPosition].stride = 3*4;
    _mtlVertexDescriptor.layouts[BufferIndexMeshUV0].stride = 2*4;
    _mtlVertexDescriptor.layouts[BufferIndexMeshNormal].stride = 3*4;
    _mtlVertexDescriptor.layouts[BufferIndexMeshTangent].stride = 4*4;
    
    //-----------------------
    // for ModelIO
    _mdlVertexDescriptor =
        MTKModelIOVertexDescriptorFromMetal(_mtlVertexDescriptor);

    _mdlVertexDescriptor.attributes[VertexAttributePosition].name  = MDLVertexAttributePosition;
    _mdlVertexDescriptor.attributes[VertexAttributeTexcoord].name  = MDLVertexAttributeTextureCoordinate;
    _mdlVertexDescriptor.attributes[VertexAttributeNormal].name    = MDLVertexAttributeNormal;
    _mdlVertexDescriptor.attributes[VertexAttributeTangent].name   = MDLVertexAttributeTangent;
}



- (void)_loadMetalWithView:(nonnull MTKView *)view
{
    /// Load Metal state objects and initialize renderer dependent view properties

    view.colorPixelFormat = MTLPixelFormatRGBA16Float;
    view.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    view.sampleCount = 1;

    _viewFramebuffer.colorPixelFormat = view.colorPixelFormat;
    _viewFramebuffer.depthStencilPixelFormat = view.depthStencilPixelFormat;
    _viewFramebuffer.sampleCount = view.sampleCount;
    
    [self _createVertexDescriptor];
    
    // first time use the default library, if reload is called then use different library
    _shaderLibrary = [_device newDefaultLibrary];

    
    [self _createRenderPipelines];
    
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

- (BOOL)hotloadShaders:(const char*)filename
{
    NSURL* _metallibFileURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filename]];

    NSError* err = nil;
    NSDate *fileDate = nil;
    [_metallibFileURL getResourceValue:&fileDate forKey:NSURLContentModificationDateKey error:&err];

    // only reload if the metallib changed timestamp, otherwise default.metallib has most recent copy
    if (err != nil || [_metallibFileDate isEqualToDate:fileDate]) {
        return NO;
    }
    _metallibFileDate = fileDate;
    
    // Now dynamically load the metallib
    NSData* dataNS = [NSData dataWithContentsOfURL:_metallibFileURL options:NSDataReadingMappedIfSafe
 error:&err];
    if (dataNS == nil) {
        return NO;
    }
    dispatch_data_t data = dispatch_data_create(dataNS.bytes, dataNS.length, dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    
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

- (id<MTLComputePipelineState>)_createComputePipeline:(const char*)name
{
    NSString* nameNS = [NSString stringWithUTF8String:name];
    NSError *error = nil;
    id<MTLFunction> computeFunction = [_shaderLibrary newFunctionWithName:nameNS];
    
    id<MTLComputePipelineState> pipe;
    if (computeFunction) {
        computeFunction.label = nameNS;
        
        pipe = [_device newComputePipelineStateWithFunction:computeFunction error:&error];
    }

    if (!pipe) {
        KLOGE("kramv", "Failed to create compute pipeline state for %s, error %s", name, error ? error.localizedDescription.UTF8String : "");
        return nil;
    }
    
    return pipe;
}

- (void)_createComputePipelines
{
    _pipelineStateImageCS       = [self _createComputePipeline:"SampleImageCS"];
    _pipelineStateImageArrayCS  = [self _createComputePipeline:"SampleImageArrayCS"];
    _pipelineStateVolumeCS      = [self _createComputePipeline:"SampleVolumeCS"];
    _pipelineStateCubeCS        = [self _createComputePipeline:"SampleCubeCS"];
    _pipelineStateCubeArrayCS   = [self _createComputePipeline:"SampleCubeArrayCS"];
    _pipelineState1DArrayCS     = [self _createComputePipeline:"SampleImage1DArrayCS"];
}

- (id<MTLRenderPipelineState>)_createRenderPipeline:(const char*)vs fs:(const char*)fs
{
    NSString* vsNameNS = [NSString stringWithUTF8String:vs];
    NSString* fsNameNS = [NSString stringWithUTF8String:fs];
   
    id <MTLFunction> vertexFunction;
    id <MTLFunction> fragmentFunction;
    
    MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = fsNameNS;
    pipelineStateDescriptor.sampleCount = _viewFramebuffer.sampleCount;
    pipelineStateDescriptor.vertexDescriptor = _mtlVertexDescriptor;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat = _viewFramebuffer.colorPixelFormat;
    
    // TODO: could drop these for images, but want a 3D preview of content
    // or might make these memoryless.
    pipelineStateDescriptor.depthAttachmentPixelFormat = _viewFramebuffer.depthStencilPixelFormat;
    pipelineStateDescriptor.stencilAttachmentPixelFormat = _viewFramebuffer.depthStencilPixelFormat;

    NSError *error = NULL;
    
    //-----------------------
   
    vertexFunction = [_shaderLibrary newFunctionWithName:vsNameNS];
    fragmentFunction = [_shaderLibrary newFunctionWithName:fsNameNS];
    
    id<MTLRenderPipelineState> pipe;
    
    if (vertexFunction && fragmentFunction) {
        vertexFunction.label = vsNameNS;
        fragmentFunction.label = fsNameNS;
       
        pipelineStateDescriptor.vertexFunction = vertexFunction;
        pipelineStateDescriptor.fragmentFunction = fragmentFunction;
        
        pipe = [_device newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
    }
    
    if (!pipe)
    {
        KLOGE("kramv", "Failed to create render pipeline state for %s, error %s", fs, error ? error.description.UTF8String : "");
        return nil;
    }
    
    return pipe;
}

- (void)_createRenderPipelines
{
    _pipelineStateImage         = [self _createRenderPipeline:"DrawImageVS" fs:"DrawImagePS"];
    _pipelineStateImageArray    = [self _createRenderPipeline:"DrawImageVS" fs:"DrawImageArrayPS"];
    _pipelineState1DArray       = [self _createRenderPipeline:"DrawImageVS" fs:"Draw1DArrayPS"];
    _pipelineStateCube          = [self _createRenderPipeline:"DrawCubeVS" fs:"DrawCubePS"];
    _pipelineStateCubeArray     = [self _createRenderPipeline:"DrawCubeVS" fs:"DrawCubeArrayPS"];
    _pipelineStateVolume        = [self _createRenderPipeline:"DrawVolumeVS" fs:"DrawVolumePS"];
}

- (void)_createSampleRender
{
    {
        // writing to this texture
        MTLTextureDescriptor* textureDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:1 height:1 mipmapped:NO];
        
        textureDesc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        textureDesc.storageMode = MTLStorageModeManaged;
        _sampleComputeTex = [_device newTextureWithDescriptor:textureDesc];
    }
    
    {
        // this must match drawable format due to using a blit to copy pixel out of drawable
        MTLTextureDescriptor* textureDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float width:1 height:1 mipmapped:NO];
        //textureDesc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        textureDesc.storageMode = MTLStorageModeManaged;

        _sampleRenderTex = [_device newTextureWithDescriptor:textureDesc];
    }
}

- (MTKMesh*)_createMeshAsset:(const char*)name mdlMesh:(MDLMesh*)mdlMesh doFlipUV:(bool)doFlipUV
{
    NSError* error = nil;

    mdlMesh.vertexDescriptor = _mdlVertexDescriptor;
    
    // ModelIO has the uv going counterclockwise on sphere/cylinder, but not on the box.
    // And it also has a flipped bitangent.w.
    
    // flip the u coordinate
    if (doFlipUV)
    {
        id<MDLMeshBuffer> uvs = mdlMesh.vertexBuffers[BufferIndexMeshUV0];
        MDLMeshBufferMap *uvsMap = [uvs map];
        
        packed_float2* uvData = (packed_float2*)uvsMap.bytes;
    
        for (uint32_t i = 0; i < mdlMesh.vertexCount; ++i) {
            auto& uv = uvData[i];
            
            uv.x = 1.0f - uv.x;
        }
    }
    
    [mdlMesh addOrthTanBasisForTextureCoordinateAttributeNamed: MDLVertexAttributeTextureCoordinate
                                          normalAttributeNamed: MDLVertexAttributeNormal
                                         tangentAttributeNamed: MDLVertexAttributeTangent];
    
    // DONE: flip the bitangent.w sign here, and remove the flip in the shader
    bool doFlipBitangent = true;
    if (doFlipBitangent)
    {
        id<MDLMeshBuffer> uvs = mdlMesh.vertexBuffers[BufferIndexMeshTangent];
        MDLMeshBufferMap *uvsMap = [uvs map];
        packed_float4* uvData = (packed_float4*)uvsMap.bytes;
        
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
    
        [pos.buffer addDebugMarker:@"Pos" range:NSMakeRange(pos.offset, pos.length)];
        [uvs.buffer addDebugMarker:@"UV" range:NSMakeRange(uvs.offset, uvs.length)];
        [normals.buffer addDebugMarker:@"Nor" range:NSMakeRange(normals.offset, normals.length)];
        [tangents.buffer addDebugMarker:@"Tan" range:NSMakeRange(tangents.offset, tangents.length)];
        
        // This seems to already be named "ellisoid-Indices",
        // need to do for ib as well
        for (MTKSubmesh* submesh in mesh.submeshes) {
            [submesh.indexBuffer.buffer addDebugMarker:mesh.name range:NSMakeRange(submesh.indexBuffer.offset, submesh.indexBuffer.length)];
        }
    }
    
    if(!mesh || error)
    {
        KLOGE("kramv", "Error creating MetalKit mesh %s", error.localizedDescription.UTF8String);
        return nil;
    }

    return mesh;
}

// why isn't this defined in simd lib?
struct packed_float3 {
    float x,y,z;
};

- (void)_loadAssets
{
    /// Load assets into metal objects
    
    MDLMesh *mdlMesh;
    
    mdlMesh = [MDLMesh newBoxWithDimensions:(vector_float3){1, 1, 1}
                                            segments:(vector_uint3){1, 1, 1}
                                        geometryType:MDLGeometryTypeTriangles
                                       inwardNormals:NO
                                           allocator:_metalAllocator];
    
    _meshBox = [self _createMeshAsset:"MeshBox" mdlMesh:mdlMesh doFlipUV:false];
    
    // The sphere/cylinder shapes are v increasing in -Y, and u increasing conterclockwise,
    // u is the opposite direction to the cube/plane, so need to flip those coords
    // I think this has also flipped the tangents the wrong way, but building tangents after
    // flipping u direction doesn't flip the bitangent.  So bitangent.w is flipped.
    // For sanity, Tangent is increasing u, and Bitangent is increasing v.
    
    // All prims are viewed with +Y, not +Z up
    
    mdlMesh = [MDLMesh newEllipsoidWithRadii:(vector_float3){0.5, 0.5, 0.5} radialSegments:16 verticalSegments:16 geometryType:MDLGeometryTypeTriangles inwardNormals:NO hemisphere:NO allocator:_metalAllocator];

    float angle = M_PI * 0.5;
    float2 cosSin = float2m(cos(angle), sin(angle));
    
    {
        mdlMesh.vertexDescriptor = _mdlVertexDescriptor;
        
        id<MDLMeshBuffer> pos = mdlMesh.vertexBuffers[BufferIndexMeshPosition];
        MDLMeshBufferMap *posMap = [pos map];
        packed_float3* posData = (packed_float3*)posMap.bytes;
        
        id<MDLMeshBuffer> normals = mdlMesh.vertexBuffers[BufferIndexMeshNormal];
        MDLMeshBufferMap *normalsMap = [normals map];
        packed_float3* normalData = (packed_float3*)normalsMap.bytes;
        
        // vertexCount reports 306, but vertex 289+ are garbage
        uint32_t numVertices = 289; // mdlMesh.vertexCount
        
        for (uint32_t i = 0; i < numVertices; ++i) {
            {
                auto& pos = posData[i];
            
                // dumb rotate about Y-axis
                auto copy = pos;
                
                pos.x = copy.x * cosSin.x - copy.z * cosSin.y;
                pos.z = copy.x * cosSin.y + copy.z * cosSin.x;
            }
            
            {
                auto& normal = normalData[i];
                auto copy = normal;
                normal.x = copy.x * cosSin.x - copy.z * cosSin.y;
                normal.z = copy.x * cosSin.y + copy.z * cosSin.x;
            }
        }
        
        // Hack - knock out all bogus tris from ModelIO that lead to garbage tris
        for (uint32_t i = numVertices; i < mdlMesh.vertexCount; ++i) {
            auto& pos = posData[i];
            pos.x = NAN;
        }
            
    }
    
    _meshSphere = [self _createMeshAsset:"MeshSphere" mdlMesh:mdlMesh doFlipUV:true];
       
    
    mdlMesh = [MDLMesh newEllipsoidWithRadii:(vector_float3){0.5, 0.5, 0.5} radialSegments:16 verticalSegments:16 geometryType:MDLGeometryTypeTriangles inwardNormals:NO hemisphere:NO allocator:_metalAllocator];
    
    
    // ModelIO has the uv going counterclockwise on sphere/cylinder, but not on the box.
    // And it also has a flipped bitangent.w.
    
    // flip the u coordinate
    bool doFlipUV = true;
    if (doFlipUV)
    {
        mdlMesh.vertexDescriptor = _mdlVertexDescriptor;
        
        id<MDLMeshBuffer> uvs = mdlMesh.vertexBuffers[BufferIndexMeshUV0];
        MDLMeshBufferMap *uvsMap = [uvs map];
        packed_float2* uvData = (packed_float2*)uvsMap.bytes;
    
        // this is all aos
        
        id<MDLMeshBuffer> pos = mdlMesh.vertexBuffers[BufferIndexMeshPosition];
        MDLMeshBufferMap *posMap = [pos map];
        packed_float3* posData = (packed_float3*)posMap.bytes;
        
        id<MDLMeshBuffer> normals = mdlMesh.vertexBuffers[BufferIndexMeshNormal];
        MDLMeshBufferMap *normalsMap = [normals map];
        packed_float3* normalData = (packed_float3*)normalsMap.bytes;
        
        
        // vertexCount reports 306, but vertex 289+ are garbage
        uint32_t numVertices = 289; // mdlMesh.vertexCount
        
        for (uint32_t i = 0; i < numVertices; ++i) {
            {
                auto& pos = posData[i];
                
                // dumb rotate about Y-axis
                auto copy = pos;
                pos.x = copy.x * cosSin.x - copy.z * cosSin.y;
                pos.z = copy.x * cosSin.y + copy.z * cosSin.x;
            }
            
            {
                auto& normal = normalData[i];
                auto copy = normal;
                normal.x = copy.x * cosSin.x - copy.z * cosSin.y;
                normal.z = copy.x * cosSin.y + copy.z * cosSin.x;
            }
            
            auto& uv = uvData[i];
        
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
            auto& pos = posData[i];
            pos.x = NAN;
        }
        
        // TODO: may need to flip tangent on the inverted side
        // otherwise lighting is just wrong, but tangents generated in _createMeshAsset
        // move that here, and flip the tangents in the loop
    }
        
    _meshSphereMirrored = [self _createMeshAsset:"MeshSphereMirrored" mdlMesh:mdlMesh doFlipUV:false];
    
    
// this maps 1/3rd of texture to the caps, and just isn't a very good uv mapping, using capsule instead
//    mdlMesh = [MDLMesh newCylinderWithHeight:1.0
//                                       radii:(vector_float2){0.5, 0.5}
//                                            radialSegments:16
//                                        verticalSegments:1
//                                        geometryType:MDLGeometryTypeTriangles
//                                       inwardNormals:NO
//                                           allocator:_metalAllocator];
//
//    _meshCylinder = [self _createMeshAsset:"MeshCylinder" mdlMesh:mdlMesh doFlipUV:true];
    
    mdlMesh = [MDLMesh newCapsuleWithHeight:1.0
                                   radii:(vector_float2){0.5, 0.25} // vertical cap subtracted from height
                          radialSegments:16
                        verticalSegments:1
                      hemisphereSegments:16
                            geometryType:MDLGeometryTypeTriangles
                           inwardNormals:NO
                               allocator:_metalAllocator];

    
    _meshCapsule = [self _createMeshAsset:"MeshCapsule" mdlMesh:mdlMesh doFlipUV:true];
    
    _mesh = _meshBox;
    
}



- (BOOL)loadTextureFromImage:(nonnull const char*)fullFilenameString
                   timestamp:(double)timestamp
                       image:(kram::KTXImage&)image
                 imageNormal:(kram::KTXImage*)imageNormal
                    isArchive:(BOOL)isArchive
{
    // image can be decoded to rgba8u if platform can't display format natively
    // but still want to identify blockSize from original format
    string fullFilename = fullFilenameString;
    
    // Note that modstamp can change, but content data hash may be the same
    bool isNewFile = (fullFilename != _showSettings->lastFilename);
    bool isTextureChanged = isNewFile || (timestamp != _showSettings->lastTimestamp);
    
    if (isTextureChanged) {     
        // synchronously cpu upload from ktx file to buffer, with eventual gpu blit from buffer to returned texture.  TODO: If buffer is full, then something needs to keep KTXImage and data alive.  This load may also decode the texture to RGBA8.
        
        MTLPixelFormat originalFormatMTL = MTLPixelFormatInvalid;
        id<MTLTexture> texture = [_loader loadTextureFromImage:image originalFormat:&originalFormatMTL];
        if (!texture) {
            return NO;
        }
        
        // hacking in the normal texture here, so can display them together during preview
        id<MTLTexture> normalTexture;
        if (imageNormal) {
            normalTexture = [_loader loadTextureFromImage:*imageNormal originalFormat:nil];
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
            _showSettings->imageInfo = kramInfoKTXToString(fullFilename, image, false);
            _showSettings->imageInfoVerbose = kramInfoKTXToString(fullFilename, image, true);
        }
        
        _showSettings->originalFormat = (MyMTLPixelFormat)originalFormatMTL;
        _showSettings->decodedFormat = (MyMTLPixelFormat)texture.pixelFormat;
        
        _showSettings->lastFilename = fullFilename;
        _showSettings->lastTimestamp = timestamp;
        
        @autoreleasepool {
            _colorMap = texture;
            _normalMap = normalTexture;
        }
        
        [self updateImageSettings:fullFilename image:image];
    }
    
    [self resetSomeImageSettings:isNewFile];
    
    return YES;
}
    
- (BOOL)loadTexture:(nonnull NSURL *)url
{
    string fullFilename = url.path.UTF8String;
    
    // can use this to pull, or use fstat on FileHelper
    NSDate *fileDate = nil;
    NSError *error = nil;
    [url getResourceValue:&fileDate forKey:NSURLContentModificationDateKey error:&error];
    
    // DONE: tie this to url and modstamp differences
    double timestamp = fileDate.timeIntervalSince1970;
    bool isNewFile =  (fullFilename != _showSettings->lastFilename);
    
    bool isTextureChanged = isNewFile || (timestamp != _showSettings->lastTimestamp);
    
    // image can be decoded to rgba8u if platform can't display format natively
    // but still want to identify blockSize from original format
    if (isTextureChanged) {
        // TODO: hold onto these, so can reference block data
        KTXImage image;
        KTXImageData imageData;
        
        if (![_loader loadImageFromURL:url image:image imageData:imageData]) {
            return NO;
        }
        
        MTLPixelFormat originalFormatMTL = MTLPixelFormatInvalid;
        id<MTLTexture> texture = [_loader loadTextureFromImage:image originalFormat:&originalFormatMTL];
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
            _showSettings->imageInfo = kramInfoKTXToString(fullFilename, image, false);
            _showSettings->imageInfoVerbose = kramInfoKTXToString(fullFilename, image, true);
        }
        
        _showSettings->originalFormat = (MyMTLPixelFormat)originalFormatMTL;
        _showSettings->decodedFormat = (MyMTLPixelFormat)texture.pixelFormat;
        
        _showSettings->lastFilename = fullFilename;
        _showSettings->lastTimestamp = timestamp;
        
        @autoreleasepool {
            _colorMap = texture;
            _normalMap = nil;
        }
        
        [self updateImageSettings:fullFilename image:image];
    }
    
    [self resetSomeImageSettings:isNewFile];
    
    return YES;
}

// only called on new or modstamp-changed image
- (void)updateImageSettings:(const string&)fullFilename image:(KTXImage&)image
{
    // this is the actual format, may have been decoded
    id<MTLTexture> texture = _colorMap;
    MyMTLPixelFormat format = (MyMTLPixelFormat)texture.pixelFormat;
    
    // format may be trancoded to gpu-friendly format
    MyMTLPixelFormat originalFormat = image.pixelFormat;
    
    _showSettings->blockX = image.blockDims().x;
    _showSettings->blockY = image.blockDims().y;
    
    _showSettings->isSigned = isSignedFormat(format);
    
    string fullFilenameCopy = fullFilename;
    string filename = toLower(fullFilenameCopy);

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
    
    // could cycle between rrr1 and r001.
    int32_t numChannels = numChannelsOfFormat(originalFormat);
    bool isSigned = isSignedFormat(originalFormat);
    
    // note that decoded textures are 3/4 channel even though they are normal/sdf originally, so test those first
    if (numChannels == 2 || endsWith(filenameShort, "-n") || endsWith(filenameShort, "_normal")) {
        isNormal = true;
    }
    else if ((numChannels == 1 && isSigned) || endsWith(filenameShort, "-sdf")) {
        isSDF = true;
    }
    else if (numChannels == 3 || numChannels == 4 || endsWith(filenameShort, "-a") || endsWith(filenameShort, "_basecolor")) {
        isAlbedo = true;
    }
    
    _showSettings->isNormal = isNormal;
    _showSettings->isSDF = isSDF;
    
    // textures are already premul, so don't need to premul in shader
    // should really have 3 modes, unmul, default, premul
    bool isPNG = isPNGFilename(filename.c_str());
    
    _showSettings->isPremul = false;
    if (isAlbedo && isPNG) {
        _showSettings->isPremul = true; // convert to premul in shader, so can see other channels
    }
    else if (isNormal || isSDF) {
        _showSettings->isPremul = false;
    }
        
    _showSettings->numChannels = numChannels;
    
    // TODO: identify if texture holds normal data from the props
    // have too many 4 channel normals that shouldn't swizzle like this
    // kramTextures.py is using etc2rg on iOS for now, and not astc.
    
    _showSettings->isSwizzleAGToRG = false;

// For best sdf and normal reconstruct from ASTC or BC3, must use RRR1 and GGGR or RRRG
// BC1nm multiply r*a in the shader, but just use BC5 anymore.
//    if (isASTCFormat(originalFormat) && isNormal) {
//        // channels after = "ag01"
//        _showSettings->isSwizzleAGToRG = true;
//    }
        
    // can derive these from texture queries
    _showSettings->mipCount = (int32_t)image.header.numberOfMipmapLevels;
    _showSettings->faceCount = (image.textureType == MyMTLTextureTypeCube ||
                               image.textureType == MyMTLTextureTypeCubeArray) ? 6 : 0;
    _showSettings->arrayCount = (int32_t)image.header.numberOfArrayElements;
    _showSettings->sliceCount = (int32_t)image.depth;
    
    _showSettings->imageBoundsX = (int32_t)image.width;
    _showSettings->imageBoundsY = (int32_t)image.height;
}

float zoom3D = 1.0f;

- (void)resetSomeImageSettings:(BOOL)isNewFile {
    
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
        _showSettings->mipNumber = std::min(_showSettings->mipNumber, _showSettings->mipCount);
        _showSettings->faceNumber = std::min(_showSettings->faceNumber, _showSettings->faceCount);
        _showSettings->arrayNumber = std::min(_showSettings->arrayNumber, _showSettings->arrayCount);
        _showSettings->sliceNumber = std::min(_showSettings->sliceNumber, _showSettings->sliceCount);
    }

    
    [self updateViewTransforms];
    
    // this controls viewMatrix (global to all visible textures)
    _showSettings->panX = 0.0f;
    _showSettings->panY = 0.0f;
    
    _showSettings->zoom = _showSettings->zoomFit;
    
    // test rendering with inversion and mirroring and non-uniform scale
    bool doInvertX = false;
    bool doScaleX = false;
    
    // have one of these for each texture added to the viewer
    float scaleX = MAX(1, _showSettings->imageBoundsX);
    float scaleY = MAX(1, _showSettings->imageBoundsY);
    float scaleZ = MAX(scaleX, scaleY); // don't want 1.0f, or specular is all off due to extreme scale differences
    
    float tmpScaleX = scaleX;
    if (doInvertX) {
        tmpScaleX = -tmpScaleX;
    }
    if (doScaleX) {
        tmpScaleX *= 2.0f;
    }
    
    _modelMatrix = float4x4(float4m(tmpScaleX, scaleY, scaleZ, 1.0f)); // non uniform scale
    _modelMatrix = _modelMatrix * matrix4x4_translation(0.0f, 0.0f, -1.0); // set z=-1 unit back
    
    // uniform scaled 3d primitive
    float scale = MAX(scaleX, scaleY);
    
    // store the zoom into thew view matrix
    // fragment tangents seem to break down at high model scale due to precision differences between worldPos and uv
    static bool useZoom3D = false;
    if (useZoom3D) {
        zoom3D = scale; // * _showSettings->viewSizeX / 2.0f;
        scale = 1.0;
    }
    
    _modelMatrix3D = float4x4(float4m((doScaleX || doInvertX) ? tmpScaleX : scale, scale, scale, 1.0f)); // uniform scale
    _modelMatrix3D = _modelMatrix3D * matrix4x4_translation(0.0f, 0.0f, -1.0f); // set z=-1 unit back
}

- (float4x4)computeImageTransform:(float)panX panY:(float)panY zoom:(float)zoom {
    // translate
    float4x4 panTransform = matrix4x4_translation(-panX, panY, 0.0);
    
    // non-uniform scale is okay here, only affects ortho volume
    // setting this to uniform zoom and object is not visible, zoom can be 20x in x and y
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
        return _projectionMatrix * viewMatrix * _modelMatrix;
    }
}

bool almost_equal_elements(float3 v, float tol) {
    return (fabs(v.x - v.y) < tol) && (fabs(v.x - v.z) < tol);
}

const float3x3& toFloat3x3(const float4x4& m) {
    return (const float3x3&)m;
}

float4 inverseScaleSquared(const float4x4& m) {
    float3 scaleSquared = float3m(
        length_squared(m.columns[0].xyz),
        length_squared(m.columns[1].xyz),
        length_squared(m.columns[2].xyz));
    
    // if uniform, then set scaleSquared all to 1
    if (almost_equal_elements(scaleSquared, 1e-5)) {
        scaleSquared = float3m(1.0);
    }
   
    // don't divide by 0
    float3 invScaleSquared = recip(simd::max(float3m(0.0001 * 0.0001), scaleSquared));
        
    // identify determinant here for flipping orientation
    // all shapes with negative determinant need orientation flipped for backfacing
    // and need to be grouned together if rendering with instancing
    float det = determinant(toFloat3x3(m));
    
    return float4m(invScaleSquared, det);
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
    uniforms.lightingMode = (ShaderLightingMode)_showSettings->lightingMode;
    
    MyMTLTextureType textureType = MyMTLTextureType2D;
    MyMTLPixelFormat textureFormat = MyMTLPixelFormatInvalid;
    if (_colorMap) {
        textureType = (MyMTLTextureType)_colorMap.textureType;
        textureFormat = (MyMTLPixelFormat)_colorMap.pixelFormat;
    }
    
    uniforms.isCheckerboardShown = _showSettings->isCheckerboardShown;
    
    // addressing mode
    bool isCube = (textureType == MyMTLTextureTypeCube || textureType == MyMTLTextureTypeCubeArray);
    bool doWrap = !isCube &&  _showSettings->isWrap;
    bool doEdge = !doWrap;
    bool doZero = !doEdge;
    uniforms.isWrap = doWrap ? _showSettings->isWrap : false;
    
    uniforms.isPreview = _showSettings->isPreview;
    
    uniforms.isNormalMapPreview = false;
    if (uniforms.isPreview) {
        uniforms.isNormalMapPreview = uniforms.isNormal || (_normalMap != nil);
        
        if (_normalMap != nil) {
            uniforms.isNormalMapSigned = isSignedFormat((MyMTLPixelFormat)_normalMap.pixelFormat);
            uniforms.isNormalMapSwizzleAGToRG = false; // TODO: need a prop for this
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
    
    // no debug mode when preview kicks on, make it possible to toggle back and forth more easily
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
    switch(_showSettings->meshNumber) {
        case 0: _mesh = _meshBox; _showSettings->is3DView = false; break;
        case 1: _mesh = _meshBox; break;
        case 2: _mesh = _meshSphere; break;
        case 3: _mesh = _meshSphereMirrored; break;
        //case 3: _mesh = _meshCylinder; break;
        case 4: _mesh = _meshCapsule; break;
    }
    uniforms.is3DView = _showSettings->is3DView;
   
    // on small textures can really see missing pixel (3 instead of 4 pixels)
    // so only do this on the sphere/capsule which wrap-around uv space
    uniforms.isInsetByHalfPixel = false;
    if (_showSettings->meshNumber >= 2 && doZero) {
        uniforms.isInsetByHalfPixel = true;
    }
    
    // translate
    float4x4 panTransform = matrix4x4_translation(-_showSettings->panX, _showSettings->panY, 0.0);
    
    // scale
    float zoom = _showSettings->zoom;
    
    if (_showSettings->is3DView) {
        _viewMatrix3D = float4x4(float4m(zoom, zoom, 1.0f, 1.0f)); // non-uniform
        _viewMatrix3D = panTransform * _viewMatrix3D;
        
        // viewMatrix should typically be the inverse
        //_viewMatrix = simd_inverse(_viewMatrix3D);
       
        float4x4 projectionViewMatrix = _projectionMatrix * _viewMatrix3D;
        uniforms.projectionViewMatrix = projectionViewMatrix;
        
        // works when only one texture, but switch to projectViewMatrix
        uniforms.modelMatrix = _modelMatrix3D;
       
        uniforms.modelMatrixInvScale2 = inverseScaleSquared(_modelMatrix3D);
        
        _showSettings->isInverted = uniforms.modelMatrixInvScale2.w < 0.0f;
        
        // this was stored so view could use it, but now that code calcs the transform via computeImageTransform
        _showSettings->projectionViewModelMatrix = uniforms.projectionViewMatrix * uniforms.modelMatrix;
        
        // cache the camera position
        uniforms.cameraPosition = inverse(_viewMatrix3D).columns[3].xyz; // this is all ortho
    }
    else {
        _viewMatrix = float4x4(float4m(zoom, zoom, 1.0f, 1.0f));
        _viewMatrix = panTransform * _viewMatrix;
        
        // viewMatrix should typically be the inverse
        //_viewMatrix = simd_inverse(_viewMatrix3D);
       
        float4x4 projectionViewMatrix = _projectionMatrix * _viewMatrix;
        uniforms.projectionViewMatrix = projectionViewMatrix;
        
        // works when only one texture, but switch to projectViewMatrix
        uniforms.modelMatrix = _modelMatrix;
       
        uniforms.modelMatrixInvScale2 = inverseScaleSquared(_modelMatrix);
        
        _showSettings->isInverted = uniforms.modelMatrixInvScale2.w < 0.0f;
        
        // this was stored so view could use it, but now that code calcs the transform via computeImageTransform
        _showSettings->projectionViewModelMatrix = uniforms.projectionViewMatrix * uniforms.modelMatrix ;
        
        // cache the camera position
        uniforms.cameraPosition = inverse(_viewMatrix).columns[3].xyz; // this is all ortho
    }

    //_rotation += .01;
}

- (void)_setUniformsLevel:(UniformsLevel&)uniforms mipLOD:(int32_t)mipLOD
{
    uniforms.mipLOD = mipLOD;
    
    uniforms.arrayOrSlice = 0;
    uniforms.face  = 0;

    uniforms.textureSize = float4m(0.0f);
    MyMTLTextureType textureType = MyMTLTextureType2D;
    if (_colorMap) {
        textureType = (MyMTLTextureType)_colorMap.textureType;
        uniforms.textureSize = float4m(_colorMap.width, _colorMap.height, 1.0f/_colorMap.width, 1.0f/_colorMap.height);
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

        id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
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
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
        if (blitEncoder)
        {
            blitEncoder.label = @"MyBlitEncoder";
            [_loader uploadTexturesIfNeeded:blitEncoder commandBuffer:commandBuffer];
            [blitEncoder endEncoding];
        }
        
        
        [self drawMain:commandBuffer view:view];
        
        // hold onto this for sampling from it via eyedropper
        _lastDrawableTexture = view.currentDrawable.texture;
        
        [commandBuffer presentDrawable:view.currentDrawable];
        [commandBuffer commit];
    }
}

- (void)drawMain:(id<MTLCommandBuffer>)commandBuffer view:(nonnull MTKView *)view {
    // Delay getting the currentRenderPassDescriptor until absolutely needed. This avoids
    //   holding onto the drawable and blocking the display pipeline any longer than necessary
    MTLRenderPassDescriptor* renderPassDescriptor = view.currentRenderPassDescriptor;

    if (renderPassDescriptor == nil) {
        return;
    }
    
    if (_colorMap == nil) {
        // this will clear target
        id<MTLRenderCommandEncoder> renderEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        
        if (renderEncoder) {
            renderEncoder.label = @"MainRender";
            [renderEncoder endEncoding];
        }
        
        return;
    }
    
    // Final pass rendering code here
    id<MTLRenderCommandEncoder> renderEncoder =
    [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    if (!renderEncoder) {
        return;
    }
    
    renderEncoder.label = @"MainRender";

    // set raster state
    [renderEncoder setFrontFacingWinding:_showSettings->isInverted ?
                        MTLWindingCounterClockwise : MTLWindingCounterClockwise];
    [renderEncoder setCullMode:MTLCullModeBack];
    [renderEncoder setDepthStencilState:_depthStateFull];

    [renderEncoder pushDebugGroup:@"DrawShape"];

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

    //---------------------------------------
    // figure out the sampler

    id <MTLSamplerState> sampler;

    MyMTLTextureType textureType = (MyMTLTextureType)_colorMap.textureType;
    
    bool isCube = (textureType == MyMTLTextureTypeCube || textureType == MyMTLTextureTypeCubeArray);
    bool doWrap = !isCube && _showSettings->isWrap;
    bool doEdge = !doWrap;
    
    if (_showSettings->isPreview) {
        sampler = doWrap ? _colorMapSamplerFilterWrap : (doEdge ? _colorMapSamplerFilterEdge : _colorMapSamplerFilterBorder);
    }
    else {
        sampler = doWrap ? _colorMapSamplerNearestWrap : (doEdge ? _colorMapSamplerNearestEdge : _colorMapSamplerNearestBorder);
    }
    
    //---------------------------------------
    //for (texture in _textures) // TODO: setup
    //if (_colorMap)
    {
        // TODO: set texture specific uniforms, but using single _colorMap for now
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
        [renderEncoder setFragmentTexture:_colorMap
                                  atIndex:TextureIndexColor];
        
        // setup normal map
        if (_normalMap && _showSettings->isPreview)
        {
            [renderEncoder setFragmentTexture:_normalMap
                                      atIndex:TextureIndexNormal];
        }

        UniformsLevel uniformsLevel;
        uniformsLevel.drawOffset = float2m(0.0f);
        
        if (_showSettings->isPreview) {
            // upload this on each face drawn, since want to be able to draw all mips/levels at once
            [self _setUniformsLevel:uniformsLevel mipLOD:_showSettings->mipNumber];
            
            [renderEncoder setVertexBytes:&uniformsLevel
                                    length:sizeof(uniformsLevel)
                                   atIndex:BufferIndexUniformsLevel];

            [renderEncoder setFragmentBytes:&uniformsLevel
                                      length:sizeof(uniformsLevel)
                                     atIndex:BufferIndexUniformsLevel];
            
            // use exisiting lod, and mip
            [renderEncoder setFragmentSamplerState:sampler atIndex:SamplerIndexColor];
            
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
                        
            // gap the contact sheet, note this 2 pixels is scaled on small textures by the zoom
            int32_t gap = _showSettings->showAllPixelGap; // * _showSettings->viewContentScaleFactor;
            
            for (int32_t mip = 0; mip < _showSettings->mipCount; ++mip) {
                
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
                    [renderEncoder setFragmentSamplerState:sampler
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
            int32_t mip = _showSettings->mipNumber;
            
            // upload this on each face drawn, since want to be able to draw all mips/levels at once
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
    if (_colorMap == nil) {
        return;
    }
    
    // this can occur during a resize
    if (!_lastDrawableTexture)
        return;
    
    id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    if (!commandBuffer)
        return;
    
    commandBuffer.label = @"MyCommand";

    // this reads directly from compressed texture via a compute shader
    int32_t textureLookupX = _showSettings->textureLookupX;
    int32_t textureLookupY = _showSettings->textureLookupY;
    
    bool isDrawableBlit = _showSettings->isEyedropperFromDrawable();
    
    // TODO: only don't blit for plane + no debug or shape
    // otherwise want the pixel under the cursor, but this may include grid mixed in and other debug overlays
    if (isDrawableBlit) {
        MTLOrigin srcOrigin = MTLOriginMake(_showSettings->cursorX, _showSettings->cursorY, 0);
        srcOrigin.x *= _showSettings->viewContentScaleFactor;
        srcOrigin.y *= _showSettings->viewContentScaleFactor;
        
        if ((srcOrigin.x >= 0 && srcOrigin.x < _lastDrawableTexture.width) &&
            (srcOrigin.y >= 0 && srcOrigin.y < _lastDrawableTexture.height))
        {
            
            // Note: here we don't know the uv in original texture, would have to write that out to another
            // texture.  Also on shapes, texel may not change but lighting might.
            
            // can simply blit the color out of the render buffer
            id <MTLBlitCommandEncoder> blitCommandEncoder = [commandBuffer blitCommandEncoder];
            if (blitCommandEncoder) {
                [blitCommandEncoder copyFromTexture:_lastDrawableTexture
                                        sourceSlice:0 sourceLevel:0 sourceOrigin:srcOrigin sourceSize:MTLSizeMake(1,1,1)
                                          toTexture:_sampleRenderTex
                                   destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)
                ];
                [blitCommandEncoder synchronizeResource:_sampleRenderTex];
                [blitCommandEncoder endEncoding];
            }
        }
    }
    else {
        
        int32_t textureLookupMipX = _showSettings->textureLookupMipX;
        int32_t textureLookupMipY = _showSettings->textureLookupMipY;
        
        [self drawSamples:commandBuffer lookupX:textureLookupMipX lookupY:textureLookupMipY];
        
        // Synchronize the managed texture.
        id <MTLBlitCommandEncoder> blitCommandEncoder = [commandBuffer blitCommandEncoder];
        if (blitCommandEncoder) {
            [blitCommandEncoder synchronizeResource:_sampleComputeTex];
            [blitCommandEncoder endEncoding];
        }
    }
    
    // After synchonization, copy value back to the cpu
    id<MTLTexture> texture = isDrawableBlit ? _sampleRenderTex : _sampleComputeTex;
    
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer)
    {
        if (buffer.error != nil) {
            return;
        }
        // only 1 pixel in the texture right now
        float4 data;
        
        // copy from texture back to CPU, might be easier using MTLBuffer.contents
        MTLRegion region = {
            { 0, 0, 0 }, // MTLOrigin
            { 1, 1, 1 }  // MTLSize
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
    if (!renderEncoder)
        return;
    
    renderEncoder.label = @"SampleCompute";

    [renderEncoder pushDebugGroup:@"DrawShape"];

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
    
    [renderEncoder setTexture:_sampleComputeTex
                      atIndex:TextureIndexSamples];
    
    [renderEncoder setBytes:&uniforms length:sizeof(UniformsCS) atIndex:BufferIndexUniformsCS];
    
    // sample and copy back pixels off the offset
    [renderEncoder dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
    
    [renderEncoder popDebugGroup];
    [renderEncoder endEncoding];
}


- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    // Don't crashing trying to readback from the cached drawable during a resize.
    _lastDrawableTexture = nil;
    
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
    _projectionMatrix = orthographic_rhs(_showSettings->viewSizeX, _showSettings->viewSizeY, 0.1f, 100000.0f, _showSettings->isReverseZ);
    
    // DONE: adjust zoom to fit the entire image to the window
    _showSettings->zoomFit = MIN((float)_showSettings->viewSizeX,  (float)_showSettings->viewSizeY) /
        MAX(1, MAX((float)_showSettings->imageBoundsX, (float)_showSettings->imageBoundsY));
    
    // already using drawableSize which includes scale
    // TODO: remove contentScaleFactor of view, this can be 1.0 to 2.0f
    // why does this always report 2x even when I change monitor res.
    //_showSettings->zoomFit /= _showSettings->viewContentScaleFactor;
}


@end
