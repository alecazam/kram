#!/bin/bash

# note: zsh works on TrivisCI osx, but not on Win git bash, so using bassh

# osx, windows, linux
buildType=$1

# translate Github Actions to os typpe
if [[ $buildType =~ "macos" ]]; then
	buildType="macos"
fi
if [[ $buildType =~ "windows" ]]; then
	buildType="windows"
fi
if [[ $buildType =~ "ubuntu" ]]; then
	buildType="linux"
fi

# exit on failure of any command
set -e

# don't use this or secrets may appear in build log
#set -x

# create directories to install results
mkdir -p bin
mkdir -p build

pushd build

# can't just use cmake .. on osx, cmake gets confused about metal files since language not recognized
# but Xcode knows how to build these.  I don't to setup special rule for metal files right now.
if [[ $buildType == macos ]]; then
	cmake .. -G Xcode
elif [[ $buildType == windows ]]; then
	#cmake .. -G "Visual Studio 15 2017 Win64"
	cmake .. -G "Visual Studio 16 2019" -A x64
elif [[ $buildType == linux ]]; then
	cmake .. 
fi

# build the release build
cmake --build . --config Release

# copy it to bin directory
cmake --install . --config Release

popd

#---------------------------------

pushd bin

# put these in with the zip
cp ../LICENSE .
cp ../README.md .

# concatenate to one platform-specific zip archive to make it easy to install
# lovely... zip isn't a part of Git-Bash, so use 7z in MSYS2
if [[ $buildType == windows ]]; then
	7z a -r "kram-${buildType}.zip" .
else 
	zip -r "kram-${buildType}.zip" .
fi

popd
