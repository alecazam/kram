# for now don't require high cmake for TravisCI builds on Win, but need 3.19.2 for universal app builds
if (APPLE)
    cmake_minimum_required(VERSION 3.19.2 FATAL_ERROR)
elseif (WIN32)
    cmake_minimum_required(VERSION 3.18.0 FATAL_ERROR)
endif()

#-----------------------------------------------------

# https://cmake.org/cmake/help/latest/policy/CMP0077.html#policy:CMP0077
#cmake_policy(SET CMP0077 NEW)

if (APPLE)
    message("build for macOS")
elseif (WIN32)
    message("build for winOS")
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

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)
set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)")

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_DEFAULT_STARTUP_PROJECT "kram")

#-----------------------------------------------------

set(myTargetWorkspace kramWorkspace)

if (APPLE)
project(${myTargetWorkspace} LANGUAGES C CXX OBJCXX)
elseif (WIN32)
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

#-----------------------------------------------------

set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

install(TARGETS libkram ARCHIVE DESTINATION ${BIN_DIR})
install(TARGETS kram RUNTIME DESTINATION ${BIN_DIR})
if (APPLE)
	install(TARGETS kramv BUNDLE DESTINATION ${BIN_DIR})
endif()




