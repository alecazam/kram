#!/bin/zsh

buildType=$1

set -x

# create directories to install results
mkdir -p bin
mkdir -p build

pushd build

# can't just use cmake .., gets confused about metal files since language not recognized, but Xcode knows how to build these
if [[ $buildType == osx ]]; then
	cmake .. -G Xcode
elif [[ $buildType == windows ]]; then
	cmake .. -G "Visual Studio 15 2017" -A x64
elif [[ $buildType == linux ]]; then
	cmake .. 
fi

cmake --build . --config Release
cmake --install . --config Release
popd
