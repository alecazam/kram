#!/bin/bash

# note: zsh works on  osx, but not on Win git bash, so using bash

#-----------------------------------------------

# write out the git tag as a version.h file in a 
tag=$(git describe --always --tags)
versionFile=./libkram/kram/KramVersion.h

echo "#pragma once" > $versionFile
echo "#define KRAM_VERSION \"$tag\"" >> $versionFile

#-----------------------------------------------

# osx, windows, linux
buildType=$1

# translate Github Actions to os typpe
if [[ $buildType =~ "macos" ]]; then
	buildType="macos"
elif [[ $buildType =~ "windows" ]]; then
	buildType="windows"
elif [[ $buildType =~ "ubuntu" ]]; then
	buildType="linux"
else
	buildType="macos"
fi

# exit on failure of any command
set -e

# don't use this or secrets may appear in build log
#set -x

# create directories to install results
mkdir -p bin

# need absolute path
# but macos left out realpath, ugh
# https://stackoverflow.com/questions/3572030/bash-script-absolute-path-with-os-x
if [[ $buildType == macos ]]; then
	binPath=$( cd "$(dirname "bin")" ; pwd -P )

	# not sure why above doesn't include the folder name
	binHolderPath="${binPath}"
	binPath="${binPath}/bin"
else
	binPath=$(realpath bin)
fi

# can't just use cmake .. on osx, cmake gets confused about metal files since language not recognized
# but Xcode knows how to build these.  I don't want to setup special rule for metal files right now.
if [[ $buildType == macos ]]; then

	# not using CMake anymore on mac/iOS.  Using custom workspace and projects.
	#cmake .. -G Xcode
	# build the release build
	#cmake --build . --config Release
	# copy it to bin directory
	#cmake --install . --config Release

	# this dir already exists, so don't have to mkdir
	pushd build2

	# build libraries
	# see here about destination arg
	# https://github.com/appcelerator/titanium_mobile/pull/13098
	xcodebuild build -sdk iphoneos -workspace kram.xcworkspace -scheme kram-ios -configuration Release -destination generic/platform=iOS CONFIGURATION_BUILD_DIR=${binPath} BUILD_LIBRARY_FOR_DISTRIBUTION=YES
	xcodebuild build -sdk macosx -workspace kram.xcworkspace -scheme kram -configuration Release -destination generic/platform=macOS CONFIGURATION_BUILD_DIR=${binPath} BUILD_LIBRARY_FOR_DISTRIBUTION=YES
	
	# install apps so they are signed
	# can't specify empty INSTALL_PATH, or xcodebuild succeeds but copies nothing to bin
	xcodebuild install -sdk macosx -workspace kram.xcworkspace -scheme kramc -configuration Release -destination generic/platform=macOS DSTROOT=${binHolderPath} INSTALL_PATH=bin
	xcodebuild install -sdk macosx -workspace kram.xcworkspace -scheme kramv -configuration Release -destination generic/platform=macOS DSTROOT=${binHolderPath} INSTALL_PATH=bin

	popd

elif [[ $buildType == windows ]]; then
	mkdir -p build

	pushd build

	#cmake .. -G "Visual Studio 15 2017 Win64"
	cmake .. -G "Visual Studio 16 2019" -A x64

	# build the release build
	cmake --build . --config Release

	# copy it to bin directory
	cmake --install . --config Release

	popd

elif [[ $buildType == linux ]]; then
	mkdir -p build

	pushd build

	cmake .. 

	# build the release build
	cmake --build . --config Release

	# copy it to bin directory
	cmake --install . --config Release

	popd
fi


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
