#!/bin/zsh

set -x

# create directories to install results
mkdir -p bin
mkdir -p build

pushd build
cmake --install . --config Release
popd
