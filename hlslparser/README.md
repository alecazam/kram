HLSLParser
==========

This version of thekla/hlslparser takes a HLSL-like syntax and then converts that into modern HLSL and MSL.  Special thanks to Max McGuire (@Unknown Worlds) and Ignacio Castano and Johnathan Blow (@Thekla) for releasing this as open-source.  I've left out GLSL and DX9 legacy codegen to simplify maintaining the codebase.  This is a small amount of code compared with the Krhonos shader tools.

There are still growing pains to using hlslparser.  It can't do all the manipulation that glsc and spriv-cross does to ensure that a valid shader model shader is created.  But compiling with DXC/metal should help avoid issues.  I still don't know how to resolve reflection, since each compiler generates it's own unique data formats.  Reflection is simpler on the spirv path.

The point of this hlslparser is to preserve comments, generate MSL/HLSL code close to the original sources, and be easy to extend.  MSL and HLSL are nearly the same shader language at the core.  Typical spriv to MSL transpiles look  assembly-like in code flow.  Spriv-opt introduces 100's of temp registers into the code, gens 24 character floats, strips comments, and the resulting code isn't simple to step through in Metal GPU capture.  At the same time, Apple ignores generating Spirv from MSL, so here we are.  Spriv should remain a final assembly format to feed to Vulkan drivers.

---------------------------------

Paths to generate MSL from HLSL 

* Spirv   HLSL9 > hlslparser > HLSL10 > DXC > spv  
*  reflection                                 spv > spv-reflect -> ref
* MSL     HLSL9 > hlslparser > MSL    > metal > air/metallib
*
* Transpiling MSL: HLSL10 > DXC   > spv > spirv-cross > MSL
* Transpiling MSL: HLSL10 > glslc > spv > spirv-cross > MSL
*
*  assumes DXC/glslc do optimizations similar to spirv-opt
*
* Variant generation 
* HLSL9 > preprocess + defines > HLSL9
*  or
* HLSL9 + specialization > hlslparser

---------------------------------

TODO:
* compute shader support
* generate reflection data from parse of HLSL
* handle HLSL vulkan extension constructs, convert these to MSL kernels too
* variants from preprocess or vulkan specialization constants
* get tile shader kernels to work
* get SSBO to work
* fix shaders to not structify metal and mod the source names
* handle reflection (spirv-reflect?)
* poor syntax highlighting of output .metal file, does Xcode have to compile?
* no syntax highlighting of .hlsl files in Xcode, but VSCode has HLSL but not MSL

* May want to swtich to VSCode for shader development
* https://marketplace.visualstudio.com/items?itemName=doublebuffer.metal-shader&utm_source=VSCode.pro&utm_campaign=AhmadAwais

---------------------------------

Overview
---

|Code          |                   |
|--------------|-------------------|
|HLSLTokenizer | produces HLSLTree |
|HLSLParser    | produces HLSLTree |
|HLSLTree      | AST tree of the source HLSL |
|||
|MSLGenerator  | convert HLSLTree to MSL |
|HLSLGenerator | to a DX10-style HLSL |
|GLSLGenerator | unsupported |
|||
|Engine        | some string and log helpers |
|||
|ShaderMSL.h   | macros/functions to translate to MSL |
|ShaderHLSL.h  | to HLSL |
|||
|buildShaders  | build hlslparser first, and update ShaderMSL/HLSL.h files, then run this script.  Runs hlslparer to generate MSL/HLSL, then runs that through DXC/Metal compiler. | 
    
|Apps          |                   |
|--------------|-------------------|
|hlslparser | convert dx9 style HLSL to DX10 HLSL and MSL |
|DXC | Microsoft's open-source compiler, gens HLSL 6.0-6.6 DXIL, and spv1.0-1.2, clang-based optimizer, installed with Vulkan SDK |
|glslc | Google's wrapper to glslang, preprocessor, reflection, see below |
|glslang | GLSL and HLSL compiler, but doesn't compile valid HLSL half code |
|spirv-opt | spv optimizer |
|spriv-cross | transpile spv to MSL, HLSL, and GLSL, but codegen has 100's of temp vars, no comments, can target specific MSL/HLSL models |
|spirv-reflect | gens reflection data from spv file |

