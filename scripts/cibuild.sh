#!/bin/zsh

set -x

# create directories to install results
mkdir -p bin
mkdir -p build

pushd build
cmake ..
cmake --build . --config Release
cmake --install . --config Release
popd
