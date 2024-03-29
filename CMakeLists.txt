# for now don't require high cmake for TravisCI builds on Win, but need 3.19.2 for universal app builds
if (APPLE)
    cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)
else()
    cmake_minimum_required(VERSION 3.18.0 FATAL_ERROR)
endif()

#-----------------------------------------------------

set(BUILD_MAC FALSE)
set(BUILD_WIN FALSE)

if (APPLE)
    message("build for macOS")
    set(BUILD_MAC TRUE)
elseif (WIN32)
    message("build for win x64")
    set(BUILD_WIN TRUE)
endif()

#-----------------------------------------------------

# SYSROOT must be set before project,
# SYSROOT is max OS, deployment is min OS on Apple.
# If SYSROOT not set, then cmake uses min OS from deployment target.  Ugh.
# so have to force SYSROOT to latest SDK.
# Want to set 11.0 here, but Xcode 12.3 ships with 11.1, etc.
# So then cmake breaks when it cannot find the C compiler, etc.
# Setting macosx to take the latest greatest sdk.

# don't change these to set_property(GLOBAL) or set_target_properties, the need to be set prior to project
# and only seem to work if set() is used to force the global value.

# suppress ZERO_CHECK project
set(CMAKE_SUPPRESS_REGENERATION true)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Xcode 12.2 ships with macosx11.0, but 12.3 ships with macosx11.1
# cmake on 12.3 completely breaks when this is set and can't find c compilers.
# set(CMAKE_OSX_SYSROOT macosx11.0)
# set(CMAKE_OSX_SYSROOT macos)  # this doesn't work

# CMAKE_OSX_DEPLOYMENT_TARGET must be set as a CACHE variable, or it will be stripped
if (BUILD_MAC)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
    set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)" CACHE STRING "Architecture macOS")
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_BUILD_TYPE Release)

if (BUILD_MAC)
    set(CMAKE_DEFAULT_STARTUP_PROJECT "kramc")
elseif (BUILD_WIN)
    set(CMAKE_DEFAULT_STARTUP_PROJECT "kramc")
endif()


#-----------------------------------------------------

# cmake translates project to sln in Win, but to xcode projects on Mac.
# No way to make xcode workspaces, but could do manually.
set(myTargetWorkspace kramWorkspace)

if (BUILD_MAC)
    project(${myTargetWorkspace} LANGUAGES C CXX OBJCXX)
elseif (BUILD_WIN)
    project(${myTargetWorkspace} LANGUAGES C CXX)
endif()

# the kram static library libkram which should build on iOS/Android/Mac/Win
# this doesn't set a project, but maybe it should
add_subdirectory(libkram)

# the CLI app for Mac/Win that can build content for other platforms, uses libkram
add_subdirectory(kramc)

if (BUILD_MAC)
    # the viewer is only written for macOS Intel/ARM currently, uses libkram
    add_subdirectory(kramv)

    # ps plugin that uses libkram
    add_subdirectory(plugin)
endif()

# this is an Explorer thumbnail extension (run script to un/register), uses libkrma
if (BUILD_WIN)
    add_subdirectory(kram-thumb-win)
endif()

# hack hlslparser for win build into kram for now, does not use kram
if (BUILD_WIN)
    add_subdirectory(hlslparser)
endif()

#-----------------------------------------------------

# https://discourse.cmake.org/t/specifying-cmake-osx-sysroot-breaks-xcode-projects-but-no-other-choice/2532/8
# use snipet from Alian Martin to validate SDK

if (BUILD_MAC)
    if(NOT DEFINED CMAKE_OSX_SYSROOT)
        message(FATAL_ERROR "Cannot check SDK version if CMAKE_OSX_SYSROOT is not defined."
    )
    endif()

    # check the Xcode app itself for it's version
    set(XCODE_MIN_APP 12.2)
    if(XCODE AND XCODE_VERSION VERSION_LESS XCODE_MIN_APP)
        message(FATAL_ERROR "This project requires at least Xcode ${XCODE_MIN_APP}")
    endif()

    # check the SDK
    set(XCODE_MIN_SDK_IOS 14.1)
    set(XCODE_MIN_SDK_MACOS 11.0)

    execute_process(
        COMMAND xcrun --sdk "${CMAKE_OSX_SYSROOT}" --show-sdk-version
        OUTPUT_VARIABLE SDK_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    message("macOS SDK ${SDK_VERSION}")
    message("macOS deploy ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    message("macOS arch ${CMAKE_OSX_ARCHITECTURES}")
                    
    if (SDK_VERSION VERSION_LESS XCODE_MIN_SDK_MACOS)
        message(FATAL_ERROR "This project requires at least macOS SDK ${XCODE_MIN_SDK_MACOS}"
    )
    endif()
    
endif()

#-----------------------------------------------------

# was considering platform-specific builds, but mac/win don't conflict
set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)
    
# So by default install depends on ALL_BUILD target, but that will fail if plugin
# does not have everything setup to build (or like now is not building).
# The plugin is currently setting EXCLUDE_FROM_ALL on the target so it's not built.
# https://stackoverflow.com/questions/17164731/installing-only-one-target-and-its-dependencies-out-of-a-complex-project-with

# install doesn't seem to do anything on WIN32, the build elements are not copied
install(TARGETS libkram ARCHIVE DESTINATION ${BIN_DIR})

if (BUILD_MAC OR BUILD_WIN)
    install(TARGETS kram RUNTIME DESTINATION ${BIN_DIR})
endif()

if (BUILD_MAC)
	install(TARGETS kramv BUNDLE DESTINATION ${BIN_DIR})
endif()

if (BUILD_WIN)
    install(TARGETS kram-thumb-win LIBRARY DESTINATION ${BIN_DIR})
endif()

# don't install this yet
#if (BUILD_MAC)
#    install(TARGETS kram-ps BUNDLE DESTINATION ${BIN_DIR})
#endif()

# hlslparser is also now in the kram build.  Keep executables up to date.
# I would use the sln file, but msbuild doesn't like to be called from cibuld.sh
if (BUILD_WIN)
    install(TARGETS hlslparser RUNTIME DESTINATION ${BIN_DIR})
endif()



