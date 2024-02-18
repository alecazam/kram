#!/bin/bash

mkdir -p out

mkdir -p out/mac
mkdir -p out/win
mkdir -p out/android
mkdir -p out/ios

# display commands
set -x

# note bash can't expand tilda, so using HOME instead
# This only works if running from terminal, and not from Xcode
#  so back to hardcoding the path.
vulkanSDK="${HOME}/devref/vulkansdk/1.3.275.0/macOS/bin/"
#vulkanSDK=""

projectDir="${HOME}/devref/kram/hlslparser/"

srcDir=${projectDir}
srcDir+="shaders/"

includeDir=${srcDir}

dstDir=${projectDir}
dstDir+="out/"

#dstDirOut=${projectDir}
#dstDirOut+="out/"

# this only pulls the release build, so testing debug won't update
appHlslparser=${projectDir}build/hlslparser/Build/Products/Release/hlslparser

appDxc=${vulkanSDK}
appGlslc=${vulkanSDK}
appSpirvReflect=${vulkanSDK}
appSpirvCross=${vulkanSDK}

# compilers
appDxc+="dxc"
appGlslc+="glslc"
appMetalMac="xcrun -sdk macosx metal"
# sdk doesn't seem to need to be iphoneos
appMetaliOS="xcrun -sdk macosx metal"

# reflect/transpile spv
appSpirvReflect+="spirv-reflect"
appSpirvCross+="spirv-cross"

# this has to be installed from online installer, this is v1.1
appMetalShaderConverter="metal-shaderconverter"

# TODO: also use the metal tools on Win to build
# and already have vulkan sdk

# Xcode will only do clickthrough to warnings/errors if the filename
# is a full path.  That's super annoying.

parserOptions=""

# preserve comments
parserOptions+="-g -line "

# build the metal shaders - mac
pushd out/mac

echo gen MSL
${appHlslparser} ${parserOptions} -i ${srcDir}Skinning.hlsl -o Skinning.metal
${appHlslparser} ${parserOptions} -i ${srcDir}Sample.hlsl -o Sample.metal
${appHlslparser} ${parserOptions} -i ${srcDir}Compute.hlsl -o Compute.metal

popd > /dev/null

# build the metal shaders - ios
pushd out/ios

echo gen MSL
${appHlslparser} ${parserOptions} -i ${srcDir}Skinning.hlsl -o Skinning.metal
${appHlslparser} ${parserOptions} -i ${srcDir}Sample.hlsl -o Sample.metal
${appHlslparser} ${parserOptions} -i ${srcDir}Compute.hlsl -o Compute.metal

popd > /dev/null

# build the hlsl shaders - android
pushd out/android

echo gen HLSL
${appHlslparser} ${parserOptions} -i ${srcDir}Skinning.hlsl -o Skinning.hlsl
${appHlslparser} ${parserOptions} -i ${srcDir}Sample.hlsl -o Sample.hlsl
${appHlslparser} ${parserOptions} -i ${srcDir}Compute.hlsl -o Compute.hlsl

popd > /dev/null

# build the hlsl shaders - win
pushd out/win

echo gen HLSL
${appHlslparser} ${parserOptions} -i ${srcDir}Skinning.hlsl -o Skinning.hlsl
${appHlslparser} ${parserOptions} -i ${srcDir}Sample.hlsl -o Sample.hlsl
${appHlslparser} ${parserOptions} -i ${srcDir}Compute.hlsl -o Compute.hlsl

popd > /dev/null


#-------------------------------

# DONE: metal3.0 on M1 macOS13/iOS16
# record sources into code for gpu capture (don't ship this), debug mode

# O2 + size opt
# metalMacOptions+="-Os"

testMetal=1

if [[ $testMetal -eq 1 ]]; then
    # Metal is C++14
    metalMacOptions="-frecord-sources -g "
    metalMacOptions+="-std=metal3.0 "

    # see if HLSL compiles to MSL (requires macOS Vulkan install)
    
    echo compile mac to metallib
    ${appMetalMac} ${metalMacOptions} -I ${includeDir} -o ${dstDir}mac/GameShaders.metallib   ${dstDir}mac/Skinning.metal ${dstDir}mac/Sample.metal ${dstDir}mac/Compute.metal


    metaliOSOptions="-frecord-sources -g "
    metaliOSOptions+="-std=metal3.0 "

    echo compile iOS to metallib
    ${appMetaliOS} ${metaliOSOptions} -I ${includeDir} -o ${dstDir}ios/GameShaders.metallib   ${dstDir}ios/Skinning.metal ${dstDir}ios/Sample.metal ${dstDir}ios/Compute.metal
fi



#-------------------------------


# looks like DXC wants a ps/vs/cs profile, so is expecting one shader per output

args="-nologo "

# if this is left out the transpiled MSL has no var names
# debug
args+="-Zi "

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

# 6.1 for ConstantBuffer
# 6.2 for u/short and half <- target
# 6.6 adds u/char8 pack/unpack calls
vsargs=${args}
vsargs+="-T vs_6_2 "

psargs=${args}
psargs+="-T ps_6_2 "

csargs=${args}
csargs+="-T cs_6_2 "

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