Dealing with Half
---
HLSL 6.2 includes full half and int support.   So that is the compilation target.  Note table below before adopting half in shaders.  Nvidia/AMD tried to phase out half support on DX10, but iOS re-popularized half usage.  Android and Nvidia have many dragons using half (see below).  Half is only 10-bit mantissa with 5-bit exponent.  

Platforms - iOS/PowerVR, Adreno, Mali,   Nvida, AMD, Intel

| Feature        | I | A | M | | N | A | I |
|----------------|---|---|---|-|---|---|---|
| Half Interp    | y | n | y | | n | y | ? |
| Half UBO       | y | n | y | | n | y | ? | 
| Half Push      | y | y | y | | y | n | ? |
| Half ALU       | y | y | y | | y | y | ? |

https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_16bit_storage.html

* StorageBuffer16BitAccess
* UniformAndStorageBuffer16BitAccess
* StoragePushConstant16
* StorageInputOutput16

There is also the limitation of half interpolation creating banding, and likely why Adreno/Nvidia do not support StorageInputOutput16.  Mali recommends using half to minimize parameter buffer storage out of the vertex shader, but then declaring float for the same variables in the fragment shader.  This limits sharing input/output structs.

macOS on M1 - Rosetta2 lacks f16c cpu support, so translated x64 apps crash.
  build Apple Silicon

Android missing cpu arm64+f16 support from Redmi Note 8 and other chips.
  vcvt_f32_f16 is still present without this.
  

Terms
---

* Shader Variants - it's good to define which variants of shaders to generate.  Can use static and dynamic branching to reduce variant count.  Can lead to requiring shader source if can't predefine variant count.
* Specialization Constants - allow variants to be generated within a single shader.  Spriv is marked and compiled based on these settings.  Metal has equivalent function constants
* Tile shaders - kernels/fragment shaders that run at the tile level.  Subpasses in Vulkan.  tilegroup memory to and tile data passed from stage to stage without writing back to targets.


Mobile HW
---

Mali
* TBDR
* Vulkan gpu - Midgard, Bifrost, Valhall, Immortalis
* Vulkan can read but not write SSBO in VS.
* Sparse index buffer limits 
* 180MB parameter buffer limit - device lost after exceeded
* Missing VK_POLYGON_MODE_LINE (feature.fillModeNonSolid) - affects debug visuals
* Missing firstInstance to use MDI and SBO (G52 adds this)
* ARM licenses mobile cpu reference designs
* ARM bought Mali gpu from Falanx Microsystems
* ETC2/ASTC, created ASTC format and encoders, no BC
* https://en.wikipedia.org/wiki/Mali_(processor)
* https://community.arm.com/support-forums/f/graphics-gaming-and-vr-forum/53672/vulkan-what-should-i-do-about-this-warning-bestpractices-vkcmddrawindexed-sparse-index-buffer

Adreno
* TBDR
* Occlusion queries can cause a switch from TBDR to IMR
* Half shader limits
* Qualcomm bought ATI Radeon mobile gpu
* 8K constant cache for UBO
* SSBO bypass cache
* lacks baseInstance (firstInstance) support
* ETC2/ASTC, no BC

PowerVR
* TBDR
* Very little US adoption
* Imagination absorbed into Chinese state tech conglomerate
* Origin of Apple Silicon
* PVRTC/ETC2/ASTC, no BC

iOS 
* TBDR
* locked at GL/ES 3.0 - no compute, use Metal/MSL instead
*  A7 - 2/0 cpu, ETC2, 1gb device, Metal introduced and ES3
*  A8 - 2/0, ASTC
*  A9 - 2/0 2gb, limited MDI, cpu ICB, baseVertex, baseInstance
* A10 - 2/0, 
* A11 - 2/2, gpu ICB, tile shaders, Raster Order Groups, MSAA improvements, more gpus, async compute/raster
* A12 - 2/4,
* A13 - Argument Buffer indirection for material indexing, sparse texturing
* A14/M1 - lossy FBO compression, mesh shaders,
* A15/M2 - no new gpu hw
* A16/M3? - rumored RT hw
* No SamplerMinMax support

