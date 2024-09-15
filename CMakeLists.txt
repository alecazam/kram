cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)

#-----------------------------------------------------

# really not using cmake for mac, but this was used in the past so leaving it in
set(BUILD_MAC FALSE)
set(BUILD_WIN FALSE)

#-----------------------------------------------------

# SYSROOT must be set before project,
# SYSROOT is max OS, deployment is min OS on Apple.
# If SYSROOT not set, then cmake uses min OS from deployment target.  Ugh.
# so have to force SYSROOT to latest SDK.

# don't change these to set_property(GLOBAL) or set_target_properties, the need to be set prior to project
# and only seem to work if set() is used to force the global value.

# suppress ZERO_CHECK project
set(CMAKE_SUPPRESS_REGENERATION true)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_DEFAULT_STARTUP_PROJECT "kramc")

#-----------------------------------------------------
# still building Win using Cmake

if (BUILD_WIN)
    message("build for win x64")
    set(BUILD_WIN TRUE)

    # use clang-cl on Win
    set(CMAKE_CXX_COMPILER "clang-cl")

    # cmake translates project to sln in Win, but to xcode projects on Mac.
    # No way to make xcode workspaces, but could do manually.
    set(myTargetWorkspace kramWorkspace)

    project(${myTargetWorkspace} LANGUAGES C CXX)
    
    # the kram static library libkram which should build on iOS/Android/Mac/Win
    # this doesn't set a project, but maybe it should
    add_subdirectory(libkram)

    # the CLI app for Mac/Win that can build content for other platforms, uses libkram
    add_subdirectory(kramc)

    # this is an Explorer thumbnail extension (run script to un/register), uses libkram
    add_subdirectory(kram-thumb-win)
    
    # hack hlslparser for win build into kram for now, does not use kram
    add_subdirectory(hlslparser)

    #-----------------------------------------------------

    # was considering platform-specific builds, but mac/win don't conflict
    set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

    # install doesn't seem to do anything on WIN32, the build elements are not copied
    install(TARGETS libkram ARCHIVE DESTINATION ${BIN_DIR})
    
    install(TARGETS kram RUNTIME DESTINATION ${BIN_DIR})
    install(TARGETS kram-thumb-win LIBRARY DESTINATION ${BIN_DIR})
    
    # hlslparser is also now in the kram build.  Keep executables up to date.
    # I would use the sln file, but msbuild doesn't like to be called from cibuld.sh
    install(TARGETS hlslparser RUNTIME DESTINATION ${BIN_DIR})
endif()

#-----------------------------------------------------
# This part is unmaintained.  Couldn't build app extensions via CMake.
# So now just mapintain projects

if (BUILD_MAC)
    message("build for macOS universal (unmaintained)")
    set(BUILD_MAC TRUE)
    
    # CMAKE_OSX_DEPLOYMENT_TARGET must be set as a CACHE variable, or it will be stripped
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS")
    set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)" CACHE STRING "Architecture macOS")
    
    # cmake translates project to sln in Win, but to xcode projects on Mac.
    # No way to make xcode workspaces, but could do manually.
    set(myTargetWorkspace kramWorkspace)

    project(${myTargetWorkspace} LANGUAGES C CXX OBJCXX)

    # the kram static library libkram which should build on iOS/Android/Mac/Win
    # this doesn't set a project, but maybe it should
    add_subdirectory(libkram)

    # the CLI app for Mac/Win that can build content for other platforms, uses libkram
    add_subdirectory(kramc)

    # the viewer is only written for macOS Intel/ARM currently, uses libkram
    add_subdirectory(kramv)

    # ps plugin that uses libkram
    add_subdirectory(plugin)

    #-----------------------------------------------------

    # https://discourse.cmake.org/t/specifying-cmake-osx-sysroot-breaks-xcode-projects-but-no-other-choice/2532/8
    # use snipet from Alian Martin to validate SDK

    if(NOT DEFINED CMAKE_OSX_SYSROOT)
        message(FATAL_ERROR "Cannot check SDK version if CMAKE_OSX_SYSROOT is not defined."
    )
    endif()

    # check the Xcode app itself for it's version
    set(XCODE_MIN_SDK_MACOS 13.0)
    set(XCODE_MIN_APP 13.0)
    
    if(XCODE AND XCODE_VERSION VERSION_LESS XCODE_MIN_APP)
        message(FATAL_ERROR "This project requires at least Xcode ${XCODE_MIN_APP}")
    endif()

    # check the SDK
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
  
    #-----------------------------------------------------

    # was considering platform-specific builds, but mac/win don't conflict
    set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

    install(TARGETS libkram ARCHIVE DESTINATION ${BIN_DIR})
    install(TARGETS kram RUNTIME DESTINATION ${BIN_DIR})
	install(TARGETS kramv BUNDLE DESTINATION ${BIN_DIR})
    install(TARGETS hlslparser RUNTIME DESTINATION ${BIN_DIR})

    # photoshop plugin
#    install(TARGETS kram-ps BUNDLE DESTINATION ${BIN_DIR})

endif()




