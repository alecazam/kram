#!/bin/bash

mkdir -p out

mkdir -p out/mac
mkdir -p out/win
mkdir -p out/android

# mkdir -p out/ios

# for glslc testing
#mkdir -p out/android2

pushd outshaders

# display commands
# set -x

# TODO: consider putting in path
# note bash can't expand tilda, so using HOME instead
vulkanSDK="${HOME}/devref/vulkansdk/1.3.239.0/macOS/bin/"

appHlslparser=../build/hlslparser/Build/Products/Release/hlslparser
appDxc=${vulkanSDK}
appGlslc=${vulkanSDK}
appSprivReflect=${vulkanSDK}
appDxc+="dxc"
appGlslc+="glslc"
appSprivReflect+="spirv-reflect"

#-------------------------------

# copy over the headers that translate to MSL/HLSL
cp ../shaders/ShaderMSL.h .
cp ../shaders/ShaderHLSL.h .

parserOptions=""

# preserve comments
parserOptions+="-g "

# build the metal shaders
echo gen MSL
${appHlslparser} ${parserOptions} -i ../shaders/Skinning.hlsl -o Skinning.metal

# build the hlsl shaders
echo gen HLSL
${appHlslparser} ${parserOptions} -i ../shaders/Skinning.hlsl -o Skinning.hlsl

#-------------------------------

popd > /dev/null

pushd out

#-------------------------------

# Metal is C++14

# see if HLSL compiles to MSL (requires macOS Vulkan install)

# record sources into code for gpu capture (don't ship this), debug mode
metalMacOptions="-frecord-sources -g "

# O2 + size opt
# metalMacOptions+="-Os"

# TODO: metal3.0 on M1 macOS13/iOS16
metalMacOptions+="-std=macos-metal2.3 "

# see if MSL compile
echo compile MSL for macOS
xcrun -sdk macosx metal ../outshaders/Skinning.metal ${metalMacOptions} -o mac/Skinning.metallib

# metaliosOptions="-frecord-sources -g "
# metaliosOptions+="-std=ios-metal2.3 "

#echo compile MSL for iOS
#xcrun -sdk macosx metal ../outshaders/Skinning.metal ${metaliosOptions} -o ios/Skinning.metallib

#-------------------------------


# looks like DXC wants a ps/vs/cs profile, so is expecting one shader per output

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

# default is 2018, but 2021 fixes casting rules of structs with same args
# https://devblogs.microsoft.com/directx/announcing-hlsl-2021/
args+="-HV 2021 "
 
args+="-fspv-extension=SPV_KHR_shader_draw_parameters "

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
# no idea what format the refl file from dxil is?
#echo gen DXIL with dxc
#${appDxc} ${vsargs} -E SkinningVS -Fo win/Skinning.vert.dxil -Fc win/Skinning.vert.dxil.txt -Fre win/Skinning.vert.refl Skinning.hlsl
#${appDxc} ${psargs} -E SkinningPS -Fo win/Skinning.frag.dxil -Fc win/Skinning.frag.dxil.txt -Fre win/Skinning.frag.refl Skinning.hlsl


# Optimization is also delegated to SPIRV-Tools.
# Right now there are no difference between optimization levels greater than zero;
# they will all invoke the same optimization recipe. That is, the recipe behind spirv-opt -O.
# -Os is a special set of options.  Can run custom spriv optimizations via
# -Oconfig=--loop-unroll,--scalar-replacement=300,--eliminate-dead-code-aggressive

# 1.0,1.1,1.2 default to spv1.1,1.3,1.5
echo gen SPIRV 1.2 with dxc
${appDxc} ${vsargs} -spirv -fspv-target-env=vulkan1.2 -E SkinningVS -Fo android/Skinning.vert.spv -Fc android/Skinning.vert.spv.txt ../outshaders/Skinning.hlsl
${appDxc} ${psargs} -spirv -fspv-target-env=vulkan1.2 -E SkinningPS -Fo android/Skinning.frag.spv -Fc android/Skinning.frag.spv.txt ../outshaders/Skinning.hlsl

# -Fre not supported with spriv, so just use spirv-reflect
# either yaml or random format, why can't this just output json?
${appSprivReflect} -y android/Skinning.vert.spv > android/Skinning.vert.refl
${appSprivReflect} -y android/Skinning.frag.spv > android/Skinning.frag.refl

# skip this path, have to mod hlsl just to get valid code to compile with glslc
testGlslc=0

if [[ $testGlslc -eq 1 ]]; then
    vsargs="-Os -fshader-stage=vert --target-env=vulkan1.2 "
    psargs="-Os -fshader-stage=frag --target-env=vulkan1.2 "

    # TODO: probably no equivlent to this?
    # vsargs+="-HV 2021 "
    # psargs+="-HV 2021 "
    
    # turn on half/short/ushort support
    # TODO: seems that dot, min, max and other calls don't have half3 versions needed, casts required
    # and even half3(half) isn't valid.
    # https://github.com/google/shaderc/issues/1309
    vsargs+="-fhlsl-16bit-types "
    psargs+="-fhlsl-16bit-types "

    # see SPV_GOOGLE_hlsl_functionality1
    # -fhlsl_functionality1
    # -g source level debugging info
    # -I include search path
    # note: glsl has a preprocesor
    
    echo gen SPRIV 1.2 with glslc
    ${appGlslc} ${vsargs} -fentry-point=SkinningVS -o android2/Skinning.vert.spv Skinning.hlsl
    ${appGlslc} ${psargs} -fentry-point=SkinningPS -o android2/Skinning.frag.spv Skinning.hlsl

    # TODO: need to enable half (float16_t) usage in spriv generated shaders
    # how to identify compliation is targeting Vulkan?

    # barely human readable spv assembly listing
    ${appGlslc} -S ${vsargs} -fentry-point=SkinningVS -o android2/Skinning.vert.spv.txt Skinning.hlsl
    ${appGlslc} -S ${psargs} -fentry-point=SkinningPS -o android2/Skinning.frag.spv.txt Skinning.hlsl
fi

# TODO: need to group files into library/module
# also create a readable spv file, so can look through that

# TODO: create reflect data w/o needing spriv

# here are flags to use w/DXC

# dxc can output reflection directly (only for DXIL)
# -Fre <file>             Output reflection to the given file
  
# add reflect data to aid in generating reflection data
# -fspv-reflect

# may not need this if doing dxil output, then -Fo might gen dxil asm listing
# -Cc  color-coded assembly listing
  
# -remove-unused-functions Remove unused functions and types
# -remove-unused-globals  Remove unused static globals and functions
  

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