if [[ $testMetal -eq 1 ]]; then

    pushd out/win

    # echo gen DXIL with dxc
    
    # TODO: how to link metallib to single metallib?
    # can this build to AIR, then build that into metallib?

    # Note this isn't a win file
    mscArgsVS="--vertex-stage-in --positionInvariance "
    # --enable-gs-ts-emulation  --vertex-input-layout-file=<string>
    
    mscArgsPS=" "

    mscArgsMac="--minimum-gpu-family=Metal3 --deployment-os=macOS --minimum-os-build-version=13.0.0 "
    mscArgsiOS="--minimum-gpu-family=Metal3 --deployment-os=iOS --minimum-os-build-version=16.0.0 "

    # build vert
    ${appDxc} ${vsargs} -I ${includeDir} -E SkinningVS -Fo Skinning.vert.dxil -Fc Skinning.vert.dxil.txt -Fre Skinning.vert.refl Skinning.hlsl
    
    ${appMetalShaderConverter} ${mscArgsMac} ${mscArgsVS} Skinning.vert.dxil -o Skinning.vert.mac.metallib
    ${appMetalShaderConverter} ${mscArgsiOS} ${mscArgsVS} Skinning.vert.dxil -o Skinning.vert.ios.metallib
    
    # build frag
    ${appDxc} ${psargs} -I ${includeDir} -E SkinningPS -Fo Skinning.frag.dxil -Fc Skinning.frag.dxil.txt -Fre Skinning.frag.refl Skinning.hlsl
    
    ${appMetalShaderConverter} ${mscArgsMac} ${mscArgsPS} Skinning.frag.dxil -o Skinning.frag.mac.metallib
    ${appMetalShaderConverter} ${mscArgsiOS} ${mscArgsPS} Skinning.frag.dxil -o Skinning.frag.ios.metallib
    
    popd > /dev/null
fi

# TODO: add other shaders, but also switch to for loop?


# Optimization is also delegated to SPIRV-Tools.
# Right now there are no difference between optimization levels greater than zero;
# they will all invoke the same optimization recipe. That is, the recipe behind spirv-opt -O.
# -Os is a special set of options.  Can run custom spirv optimizations via
# -Oconfig=--loop-unroll,--scalar-replacement=300,--eliminate-dead-code-aggressive

# this outputs spv for android, then transpiles it to ios
pushd out/android

echo gen SPIRV 1.3 with dxc

${appDxc} ${vsargs} -spirv -fspv-target-env=vulkan1.3 -I ${includeDir} -E SkinningVS -Fo Skinning.vert.spv -Fc Skinning.vert.spv.txt Skinning.hlsl
${appDxc} ${psargs} -spirv -fspv-target-env=vulkan1.3 -I ${includeDir} -E SkinningPS -Fo Skinning.frag.spv -Fc Skinning.frag.spv.txt Skinning.hlsl

${appDxc} ${vsargs} -spirv -fspv-target-env=vulkan1.3 -I ${includeDir} -E SampleVS -Fo Sample.vert.spv -Fc Sample.vert.spv.txt Sample.hlsl
${appDxc} ${psargs} -spirv -fspv-target-env=vulkan1.3 -I ${includeDir} -E SamplePS -Fo Sample.frag.spv -Fc Sample.frag.spv.txt Sample.hlsl

${appDxc} ${csargs} -spirv -fspv-target-env=vulkan1.3 -I ${includeDir} -E ComputeCS -Fo Compute.comp.spv -Fc Compute.comp.spv.txt Compute.hlsl

# -Fre not supported with spirv, so just use spirv-reflect
# either yaml or random format, why can't this just output json?
${appSpirvReflect} -y Skinning.vert.spv > Skinning.vert.refl
${appSpirvReflect} -y Skinning.frag.spv > Skinning.frag.refl
${appSpirvReflect} -y Sample.vert.spv > Sample.vert.refl
${appSpirvReflect} -y Sample.frag.spv > Sample.frag.refl
${appSpirvReflect} -y Compute.comp.spv > Compute.comp.refl

popd > /dev/null

# This needs spv from android for now to transpile
if [[ $testMetal -eq 1 ]]; then

    pushd out/ios

    #metaliOSOptions="-frecord-sources -g "
    #metaliOSOptions+="-std=ios-metal2.4 "

    # transpile android spirv to ios MSL for comparsion to what hlslparser MSL produces
    #  would never use this, would use hlslparser path directly or gen spirv
    #  specific for this target
    spvDir=${dstDir}/android/
    
    ${appSpirvCross} --msl --msl-version 30000 --msl-ios ${spvDir}Skinning.vert.spv --output Skinning.vert.metal
    ${appSpirvCross} --msl --msl-version 30000 --msl-ios ${spvDir}Skinning.frag.spv --output Skinning.frag.metal
    ${appSpirvCross} --msl --msl-version 30000 --msl-ios ${spvDir}Sample.vert.spv --output Sample.vert.metal
    ${appSpirvCross} --msl --msl-version 30000 --msl-ios ${spvDir}Sample.frag.spv --output Sample.frag.metal
    ${appSpirvCross} --msl --msl-version 30000 --msl-ios ${spvDir}Compute.comp.spv --output Compute.comp.metal

    # do includes survive transpile, why does this need -I ?s
    # compile to make sure code is valid
    ${appMetaliOS} ${metaliOSOptions} -o GameShadersTranspile.metallib -I ${includeDir} Skinning.vert.metal Skinning.frag.metal Sample.vert.metal Sample.frag.metal Compute.comp.metal
    
    popd > /dev/null
fi


# DONE: need to group files into library/module
# also create a readable spv file, so can look through that

# TODO: create reflect data w/o needing spirv

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
  
# TODO: transpile with spirv-cross to WGSL, GLSL, etc off the spirv.

# -enable-16bit-types     Enable 16bit types and disable min precision types. Available in HLSL 2018 and shader model 6.2
# -Fc <file>              Output assembly code listing file

# this prints cwd if not redirected
