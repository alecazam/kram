#!/bin/zsh

xcrun -sdk macosx metal -c ../kramv/KramShaders.metal -o ../bin/KramShaders.air
xcrun -sdk macosx metallib ../bin/KramShaders.air -o ../bin/KramShaders.metallib