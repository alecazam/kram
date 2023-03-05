#!/bin/bash

mkdir -p out

pushd out

# display commands
# set -x

# TODO: consider putting in path
# note bash can't expand tilda, so using HOME instead
vulkanSDK="${HOME}/devref/vulkansdk/1.3.239.0/macOS/bin/"

appHlslparser=../build/hlslparser/Build/Products/Release/hlslparser
appDxc=${vulkanSDK}
appGlslc=${vulkanSDK}
appDxc+="dxc"
appGlslc+="glslc"

# copy over the headers that translate to MSL/HLSL
cp ../shaders/ShaderMSL.h .
cp ../shaders/ShaderHLSL.h .


# build the metal shaders
echo gen MSL
${appHlslparser} -i ../shaders/Skinning.hlsl -o Skinning.metal

# build the hlsl shaders
echo gen HLSL
${appHlslparser} -i ../shaders/Skinning.hlsl -o Skinning.hlsl

# see if HLSL compiles (requires macOS Vulkan install)
# this will pull /usr/bin/local/dxc
# looks like DXC wants a ps/vs/cs profile, so is expecting one shader per file


# see if MSL compile
echo compile MSL
xcrun -sdk macosx metal Skinning.metal -o Skinning.metallib

args="-nologo "

# debug
# args+="-Zi "

# column matrices
args+="-Zpc "

# enable half instead of min16float
# can't use for input/output on Adreno or Nvidia
# can't use in push constants on AMD
# also watch interpolation if using for input/output
args+="-enable-16bit-types "

# have to also compile to 6.2
vsargs=${args}
vsargs+="-T vs_6_2 "

psargs=${args}
psargs+="-T ps_6_2 "

#echo ${vsargs}
#echo ${psargs}

# more HLSL garbage decoded
# https://therealmjp.github.io/posts/shader-fp16/
# https://therealmjp.github.io/posts/dxil-linking/

# first gen dxil to see if HLSL is valid
# see this garbage here.  Can only sign dxil on Windows.
# dxc only loads DXIL.dll on Windows
#  https://www.wihlidal.com/blog/pipeline/2018-09-16-dxil-signing-post-compile/
echo gen DXIL with dxc
${appDxc} ${vsargs} -E SkinningVS -Fo Skinning.vert.dxil Skinning.hlsl
${appDxc} ${psargs} -E SkinningPS -Fo Skinning.vert.dxil Skinning.hlsl

# 1.0,1.1,1.2 is spv1.1,1.3,1.5
#echo gen SPIRV 1.0
#${appDxc} ${vsargs} -spirv -fspv-target-env=vulkan1.0 -E SkinningVS -Fo Skinning.vert.spv Skinning.hlsl
#${appDxc} ${psargs} -spirv -fspv-target-env=vulkan1.0 -E SkinningPS -Fo Skinning.frag.spv Skinning.hlsl

#echo gen SPIRV 1.1
#${appDxc} ${vsargs} -spirv -fspv-target-env=vulkan1.1 -E SkinningVS -Fo Skinning.vert.spv1 Skinning.hlsl
#${appDxc} ${psargs} -spirv -fspv-target-env=vulkan1.1 -E SkinningPS -Fo Skinning.frag.spv1 Skinning.hlsl

echo gen SPIRV 1.2 with dxc
${appDxc} ${vsargs} -spirv -fspv-target-env=vulkan1.2 -E SkinningVS -Fo Skinning.vert.spv2 -Fc Skinning.vert.spv2.txt Skinning.hlsl
${appDxc} ${psargs} -spirv -fspv-target-env=vulkan1.2 -E SkinningPS -Fo Skinning.frag.spv2 -Fc Skinning.frag.spv2.txt Skinning.hlsl

vsargs="-Os -fshader-stage=vert --target-env=vulkan1.2 "
psargs="-Os -fshader-stage=vert --target-env=vulkan1.2 "

# turn on half/short/ushort support
# glslc fails when I set this even on latest build.
#   Skinning.hlsl:24: error: '' : could not create assignment
#   Skinning.hlsl:24: error: 'declaration' : Expected
#   Skinning.hlsl: Skinning.hlsl(24): error at column 43, HLSL parsing failed.
# seems that dot, min, max and other calls don't have half3 versions needed
# vsargs+="-fhlsl-16bit-types "
# psargs+="-fhlsl-16bit-types "

# see SPV_GOOGLE_hlsl_functionality1
# -fhlsl_functionality1
# -g source level debugging info
# -I include search path

# note: glsl has a preprocesor
echo gen SPRIV 1.2 with glslc
${appGlslc} ${vsargs} -fentry-point=SkinningVS Skinning.hlsl -o Skinning.vert.gspv2
${appGlslc} ${psargs} -fentry-point=SkinningPS Skinning.hlsl -o Skinning.frag.gspv2

# TODO: need to enable half (float16_t) usage in spriv generated shaders
# how to identify compliation is targeting Vulkan?

# barely human readable spv assembly listing
${appGlslc} -S ${vsargs} -fentry-point=SkinningVS Skinning.hlsl -o Skinning.vert.gspv2.txt
${appGlslc} -S ${psargs} -fentry-point=SkinningPS Skinning.hlsl -o Skinning.frag.gspv2.txt


# TODO: need to group files into library/module
# also create a readable spv file, so can look through that


# TODO: create reflect data w/o needing spriv

# here are flags to use

# dxc can output reflection directly
# -Fre <file>             Output reflection to the given file
  
# may not need this if doing dxil output, then -Fo might gen dxil asm listing
# -Cc  color-coded assmebly listing
  
# -remove-unused-functions Remove unused functions and types
# -remove-unused-globals  Remove unused static globals and functions
  
# add reflect data
# -fspv-reflect

# Negate SV_Position.y before writing to stage output in VS/DS/GS to accommodate Vulkan's coordinate system
# -fvk-invert-y

# Reciprocate SV_Position.w after reading from stage input in PS to accommodate the difference between Vulkan and DirectX
# -fvk-use-dx-position-w

# layout
# -fvk-use-gl-layout      Use strict OpenGL std140/std430 memory layout for Vulkan resources
# -fvk-use-scalar-layout  Use scalar memory layout for Vulkan resources
 
# -Zpc                    Pack matrices in column-major order
# -Zpr                    Pack matrices in row-major order
  
# -WX                     Treat warnings as errors
# -Zi                     Enable debug information
  
# TODO: transpile with spriv-cross to WGSL, GLSL, etc off the spirv.

# -enable-16bit-types     Enable 16bit types and disable min precision types. Available in HLSL 2018 and shader model 6.2
# -Fc <file>              Output assembly code listing file

# this prints cwd if not redirected
popd > /dev/null
