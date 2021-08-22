#!/bin/bash

# note: zsh works on  osx, but not on Win git bash, so using bash

#-----------------------------------------------

# write out the git tag as a version.h file in a 
tag=$(git describe --always --tags)
versionFile=../libkram/kram/KramVersion.h

echo "#pragma once" > $versionFile
echo "#define KRAM_VERSION \"$tag\"" >> $versionFile

#-----------------------------------------------

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

# need absolute path
binPath=$(realpath bin)

mkdir -p build

pushd build

# can't just use cmake .. on osx, cmake gets confused about metal files since language not recognized
# but Xcode knows how to build these.  I don't want to setup special rule for metal files right now.
if [[ $buildType == macos ]]; then

	# not using CMake anymore on mac/iOS.  Using custom workspace and projects.
	#cmake .. -G Xcode

	# build the release build
	#cmake --build . --config Release

	# copy it to bin directory
	#cmake --install . --config Release

	pushd ../build2

	xcodebuild build -sdk iphoneos -workspace kram.xcworkspace -scheme kram-ios CONFIGURATION_BUILD_DIR=${binPath} BUILD_LIBRARY_FOR_DISTRIBUTION=YES
	xcodebuild build -sdk macosx -workspace kram.xcworkspace -scheme kram CONFIGURATION_BUILD_DIR=${binPath} BUILD_LIBRARY_FOR_DISTRIBUTION=YES

	xcodebuild install -sdk macosx -workspace kram.xcworkspace -scheme kramc DSTROOT=${binPath} INSTALL_PATH=.
	xcodebuild install -sdk macosx -workspace kram.xcworkspace -scheme kramv DSTROOT=${binPath} INSTALL_PATH=.

	popd

elif [[ $buildType == windows ]]; then
	#cmake .. -G "Visual Studio 15 2017 Win64"
	cmake .. -G "Visual Studio 16 2019" -A x64

	# build the release build
	cmake --build . --config Release

	# copy it to bin directory
	cmake --install . --config Release

elif [[ $buildType == linux ]]; then
	cmake .. 

	# build the release build
	cmake --build . --config Release

	# copy it to bin directory
	cmake --install . --config Release
fi

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
