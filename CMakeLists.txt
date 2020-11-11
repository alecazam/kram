cmake_minimum_required(VERSION 3.11.0 FATAL_ERROR)

# https://cmake.org/cmake/help/latest/policy/CMP0077.html#policy:CMP0077
cmake_policy(SET CMP0077 NEW)

if (APPLE)
    set(macOS YES)
endif()

if (WIN32)
    set(winOS YES)
endif()

if(UNIX AND NOT APPLE)
    set(linuxOS YES)
endif()

# TODO: hook these up to added code below, and pass to as command line settings
option(ATE "Compile ATE Encoder" ON)
option(ETCENC "Compile ETC2Comp Encoder" ON)
option(SQUISH "Compile Squish Encoder" ON)
option(ASTCENC "Compile ASTCenc Encoder" ON)
option(BCENC "Compile BCenc Encoder" ON)

# convert ON to 1, UGH
set(COMPILE_ATE 0)
set(COMPILE_BCENC 0)
set(COMPILE_ETCENC 0)
set(COMPILE_SQUISH 0)
set(COMPILE_ASTCENC 0)

if (ATE)
    set(COMPILE_ATE 1)
endif()

if (BCENC)
    set(COMPILE_BCENC 1)
endif()

if (ETCENC)
    set(COMPILE_ETCENC 1)
endif()

if (SQUISH)
    set(COMPILE_SQUISH 1)
endif()

if (ASTCENC)
    set(COMPILE_ASTCENC 1)
endif()

# these don't set correct settings in Xcode, but CMake wants them set
# can compile as C11, C14, C17.  This opts for lowest setting.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# suppress ZERO_CHECK project
set(CMAKE_SUPPRESS_REGENERATION true)

if (macOS)
	message("build for macOS")

    # This must be set before project,
    # sys root is max OS, deployment is min OS
    # needs to be 11.0 for universal, but that's not a choice in Xcode 12
    set(CMAKE_OSX_SYSROOT "macosx10.15")
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)

endif()

if (winOS)
	message("build for winOS")
endif()


#-----------------------------------------------------
# kram 

# this can go before project?
set(APP_VERSION 0.9.0)

# now setup the app project, and link to libkram
set(myTargetApp kram)

project(${myTargetApp} LANGUAGES C CXX)

# **** this the executable target ****
add_executable(${myTargetApp})

# this works on vs and xcode
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_BUILD_TYPE Release)

target_include_directories(${myTargetApp} PRIVATE
    "${PROJECT_SOURCE_DIR}/source/kram/"
    )
target_sources(${myTargetApp} PRIVATE "${PROJECT_SOURCE_DIR}/source/app/KramMain.cpp")


if (macOS)
    set(PRODUCT_BUNDLE_IDENTIFIER "com.ba.kram")

    # ate is a macOS/iOS only library, and it varies in encode support by OS revision
    target_link_libraries(${myTargetApp} ate)
endif()
        
        
# No way to add a RunScript, but can set PRE_LINK, POST_BUILD commands on a target
# and then the makefile will execute those.

#-----------------------------------------------------
# libkram

# these are names, but app will be renamed to kram at the end
set(myTargetLib libkram)

# **** This will create libkram.a or .so or kram.lib depending on platform.
add_library(${myTargetLib} STATIC)

# rename app to kram, libkram will be other output
set_target_properties(${myTargetLib} PROPERTIES OUTPUT_NAME kram)

# force include this, so all output goes through common call without having to modify sources too much
# could be force include, but want precompiled headers anyways
#target_precompile_headers(${myTargetLib} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source/kram/KramConfig.h)
# turn off pch
set_target_properties(${myTargetLib} ${myTargetApp} PROPERTIES 
    DISABLE_PRECOMPILE_HEADERS ON
)

# When Win rebuilds library, it doesn't relink app to correct code when you
# build the app target project.  Breakpoints stop working after any library source edit,
# and you have to rebuild solution to get the correct code to exectute.  Since 2014.  Try this.
# And BUILD_ALL never launches properly.
# https://cmake.org/pipermail/cmake/2014-October/058798.html
if (winOS)
    SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
    SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
endif()

