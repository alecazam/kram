cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)
  
#-----------------------------------------------------

# really not using cmake for mac, but this was used in the past so leaving it in
# still building Win using Cmake.  macOS uses avx2 and Win uses avx2.
set(BUILD_MAC FALSE)
set(BUILD_WIN FALSE)

if (APPLE)
    message(STATUS "build for macOS")
    set(BUILD_MAC TRUE)
elseif (WIN32)
    message(STATUS "build for win x64")
    set(BUILD_WIN TRUE)
endif()

#-----------------------------------------------------

# suppress ZERO_CHECK project
set(CMAKE_SUPPRESS_REGENERATION true)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_DEFAULT_STARTUP_PROJECT "kramc")
    
#-----------------------------------------------------
if (BUILD_WIN)
    # cmake translates project to sln in Win, but to xcode projects on Mac.
    # No way to make xcode workspaces, but could do manually.
    set(myTargetWorkspace kramWorkspace)

    project(${myTargetWorkspace} LANGUAGES C CXX)
    
    # use clang-cl on Win, have to set both
    # this is suggested way to set, but github already reports clang 17.0.3 with VS extensions
    #  as the default compiler
    # set(CMAKE_C_COMPILER "clang-cl")
    # set(CMAKE_CXX_COMPILER "clang-cl")

    # want to only use clang across all platforms
    message(STATUS "Using ${CMAKE_CXX_COMPILER_ID} compiler")
    
    #-----------------------------------------------------

    # the kram static library libkram which should build on iOS/Android/Mac/Win
    # this doesn't set a project, but maybe it should
    add_subdirectory(libkram)

    # the CLI app for Mac/Win that can build content for other platforms, uses libkram
    add_subdirectory(kramc)

    # TODO: this needs a shared libkram, but kramc uses static libkram
    # this is an Explorer thumbnail extension (run script to un/register), uses libkram
    # add_subdirectory(kram-thumb-win)
    
    # hack hlslparser for win build into kram for now, does not use kram
    # add_subdirectory(hlslparser)

    #-----------------------------------------------------

    # was considering platform-specific builds, but mac/win don't conflict
    set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

    # install doesn't seem to do anything on WIN32, the build elements are not copied
    #install(TARGETS libkram ARCHIVE DESTINATION ${BIN_DIR})
    #install(TARGETS kram RUNTIME DESTINATION ${BIN_DIR})
    #install(TARGETS kram-thumb-win LIBRARY DESTINATION ${BIN_DIR})
    
    # hlslparser is also now in the kram build.  Keep executables up to date.
    # I would use the sln file, but msbuild doesn't like to be called from cibuld.sh
    # This builds but has a lot of warnings.  When I resume work, will re-instate.
    # install(TARGETS hlslparser RUNTIME DESTINATION ${BIN_DIR})
endif()

#-----------------------------------------------------
# This part is unmaintained.  Couldn't build app extensions via CMake.
# So now just mapintain projects

if (BUILD_MAC)
    # cmake translates project to sln in Win, but to xcode projects on Mac.
    # No way to make xcode workspaces, but could do manually.
    set(myTargetWorkspace kramWorkspace)

    project(${myTargetWorkspace} LANGUAGES C CXX OBJCXX)

    # CMAKE_OSX_DEPLOYMENT_TARGET must be set as a CACHE variable, or it will be stripped
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS")
    set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)" CACHE STRING "Architecture macOS")
   
    #-----------------------------------------------------

    # the kram static library libkram which should build on iOS/Android/Mac/Win
    # this doesn't set a project, but maybe it should
    add_subdirectory(libkram)

    # the CLI app for Mac/Win that can build content for other platforms, uses libkram
    add_subdirectory(kramc)

    # the viewer is only written for macOS Intel/ARM currently, uses libkram
    add_subdirectory(kramv)

    # ps plugin that uses libkram
    add_subdirectory(plugin)
  
    # hlslparser needs some more work to modernize to a C++ style HLSL syntax
    add_subdirectory(hlslparser)

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