* https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf

macOS
* IMR (Intel), TBDR(M1/A14), TBDR(A15/M2)
* locked at GL4.1 - no compute, clipControl, BC6/7, SSBO, dsa, error callbacks
* M1 has BC texture support, iPad/iPhone still do not
* Intel only has BC support, but architecture is being phased out
* Can use iOS tile shading on M1/M2, may work on last gen Intel?, last Intel RDNA and not RDNA2
* M1 can only run Windows ARM in Parallels VM
* Parallels Intel can't run DX12/Vulkan, only DX11
* Bootcamp can't run eGPU, but macOS can
* M1 can run iOS applications natively (but not ARKit)

Intel
* Skylake - improved fp16 support, sparse texturing
* Alderlake - removes AVX512, 8 HT big, 8 Little = 24 threads (really 16 cores)

AMD
* scalar execution instead of vector based, compute, unified ALUs for rasterization
* create Mantle API which lead to Vulkan
* GCN1  - wave64, 1 instr/4 cycles, 1 cu = 4 simd16 units
* GCN2  - Puma, PS4(Liverpool)/Xbone(Durango), sparse texturing, 2 async compute + 1 raster/compute pipe
* GCN3  - lossless DCC, high quality video scalar, video encoder/decoder
* GCN4  - Polaris, PS4 Pro(Neo)/XboneX(Scorpio), checkerboarding, degen tri removal, fp16 added back,
* GCN5  - Vega, fp16 2x perf of fp32, fp64 is 1/16 typically, mesh shaders, 
*
* RDNA  - RX5300, wave32 (or 64), 1 instr/cycle, 2cpu = 1 wgp, mesh shaders, wave32 = 1 simd32 unit, display compression, 
* RDNA2 - PS5/XboxX/SteamDeck, RX6000, RT hw, FSR
* RDNA3 - RX7000, better compute/RT hw, multichip module design,
*
* https://en.wikipedia.org/wiki/Graphics_Core_Next
* https://gpuopen.com/wp-content/uploads/2017/03/GDC2017-Advanced-Shader-Programming-On-GCN.pdf
* https://en.wikipedia.org/wiki/RDNA_(microarchitecture)
* https://en.wikipedia.org/wiki/List_of_AMD_graphics_processing_units

Nvidia
* scalar execution instead of vector based, compute, unified ALUs for rasterization
* tile-based raster/binning in Maxwell (Tegra X1), not on same level as TBDR
#
* Tegra X1+ - Mariko, Nintendo Switch, ended chip production in 2021
* 10x0 - no tensor core
* 20x0 - tensor cores, RT accel for triangle and bvh intersection
* 30x0 - 
* 40x0 - 

* https://en.wikipedia.org/wiki/List_of_Nvidia_graphics_processing_units
* https://www.pcmag.com/news/report-nvidia-to-end-production-of-nintendo-switch-tegra-chip

Graphics APIs
---
Metal
* Apple macOS/iOS only API.  Now on version 3.0
* Just works if hw supports. Easy to develop gpu algorithms.
* Argument buffer simplified in Metal 3.

Vulkan
* derived from AMD Mantle by Khronos to generalized API
* super complex low-level API making dev deal with layout, barriers, and lots of gpu garbage
* complex to enable features and extensions in api and in shaders
* Android 8/9 Vulkan drivers often locked to Vulkan 1.0 without no updates
* better on Windows/Linux where AMD/Intel/Nvidia update drivers
* cryptic validation errors improvement over glGetError()
* https://www.khronos.org/files/vulkan10-reference-guide.pdf
* https://vkguide.dev/
* https://github.com/SaschaWillems/Vulkan
* https://vulkan.gpuinfo.org/

DX9-DX12
* Microsoft Win only API
* DX12 went super low level for performance
* DX11 is a very nice api with shader stages and compute
* DX11 runs atop Parallels VM (which runs atop Metal)

