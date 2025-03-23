#!/bin/bash

# note: zsh works on  osx, but not on Win git bash, so using bash

# here is a post about grouping output using echo
# these cannot be nested
# https://github.com/orgs/community/discussions/25314
    
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

    xargs=-showBuildTimingSummary
	# build libraries
	# see here about destination arg
	# https://github.com/appcelerator/titanium_mobile/pull/13098
 
    echo "::group::list-targets"
    xcodebuild build -workspace kram.xcworkspace -list
    echo "::endgroup::"
 
	# note there is a method to make an xcframework, but seems that it has to be signed
	# instead the vos/ios libs will have unique output dirs, but don't have to when used in a workspace

    # vectormath
    echo "::group::vectormath-vos"
    xcodebuild build -sdk xros -workspace kram.xcworkspace -scheme vectormath -configuration Release ${xargs} -destination generic/platform=visionOS CONFIGURATION_BUILD_DIR=${binPath}/vos BUILD_LIBRARY_FOR_DISTRIBUTION=YES
    echo "::endgroup::"
    
    echo "::group::vectormath-ios"
    xcodebuild build -sdk iphoneos -workspace kram.xcworkspace -scheme vectormath -configuration Release ${xargs} -destination generic/platform=iOS CONFIGURATION_BUILD_DIR=${binPath}/ios BUILD_LIBRARY_FOR_DISTRIBUTION=YES
    echo "::endgroup::"
 
    echo "::group::vectormath"
    xcodebuild build -sdk macosx -workspace kram.xcworkspace -scheme vectormath -configuration Release ${xargs} -destination generic/platform=macOS CONFIGURATION_BUILD_DIR=${binPath}/mac BUILD_LIBRARY_FOR_DISTRIBUTION=YES
	echo "::endgroup::"
 
    # libkram
    echo "::group::kram-vos"
    xcodebuild build -sdk xros -workspace kram.xcworkspace -scheme kram -configuration Release ${xargs} -destination generic/platform=visionOS CONFIGURATION_BUILD_DIR=${binPath}/vos BUILD_LIBRARY_FOR_DISTRIBUTION=YES
    echo "::endgroup::"
    
    echo "::group::kram-ios"
    xcodebuild build -sdk iphoneos -workspace kram.xcworkspace -scheme kram -configuration Release ${xargs} -destination generic/platform=iOS CONFIGURATION_BUILD_DIR=${binPath}/ios BUILD_LIBRARY_FOR_DISTRIBUTION=YES
    echo "::endgroup::"
    
    echo "::group::kram"
    xcodebuild build -sdk macosx -workspace kram.xcworkspace -scheme kram -configuration Release ${xargs} -destination generic/platform=macOS CONFIGURATION_BUILD_DIR=${binPath}/mac BUILD_LIBRARY_FOR_DISTRIBUTION=YES
    echo "::endgroup::"
 
	# install apps so they are signed
	# can't specify empty INSTALL_PATH, or xcodebuild succeeds but copies nothing to bin
 
    # kramc cli
    echo "::group::kramc"
    xcodebuild install -sdk macosx -workspace kram.xcworkspace -scheme kramc -configuration Release ${xargs} -destination generic/platform=macOS DSTROOT=${binHolderPath} INSTALL_PATH=bin
    echo "::endgroup::"
      
    # kramv viewer
    echo "::group::kramv"
	xcodebuild install -sdk macosx -workspace kram.xcworkspace -scheme kramv -configuration Release ${xargs} -destination generic/platform=macOS DSTROOT=${binHolderPath} INSTALL_PATH=bin
    echo "::endgroup::"
    
	popd

	# hlslparser
	pushd hlslparser
    echo "::group::hlsl-parser"
    xcodebuild install -sdk macosx -project hlslparser.xcodeproj -scheme hlslparser -configuration Release ${xargs} -destination generic/platform=macOS DSTROOT=${binHolderPath} INSTALL_PATH=bin
    echo "::endgroup::"
	popd

    # kram-profile
    pushd kram-profile
    echo "::group::kram-profiler"
    xcodebuild install -sdk macosx -project kram-profile.xcodeproj -scheme kram-profile -configuration Release ${xargs} -destination generic/platform=macOS DSTROOT=${binHolderPath} INSTALL_PATH=bin
    echo "::endgroup::"
    popd

elif [[ $buildType == windows ]]; then
    # this builds kram.exe and thumbnailer
    echo "::group::kram-win"
    mkdir -p build

	pushd build

	# DONE: update to VS2022 and use clang
	cmake .. -G "Visual Studio 17 2022" -T ClangCL -A x64

	# build the release build
	cmake --build . --config Release

	# copy it to bin directory
    cmake --install . --config Release

    popd
    echo "::endgroup::"
    
	# mingW doesn't like running this Win style command line
	# see here for another method https://github.com/microsoft/setup-msbuild
	# just added the parser to cmake
	#
	# build hlslparser (release) to bin folder
	#pushd hlslparser
	#msbuild hlslparser.sln /p:OutputPath=${binHolderPath}
	#popd
	
elif [[ $buildType == linux ]]; then
    echo "::group::kram-linux"

    mkdir -p build

	pushd build

    # this will use make
    # cmake ..
   
    # this uses Ninja, so can see failures
    cmake .. -G Ninja

	# build the release build
	cmake --build . --config Release

	# copy it to bin directory
	cmake --install . --config Release

    popd
    echo "::endgroup::"

fi


#---------------------------------

echo "::group::archive"

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

echo "::endgroup::"
