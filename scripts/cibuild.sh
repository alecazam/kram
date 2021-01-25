#!/bin/zsh

set -x

# create directories to install results
mkdir -p bin
mkdir -p build

pushd build
cmake .. -G Xcode
cmake --install . --config Release
popd
