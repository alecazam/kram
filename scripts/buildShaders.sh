#!/bin/zsh

xcrun -sdk macosx metal -c ../kramv/KramShaders.metal -o ../bin/KramShaders.air
xcrun -sdk macosx metallib ../bin/KramShaders.air -o ../bin/KramShaders.metallib

# don't need this after metallib built
rm ../bin/KramShaders.air