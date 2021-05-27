# for now don't require high cmake for TravisCI builds on Win, but need 3.19.2 for universal app builds
if (APPLE)
    cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)
else()
    cmake_minimum_required(VERSION 3.18.0 FATAL_ERROR)
endif()

#-----------------------------------------------------

# https://cmake.org/cmake/help/latest/policy/CMP0077.html#policy:CMP0077
#cmake_policy(SET CMP0077 NEW)

set(BUILD_UNIX FALSE)
set(BUILD_IOS FALSE)
if (APPLE)
    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        message("build for iOS")
        set(BUILD_IOS TRUE)
    else()
        message("build for macOS")
    endif()
elseif (WIN32)
    message("build for win x64")
elseif (UNIX AND NOT APPLE)
    message("build for unix")
    set(UNIXBUILD TRUE)
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

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Xcode 12.2 ships with macosx11.0, but 12.3 ships with macosx11.1
# cmake on 12.3 completely breaks when this is set and can't find c compilers.
# set(CMAKE_OSX_SYSROOT macosx11.0)
# set(CMAKE_OSX_SYSROOT macos)  # this doesn't work

# CMAKE_OSX_DEPLOYMENT_TARGET must be set as a CACHE variable, or it will be stripped
if (APPLE)
    if (BUILD_IOS)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum iOS")
        set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)" CACHE STRING "Architecture iOS")
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS")
        set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)" CACHE STRING "Architecture macOS")
    endif()
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_DEFAULT_STARTUP_PROJECT "kram")

#-----------------------------------------------------

# cmake translates project to sln in Win, but to xcode projects on Mac.
# No way to make xcode workspaces, but could do manually.
set(myTargetWorkspace kramWorkspace)

if (APPLE)
    project(${myTargetWorkspace} LANGUAGES C CXX OBJCXX)
else()
    project(${myTargetWorkspace} LANGUAGES C CXX)
endif()

# the kram static library libkram which should build on iOS/Android/Mac/Win
# this doesn't set a project, but maybe it should
add_subdirectory(libkram)

# the CLI app for Mac/Win that can build content for other platforms, uses libkram
add_subdirectory(kram)

# the viewer is only written for macOS Intel/ARM currently, uses libkram
if (APPLE)
    add_subdirectory(kramv)
endif()

# ps plugin that uses libkram
if (APPLE)
    add_subdirectory(plugin)
endif()

#-----------------------------------------------------

# https://discourse.cmake.org/t/specifying-cmake-osx-sysroot-breaks-xcode-projects-but-no-other-choice/2532/8
# use snipet from Alian Martin to validate SDK

if (APPLE)
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
    set(XCODE_MIN_SDK_IOS 14.0)
    set(XCODE_MIN_SDK_MACOS 11.0)

    execute_process(
        COMMAND xcrun --sdk "${CMAKE_OSX_SYSROOT}" --show-sdk-version
        OUTPUT_VARIABLE SDK_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (BUILD_IOS)
        message("iOS SDK ${SDK_VERSION}")
        message("iOS deploy ${CMAKE_OSX_DEPLOYMENT_TARGET}")
        message("iOS arch ${CMAKE_OSX_ARCHITECTURES}")
                
        if (SDK_VERSION VERSION_LESS XCODE_MIN_SDK_IOS)
            message(FATAL_ERROR "This project requires at least iPhoneOS ${XCODE_MIN_SDK_IOS}"
        )
        endif()
    else()
        message("macOS SDK ${SDK_VERSION}")
        message("macOS deploy ${CMAKE_OSX_DEPLOYMENT_TARGET}")
        message("macOS arch ${CMAKE_OSX_ARCHITECTURES}")
                        
        if (SDK_VERSION VERSION_LESS XCODE_MIN_SDK_MACOS)
            message(FATAL_ERROR "This project requires at least macOS SDK ${XCODE_MIN_SDK_MACOS}"
        )
        endif()
    endif()
endif()

#-----------------------------------------------------

set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

# So by default install depends on ALL_BUILD target, but that will fail if plugin
# does not have everything setup to build (or like now is not building).
# The plugin is currently setting EXCLUDE_FROM_ALL on the target so it's not built.
# https://stackoverflow.com/questions/17164731/installing-only-one-target-and-its-dependencies-out-of-a-complex-project-with

# install doesn't seem to do anything on WIN32, the build elements are not copied
install(TARGETS libkram ARCHIVE DESTINATION ${BIN_DIR})
install(TARGETS kram RUNTIME DESTINATION ${BIN_DIR})
if (APPLE)
	install(TARGETS kramv BUNDLE DESTINATION ${BIN_DIR})
endif()
# don't install this
#if (APPLE)
#    install(TARGETS kram-ps BUNDLE DESTINATION ${BIN_DIR})
#endif()