if (macOS)
    # force include instead of precompile
    target_compile_options(${myTargetLib} PRIVATE -include KramConfig.h)
    target_compile_options(${myTargetApp} PRIVATE -include KramConfig.h)

    # helper to set clang settings
    macro (set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE)
        set_property (TARGET ${TARGET} PROPERTY
            XCODE_ATTRIBUTE_${XCODE_PROPERTY} ${XCODE_VALUE})
    endmacro (set_xcode_property)

    # Note: match this up with CXX version
	set_xcode_property(${myTargetLib} ${myTargetApp} CLANG_CXX_LANGUAGE_STANDARD "c++11")
	set_xcode_property(${myTargetLib} ${myTargetApp} CLANG_CXX_LIBRARY "libc++")

    # for now disable signing, and just "sign to run locally"
    set_xcode_property(${myTargetApp} CODE_SIGNING_REQUIRED "NO")
    set_xcode_property(${myTargetApp} CODE_SIGN_IDENTITY "")
    
    # turn on avx1, avx2 is still more rare
    set_xcode_property(${myTargetLib} ${myTargetApp} CLANG_X86_VECTOR_INSTRUCTIONS "avx")
    
    # this drops app from 762KB to 174KB with only ATE enabled
    # note about needing -gfull instead of -gused here or debug info messed up:
    # https://gist.github.com/tkersey/39b4fe69e14b859889ffadccb009e397
    set_xcode_property(${myTargetApp} DEAD_CODE_STRIPPING YES)
    set_xcode_property(${myTargetApp} LLVM_LTO[variant=Release] "Incremental")

    # turn off exceptions/rtti
	set_xcode_property(${myTargetLib} ${myTargetApp} GCC_ENABLE_CPP_EXCEPTIONS NO)
	set_xcode_property(${myTargetLib} ${myTargetApp} GCC_ENABLE_CPP_RTTI NO)
    
    set_xcode_property(${myTargetApp} ONLY_ACTIVE_ARCH[variant=Debug] "YES")
    
    set_xcode_property (${myTargetLib} ${myTargetApp} DEBUG_INFORMATION_FORMAT[variant=Debug] "dwarf")
    set_xcode_property (${myTargetLib} ${myTargetApp} DEBUG_INFORMATION_FORMAT[variant=Release] "dwarf-with-dsym")
    
    #set_xcode_property (${myTargetLib} GCC_TREAT_WARNINGS_AS_ERRORS "YES")

endif()

# these don't set correct setting in xcode, but keep for Win
if (winOS)
    target_compile_options(${myTargetLib} PRIVATE /FIKramConfig.h)
    target_compile_options(${myTargetApp} PRIVATE /FIKramConfig.h)

    # TODO: switch to add_target_definitions

    # to turn off exceptions/rtti use /GR and /EHsc replacement
    string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    # fixes STL
    target_compile_definitions(${myTargetLib} PRIVATE
        "-D_D_HAS_EXCEPTIONS=0")
    target_compile_definitions(${myTargetApp} PRIVATE
        "-D_D_HAS_EXCEPTIONS=0")

endif()

# ALL_BUILD can't execute an app, so change the startup project to something runnable
set(CMAKE_DEFAULT_STARTUP_PROJECT ${myTarget})
 
file(GLOB_RECURSE sources CONFIGURE_DEPENDS 
	"${PROJECT_SOURCE_DIR}/source/*.cpp"
	"${PROJECT_SOURCE_DIR}/source/*.mm"
    "${PROJECT_SOURCE_DIR}/source/*.h"
)

