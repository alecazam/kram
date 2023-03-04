HLSLParser
==========

This version of uknown worlds and thekla/hlslparser takes a HLSL-like syntax and then converts that into modern HLSL and MSL.  Special thanks to Max McGuire (@Unknown Worlds) and Ignacio Castano and Johnathan Blow (@Thelka) for releasing this as open-source.  I've left out GLSL compilation, and legacy DX9 HLSL codegen to simplify maintaining the codebase.

Overview
---
HLSLTokenizer - produces HLSLTree
HLSLParser - produces HLSLTree
HLSLTree - AST tree of the source HLSL

MSLGenerator - convert HLSLTree to MSL
HLSLGenerator - to a DX10-style HLSL
GLSLGenerator - unsupported

Engine - some string and log helpers

ShaderMSL.h - macros/functions to translate to MSL
ShaderHLSL.h - to HLSL

buildShaders.sh - build hlslparser first, and update ShaderMSL/HLSL.h files, then run this script.   It will run hlslparer to generate outputs, and then run that through DXC and the Metal compiler.  On macOS, install VulkanSDK as a faster path to DXC.

About Shader Models
---
HLSL 6.2 includes full half and int support.   So that is the compilation target.  But note the table below before adopting half in shaders.  Nvidia/AMD tried to phase out half support on the desktop, but iOS re-popularized half usage.  Android has many dragons using half (see below)

Platforms - iOS, Adreno,  Mali, Nvida, AMD
| Feature        | I | A | M | N | A |
|----------------|---|---|---|---|---|
| Half Interp    | y | n | y | n | y |
| Half UBO       | y | n | y | n | y | 
| Half Push      | y | y | y | y | n |
| Half ALU       | y | y | y | y | y |

https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_16bit_storage.html

StorageBuffer16BitAccess
UniformAndStorageBuffer16BitAccess
StoragePushConstant16
StorageInputOutput16

There is also the limitation of half interpolation creating banding, and likely why Adreno/Nvidia do not support StorageInputOutput16.  Mali recommends using half to minimize parameter buffer storage out of the vertex shader, but then declaring float for the same variables in the fragment shader.  This limits sharing input/output structs.

Android Gotchas
---

Mali cannot write SSBO in the vertex shader, but can in the fragment shader.  Only Vulkan supports readonly SSBO so no ES support.
Mali sparse index buffer limits 
  https://community.arm.com/support-forums/f/graphics-gaming-and-vr-forum/53672/vulkan-what-should-i-do-about-this-warning-bestpractices-vkcmddrawindexed-sparse-index-buffer
Mali 180MB parameter buffer limit - device lost after exceeded
Mali missing fillAsLines - affects debug visuals
Mali is missing firstInstance to use MDI and SBO

Adreno
Occlusion queries can cause a switch from TBDR to IMR

iOS 
Has no SamplerMinmax 
 A9 - limited MultiDrawInidirect that can only draw 1 element per draw
      cpu ICB
A11 - gpu ICB
A13 - Argument Buffers indirection

Shader Variants - it's good to define which variants of shaders to generate.  Can use static and dynamic branching to reduce variant count.  Can lead to requiring shader source if can't predefine variant count.

Specialization Constants - allow variants to be generated within a single shader.  Spriv is marked and compiled based on these settings.  Metal has equivalent function constants

Unlike GLSL, HLSL and MSL don't have an extension mechansims.  These shader models update in revisions. This is good and bad, but in general it simplfies picking the target for shader codegen.   These 

HLSL
missing specialization constants - Vulkan adds these in for Spirv, but it's not valid HLSL for DX12

SM 6.2 adds back half and short support

MSL
SM 2.1
SM 2.2
SM 2.3
SM 2.4

Vulkan
SM spv1.1
SM spv1.3
SM spv1.5

HLSL 
SM 6.2 - added back int/half support
  
TODO:
compute shader support
preprocessed variants
fix shader input names
handle reflection (spirv-reflect?)

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
