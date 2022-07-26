#!/bin/zsh

# run from kram directory
pushd kramv/Shaders
xcrun -sdk macosx metal KramShaders.metal skybox.metal pbr.metal hdr.metal brdf.metal -o ../../bin/KramShaders.metallib
popd
