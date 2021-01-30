#!/bin/bash

# note: zsh works on TrivisCI osx, but not on Win git bash, so using bassh

# osx, windows, linux
buildType=$1

# translate Github Actions to os typpe
if [[ $buildType =~ "macos" ]]; then
	buildType="osx"
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
if [[ $buildType == osx ]]; then
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

# now that bin folder has kramv.app, zip move it so it can be copied to releases
# this should really use cpack in the make file to build a dmg, but not there yet
if [[ $buildType == osx ]]; then
	zip -rm bin/kramv.app
fi