file(GLOB_RECURSE appSources CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/source/app/*.cpp"
    "${PROJECT_SOURCE_DIR}/source/app/*.mm"
    "${PROJECT_SOURCE_DIR}/source/app/*.h"
)

# no objc on winOS
if (winOS)
list(FILTER sources EXCLUDE REGEX ".*ateencoder.mm$")
list(FILTER sources EXCLUDE REGEX ".*ateencoder.h$")
endif()

# remove files not used
list(FILTER sources EXCLUDE REGEX ".*test.cpp$")
list(FILTER sources EXCLUDE REGEX ".squishgen.cpp$")
list(FILTER sources EXCLUDE REGEX ".Etc.cpp$")
list(FILTER sources EXCLUDE REGEX ".Etc.h$")

# wish I could still see these in project, not just strip them out
list(FILTER sources EXCLUDE REGEX ".astcenccli_image_load_store.cpp$")
list(FILTER sources EXCLUDE REGEX ".astcenccli_error_metrics.cpp$")
list(FILTER sources EXCLUDE REGEX ".astcenccli_platform_dependents.cpp$")
list(FILTER sources EXCLUDE REGEX ".astcenccli_toplevel_help.cpp$")
list(FILTER sources EXCLUDE REGEX ".astcenccli_toplevel.cpp$")

# exclude app/KramMain.cpp from library
list(FILTER sources EXCLUDE REGEX ".*KramMain.cpp$")

# this will preserve hierarchy of sources in a build project
source_group(TREE "${PROJECT_SOURCE_DIR}/source" PREFIX "source" FILES ${sources})

source_group(TREE "${PROJECT_SOURCE_DIR}/source/app" PREFIX "source" FILES ${appSources})

target_include_directories(${myTargetLib} PRIVATE
	"${PROJECT_SOURCE_DIR}/source/"
    "${PROJECT_SOURCE_DIR}/source/kram/"
	#"${PROJECT_SOURCE_DIR}/source/squish/"
	#"${PROJECT_SOURCE_DIR}/source/lodepng"
    "${PROJECT_SOURCE_DIR}/source/astc-encoder/"
	"${PROJECT_SOURCE_DIR}/source/etc2comp/"
	)
 
# only add sources to the library
target_sources(${myTargetLib} PRIVATE ${sources})

target_include_directories(${myTargetApp} PRIVATE
    "${PROJECT_SOURCE_DIR}/kram/")

target_sources(${myTargetApp} PRIVATE ${appSources})

# Needed for asctenc 2.0 in SSE 4.2 mode.  But that is buggy.  No Neon support.
# -msse4.2 -mpopcnt -DASTCENC_SSE=42 -DASTCENC_AVX=0 -DASTCENC_POPCNT=1 -DASTCENC_VECALIGN=16

# want code to run on ARM and on Intel, and SSE/AVX optimizations aren't extensive in astcenc
target_compile_definitions(${myTargetLib} PRIVATE
    -DASTCENC_SSE=0 -DASTCENC_AVX=0 -DASTCENC_POPCNT=0 -DASTCENC_VECALIGN=16)
target_compile_definitions(${myTargetApp} PRIVATE
    -DASTCENC_SSE=0 -DASTCENC_AVX=0 -DASTCENC_POPCNT=0 -DASTCENC_VECALIGN=16)

# now can set as defines
target_compile_definitions(${myTargetLib} PRIVATE
    "-DCOMPILE_ATE=${COMPILE_ATE}"
    "-DCOMPILE_BCENC=${COMPILE_BCENC}"
    "-DCOMPILE_ETCENC=${COMPILE_ETCENC}"
    "-DCOMPILE_SQUISH=${COMPILE_SQUISH}"
    "-DCOMPILE_ASTCENC=${COMPILE_ASTCENC}"
)
target_compile_definitions(${myTargetApp} PRIVATE
    "-DCOMPILE_ATE=${COMPILE_ATE}"
    "-DCOMPILE_BCENC=${COMPILE_BCENC}"
    "-DCOMPILE_ETCENC=${COMPILE_ETCENC}"
    "-DCOMPILE_SQUISH=${COMPILE_SQUISH}"
    "-DCOMPILE_ASTCENC=${COMPILE_ASTCENC}"
)

if (macOS)
    # turn on all warnings
    target_compile_options(${myTargetLib} PRIVATE -W -Wall)
    target_compile_options(${myTargetApp} PRIVATE -W -Wall)
endif()

if (winOS)
    # turn on most warnings
    target_compile_options(${myTargetLib} PRIVATE /W3)
    target_compile_options(${myTargetApp} PRIVATE /W3)

    # https://github.com/microsoft/onnxruntime/pull/3373/files/a5648dcb0cc9081d4b092bed12204efb21e8b8dd
    # stick with AVX since it's more prevalent, and needed for fast fp16 <-> fp32
    target_compile_options(${myTargetLib} PRIVATE /arch:AVX)
    target_compile_options(${myTargetApp} PRIVATE /arch:AVX)
    
endif()

# link app to the library
target_link_libraries(${myTargetApp} ${myTargetLib})


