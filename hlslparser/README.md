HLSLParser
==========

This version of uknown worlds and thekla/hlslparser takes a HLSL-like syntax and then converts that into modern HLSL and MSL.  Special thanks to Max McGuire (@Unknown Worlds) and Ignacio Castano and Johnathan Blow (@Thekla) for releasing this as open-source.  I've left out GLSL compilation, and legacy DX9 HLSL codegen to simplify maintaining the codebase.

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
HLSL 6.2 includes full half and int support.   So that is the compilation target.  Note table below before adopting half in shaders.  Nvidia/AMD tried to phase out half support on DX10, but iOS re-popularized half usage.  Android has many dragons using half (see below)

Platforms - iOS/PowerVR, Adreno,  Mali, Nvida, AMD

| Feature        | I | A | M | N | A |
|----------------|---|---|---|---|---|
| Half Interp    | y | n | y | n | y |
| Half UBO       | y | n | y | n | y | 
| Half Push      | y | y | y | y | n |
| Half ALU       | y | y | y | y | y |

https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_16bit_storage.html

* StorageBuffer16BitAccess
* UniformAndStorageBuffer16BitAccess
* StoragePushConstant16
* StorageInputOutput16

There is also the limitation of half interpolation creating banding, and likely why Adreno/Nvidia do not support StorageInputOutput16.  Mali recommends using half to minimize parameter buffer storage out of the vertex shader, but then declaring float for the same variables in the fragment shader.  This limits sharing input/output structs.

Terms
---

* Shader Variants - it's good to define which variants of shaders to generate.  Can use static and dynamic branching to reduce variant count.  Can lead to requiring shader source if can't predefine variant count.
* Specialization Constants - allow variants to be generated within a single shader.  Spriv is marked and compiled based on these settings.  Metal has equivalent function constants
* Tile shaders - kernels/fragment shaders that run at the tile level.  Subpasses in Vulkan.  tilegroup memory to and tile data passed from stage to stage without writing back to targets.


Mobile HW
---

Mali
* TBDR
* Cannot write SSBO in the vertex shader, but can in the fragment shader.  Vulkan only.
* Sparse index buffer limits 
  https://community.arm.com/support-forums/f/graphics-gaming-and-vr-forum/53672/vulkan-what-should-i-do-about-this-warning-bestpractices-vkcmddrawindexed-sparse-index-buffer
* 180MB parameter buffer limit - device lost after exceeded
* Missing fillAsLines - affects debug visuals
* Missing firstInstance to use MDI and SBO
* ARM also makes the cpu reference designs
* ARM bought from Falanx Microsystems

Adreno
* TBDR
* Occlusion queries can cause a switch from TBDR to IMR
* Half shader limits
* Formerly ATI Radeon mobile parts - division sold to Qualcomm

PowerVR
* TBDR
* Very little US adoption
* Imagination absorbed into Chinese state tech conglomerate
* Origin of Apple Silicon
* Only had PVRTC support, but now has ASTC

iOS 
* TBDR
* introduced Metal on iPhone 5S (A7)
* No SamplerMinmax 
*  A7 - ETC2 support
*  A8 - ASTC support
*  A9 - MDI api limited, cpu ICB
* A11 - gpu ICB, tile shaders (no HLSL), Raster Order Groups, MSAA controls
* A13 - Argument Buffers indirection
* A14/M1 -  
* A15/M2 -

macOS
* IMR (Intel), TBDR(M1/A14), TBDR(A15/M2)
* locked at GL4.1 - no compute, clipControl, BC6/7 support, SSBO
* M1 has BC texture support, iPad/iPhone still do not
* Intel only has BC support, but is being phased out
* Can use iOS tile shading on M1/M2, may work on last gen Intel?

* https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf

Shading Languages
---

CG
* Where it all started.  HLSL is an offshoot of this.  MSL closely resembles HLSL.

MSL
* https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf

HLSL
* Added back int/half support
* Vulkan extensions for specialization constants and subpasses
* SSBO - StructuredBuffers, ByteAddressBuffer
* DX9 and DX10 style syntax changes
* HLSL being added to clang

* https://github.com/Microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst#subpass-inputs
* https://clang.llvm.org/docs/HLSL/HLSLSupport.html#:~:text=HLSL%20uses%20templates%20to%20define,case%20and%20issues%20a%20diagnostic.

These languages not supported directly by hlslparser, but can transpile via spirv-cross.

GLSL 
* dead shader language, Vulkan/Microsoft will pursue HLSL
* bolted on extensions with Apple, AMD, Nvidia, Intel extending language
* has extension mechanism
* replaced with spirv

GLSL/ES
* even more limited 
* precision modifiers in ES for lowp (no support), mediump (might be fp16, fp24, fp32), highp (fp24 or fp32)
* replaced with spriv

WGSL
* WebGPU shading language orignally meant as text form of spriv
* Now using Dart like syntax completely unlike CG origin of other languages
* Avoids pointers/references
* Can transpile spirv to WGSL via tint, WGSL still not in spirv-cross

WHLSL
* WebGPU dropped language that would have been an offshoot of HLSL syntax.

---------------------------------

Shading Language Versions

HLSL
* SM 6.2, target, added back int/half support
* SM 6.6, latest,

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

TODO:
* compute shader support
* handle HLSL vulkan extensions, convert these to MSL kernels too
* preprocessed variants
* fix shader input names
* handle reflection (spirv-reflect?)

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
