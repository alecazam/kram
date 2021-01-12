cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)

#-----------------------------------------------------

# https://cmake.org/cmake/help/latest/policy/CMP0077.html#policy:CMP0077
#cmake_policy(SET CMP0077 NEW)

if (APPLE)
    message("build for macOS")
elseif (WIN32)
    message("build for winOS")
endif()

#-----------------------------------------------------

# suppress ZERO_CHECK project
set(CMAKE_SUPPRESS_REGENERATION true)

# This must be set before project,
# sys root is max OS, deployment is min OS
# If SYSROOT not set, then cmake uses min OS from deployment target.  Ugh.
# so have to force SYSROOT to latest SDK.
set(CMAKE_OSX_SYSROOT "macosx11.0")
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# for universal builds on macOS for Intel and Apple Silicon
# https://gitlab.kitware.com/cmake/cmake/-/merge_requests/5589
set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)")

# these are carried across the builds
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_BUILD_TYPE Release)

#-----------------------------------------------------

set(myTargetWorkspace kramWorkspace)

project(${myTargetWorkspace} LANGUAGES C CXX)

# the kram static library libkram which should build on iOS/Android/Mac/Win
add_subdirectory(source)

# the CLI app for Mac/Win that can build content for other platforms, uses libkram
add_subdirectory(source/app)

# the viewer is only written for macOS Intel/ARM currently, uses libkram
if (APPLE)
    add_subdirectory(source/viewer)
endif()

#-----------------------------------------------------

# ALL_BUILD can't execute an app, so change the startup project to something runnable
# either this or kramv
set(CMAKE_DEFAULT_STARTUP_PROJECT "kram")





