#!/bin/zsh

set -x

# create directories to install results
mkdir -p bin
mkdir -p build

pushd build
cmake .. -G Xcode
cmake --build . --config Release
cmake --install . --config Release
popd