OpenGL
* SGI developed IrisGL then OpenGL, 30 years ago, 
* stopped at GL4.6 on desktop, GL4.1 on macOS
* Shipped with GLSL
* ES mobile variants, better with extensions
* WebGL lowest common denominator browser variants, extensions often not implemented

WebGPU
* Metal/DX12/Vulkan lowest common denominator API for browser
* extensions to handle platform specifics
* using WGSL

Shading Languages
---

CG
* Where it all started.  HLSL is an offshoot of this.  MSL closely resembles HLSL.
* Nvida has abandoned this
* Unity was using but moved to HLSL
* https://developer.nvidia.com/cg-toolkit

MSL
* C++14, has ctors, cg/hlsl like
* buffers, textures, tile shaders, atomic int, 
* https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf

HLSL
* C++ like, missing ctors, cg derived for original Xbox
* Added back int/half support in SM 6.2
* Vulkan extensions for specialization constants and subpasses
* SSBO - StructuredBuffers, ByteAddressBuffer
* DX9 and DX10 style syntax differs
* HLSL added to clang
* https://github.com/Microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst#subpass-inputs
* https://clang.llvm.org/docs/HLSL/HLSLSupport.html#:~:text=HLSL%20uses%20templates%20to%20define,case%20and%20issues%20a%20diagnostic.

Shader Assembly Formats

SPIRV
* generated by DXC and glslang (glslc) for Vulkan to use
* assembly level constructs that can be transpiled back to other shading languges 
* really difficult to read
* clang optimizer
* linked into module
* cannot represent Texture2D<half4>, so can't tranpsile to MSL texture2d<half>
* https://github.com/microsoft/DirectXShaderCompiler/issues/2711
* https://www.khronos.org/spir/

AIR
* Apple IR format
* clang optimizer
* don't know of human readable version
* linked into metallib

DXIL
* generate by DXC
* used by DX12
* lacks specialization constant support
* clang optimizer


Shading languages not supported directly by hlslparser, but can transpile via spirv-cross or tint.

FX 
* wrapper around shaders, passes, state
* Microsoft abandoned this
* hlslparser has code to read pass/technique/etc.
* https://github.com/microsoft/FX11
* https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d11-effect-format

GLSL 
* dead shader language at GL 4.6, Vulkan/Microsoft will pursue HLSL
* bolted on extensions with Apple, AMD, Nvidia, Intel extending language
* has extension mechanism
* replaced with spirv
* horrible glGetError() requires sync of cpu/gpu
* locked at GL4.1 on macOS - no compute

GLSL/ES
* even more limited GLSL
* 3.0 on iOS, now emulated by Metal,
* Khronos support ends at 3.1, moved to Vulkan/spirv
* precision modifiers in ES for lowp (no support), mediump (might be fp16, fp24, fp32), highp (fp24 or fp32)
* replaced with spriv
* defaults needelessly removed from uniforms
* dot or .0 required on all floating point numbers or shader fails to compile, int vs. float
* line directives needlessly changed from GLSL

GLSL/ES (WebGL)
* imposed mobile limits on desktop - browers mostly running gpu on desktop parts, despite Apple adding ES2/3 extensions
* versions ES2 (WebGL1), ES3 (WebGL2), ES3.1 (WebGL2.1)
* lowest common denominator
* wrote Figma using WebGL1, shaders rendering 2d vectors with Photoshop-style blend modes/masks, images
* WebGL1 limited ES2 even more - npot 2d texture limits, no 3d textures, srgb missing for compresed texture, extension not on Safari, no fp64, no dynamic loops in shaders
* WebGL2/2.1 - limited ES3/3.1, added srgb, 
* fixed count loops, can break out of loop with uniform
* WebGL2.1(ES3.1) can run compute, but not on Apple platforms
* now emualted by Metal.  Angle emulates atop DX11 instead of GL for driver stability, might also have Vulkan backend.
* runs all gpu code in separate browser process, so has to marshall all data over
* can't map/unmap, have to upload changes to buffer/texture
* rewrites shader names and code internally into unreadable mess.  Complicates stepping through shader in gpu captures.
* https://www.khronos.org/webgl
* https://webglreport.com/
* https://www.khronos.org/files/webgl/webgl-reference-card-1_0.pdf
* https://www.khronos.org/files/webgl20-reference-guide.pdf

