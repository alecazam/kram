#!/bin/bash

mkdir -p out

pushd out

# display commands
set -x

app=../build/hlslparser/Build/Products/Release/hlslparser

# copy over the headers that translate to MSL/HLSL
cp ../shaders/ShaderMSL.h .
cp ../shaders/ShaderHLSL.h .


# build the metal shaders
echo gen MSL
${app} -i ../shaders/Skinning.hlsl -o Skinning.metal

# build the hlsl shaders
echo gen HLSL
${app} -i ../shaders/Skinning.hlsl -o Skinning.hlsl

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
echo compile HLSL
dxc ${vsargs} -E SkinningVS -Fo SkinningVS.dxil Skinning.hlsl
dxc ${psargs} -E SkinningPS -Fo SkinningPS.dxil Skinning.hlsl

#echo gen SPIRV 1.0
#dxc ${vsargs} -spirv -fspv-target-env=vulkan1.0 -E SkinningVS -Fo SkinningVS.vert.spv Skinning.hlsl
#dxc ${psargs} -spirv -fspv-target-env=vulkan1.0 -E SkinningPS -Fo SkinningPS.frag.spv Skinning.hlsl

#echo gen SPIRV 1.1
#dxc ${vsargs} -spirv -fspv-target-env=vulkan1.1 -E SkinningVS -Fo SkinningVS.vert.spv1 Skinning.hlsl
#dxc ${psargs} -spirv -fspv-target-env=vulkan1.1 -E SkinningPS -Fo SkinningPS.frag.spv1 Skinning.hlsl

echo gen SPIRV 1.2
dxc ${vsargs} -spirv -fspv-target-env=vulkan1.2 -E SkinningVS -Fo SkinningVS.vert.spv2 Skinning.hlsl
dxc ${psargs} -spirv -fspv-target-env=vulkan1.2 -E SkinningPS -Fo SkinningPS.frag.spv2 Skinning.hlsl

# TODO: need to group files into library/module
# also create a readable spv file, so can look through that


# TODO: create reflect data w/o needing spriv

# here are flags to use

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

popd