WGSL (WebGPU)
* WebGPU shading language orignally meant as text form of spriv
* full compute support
* now using Dart like syntax completely unlike CG origin of other languages
* avoids pointers/references
* can transpile spirv to WGSL via tint, WGSL still not in spirv-cross
* converts WGSL back to spriv.
* supposedly Apple didn't want to require spirv.
* similar in api syntax to Metal/Vulkan/DX12
* https://www.w3.org/TR/webgpu/
* https://www.w3.org/TR/WGSL/

WHLSL (WebGPU)
* WebGPU dropped proposed language that would have been an offshoot of HLSL syntax.
* https://webkit.org/blog/8482/web-high-level-shading-language/

---------------------------------

Shading Language Versions

HLSL
* SM 6.2, target, added back int/half support
* SM 6.6,
* SM 6.7, SampleCmpLevel, RWTexture2DMS, RWTexture2DMSArray, signed texture offsets
* https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_7_Advanced_Texture_Ops.html

MSL
* metal2.2, iOS13/macOS10.15,
* metal2.3, iOS14/macOS11, target, function pointers,
* metal2.4, iOS15/macOS12,
* metal3.0, iOS16/macOS13, unified shader model, latest,

Spirv
* 1.1, vulkan1.0
* 1.3, vulkan1.1
* 1.5, vulkan1.2, target,

---------------------------------

This is a fork of [Unknownworld's hlslparser](https://github.com/unknownworlds/hlslparser) adapted to our needs in [The Witness](http://the-witness.net). We currently use it to translate pseudo-HLSL shaders (using the legacy D3D9 syntax) to HLSL10 and Metal Shading Language (MSL). There's also a GLSL translator available that we do not use yet, but that is being maintained by community contributions.

The HLSL parser has been extended with many HLSL10 features, but retaining the original HLSL C-based syntax.

For example, the following functions in our HLSL dialect:

```C
float tex2Dcmp(sampler2DShadow s, float3 texcoord_comparevalue);
float4 tex2DMSfetch(sampler2DMS s, int2 texcoord, int sample);
int2 tex2Dsize(sampler2D s);
```

Are equivalent to these methods in HLSL10:

```C++
float Texture2D::SampleCmp(SamplerComparisonState s, float2 texcoord, float comparevalue);
float4 Texture2DMS<float4>::Load(int2 texcoord, int sample);
void Texture2D<float4>::GetDimensions(out uint w, out uint h);
```



Here are the original release notes:


> HLSL Parser and GLSL code generator
>
> This is the code we used in Natural Selection 2 to convert HLSL shader code to
GLSL for use with OpenGL. The code is pulled from a larger codebase and has some
dependencies which have been replaced with stubs. These dependencies are all very
basic (array classes, memory allocators, etc.) so replacing them with our own
equivalent should be simple if you want to use this code.
>
> The parser is designed to work with HLSL code written in the legacy Direct3D 9
style (e.g. D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY should be used with D3D11).
The parser works with cbuffers for uniforms, so in addition to generating GLSL,
there is a class provided for generating D3D9-compatible HLSL which doesn't
support cbuffers. The GLSL code requires version 3.1 for support of uniform blocks.
The parser is designed to catch all errors and generate "clean" GLSL which can
then be compiled without any errors.
>
> The HLSL parsing is done though a basic recursive descent parser coded by hand
rather than using a parser generator. We believe makes the code easier to
understand and work with.
>
> To get consistent results from Direct3D and OpenGL, our engine renders in OpenGL
"upside down". This is automatically added into the generated GLSL vertex shaders.
>
> Although this code was written specifically for our use, we hope that it may be
useful as an educational tool or a base for someone who wants to do something
similar.
