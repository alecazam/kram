cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)

#-----------------------------------------------------

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

#-----------------------------------------------------
# libkram

# these are names, but app will be renamed to kram at the end
set(myTargetLib libkram)

# **** This will create libkram.a or .so or kram.lib depending on platform.
add_library(${myTargetLib} STATIC)

# turn off pch
set_target_properties(${myTargetLib} PROPERTIES 
    # rename lib to kram, libkram will then be the output
    OUTPUT_NAME kram

    DISABLE_PRECOMPILE_HEADERS ON
)

file(GLOB_RECURSE libSources CONFIGURE_DEPENDS 
	"${PROJECT_SOURCE_DIR}/source/astc-encoder/*.cpp"
	"${PROJECT_SOURCE_DIR}/source/astc-encoder/*.h"

    # ATE is Apple specific (ios + macOS)
    "${PROJECT_SOURCE_DIR}/source/ate/*.mm"
    "${PROJECT_SOURCE_DIR}/source/ate/*.h"

    "${PROJECT_SOURCE_DIR}/source/bc7enc/*.cpp"
    "${PROJECT_SOURCE_DIR}/source/bc7enc/*.h"

    "${PROJECT_SOURCE_DIR}/source/etc2comp/*.cpp"
    "${PROJECT_SOURCE_DIR}/source/etc2comp/*.h"

    "${PROJECT_SOURCE_DIR}/source/heman/*.cpp"
    "${PROJECT_SOURCE_DIR}/source/heman/*.h"

    "${PROJECT_SOURCE_DIR}/source/kram/*.cpp"
    "${PROJECT_SOURCE_DIR}/source/kram/*.h"

    "${PROJECT_SOURCE_DIR}/source/lodepng/*.cpp"
    "${PROJECT_SOURCE_DIR}/source/lodepng/*.h"

    "${PROJECT_SOURCE_DIR}/source/squish/*.cpp"
    "${PROJECT_SOURCE_DIR}/source/squish/*.h"

    "${PROJECT_SOURCE_DIR}/source/tmpfileplus/*.cpp"
    "${PROJECT_SOURCE_DIR}/source/tmpfileplus/*.h"
)

# no objc on winOS
if (winOS)
    list(FILTER libSources EXCLUDE REGEX ".*ateencoder.mm$")
    list(FILTER libSources EXCLUDE REGEX ".*ateencoder.h$")
endif()

# remove files not used
list(FILTER libSources EXCLUDE REGEX ".*test.cpp$")
list(FILTER libSources EXCLUDE REGEX ".squishgen.cpp$")
list(FILTER libSources EXCLUDE REGEX ".Etc.cpp$")
list(FILTER libSources EXCLUDE REGEX ".Etc.h$")

# wish I could still see these in project, not just strip them out
list(FILTER libSources EXCLUDE REGEX ".astcenccli_image_load_store.cpp$")
list(FILTER libSources EXCLUDE REGEX ".astcenccli_error_metrics.cpp$")
list(FILTER libSources EXCLUDE REGEX ".astcenccli_platform_dependents.cpp$")
list(FILTER libSources EXCLUDE REGEX ".astcenccli_toplevel_help.cpp$")
list(FILTER libSources EXCLUDE REGEX ".astcenccli_toplevel.cpp$")

# this will preserve hierarchy of sources in a build project
source_group(TREE "${PROJECT_SOURCE_DIR}/source" PREFIX "source" FILES ${libSources})

target_include_directories(${myTargetLib} PUBLIC
    "${PROJECT_SOURCE_DIR}/source/kram/"
)

target_include_directories(${myTargetLib} PRIVATE
    "${PROJECT_SOURCE_DIR}/source/"
    #"${PROJECT_SOURCE_DIR}/source/kram/"
    #"${PROJECT_SOURCE_DIR}/source/squish/"
    #"${PROJECT_SOURCE_DIR}/source/lodepng"
    "${PROJECT_SOURCE_DIR}/source/astc-encoder/"
    "${PROJECT_SOURCE_DIR}/source/etc2comp/"
    )
 
# only add sources to the library
target_sources(${myTargetLib} PRIVATE ${libSources})

if (APPLE)
    set_target_properties(${myTargetLib} PROPERTIES
        # Note: match this up with CXX version
        # c++11 min
        XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++14"
        XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++"

        # avx1
        XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS "avx"
        
        # turn off exceptions/rtti
        XCODE_ATTRIBUTE_GCC_ENABLE_CPP_EXCEPTIONS NO
        XCODE_ATTRIBUTE_GCC_ENABLE_CPP_RTTI NO
    )

    target_compile_options(${myTargetLib} PRIVATE -include KramConfig.h -W -Wall)
elseif (WIN32)
    
    # TODO: switch to add_target_definitions

    # to turn off exceptions/rtti use /GR and /EHsc replacement
    string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    # force include and fix STL
    target_compile_options(${myTargetLib} PRIVATE /FIKramConfig.h /W3 /arch:AVX)
    target_compile_definitions(${myTargetLib} PRIVATE "-D_D_HAS_EXCEPTIONS=0")

endif()


target_compile_definitions(${myTargetLib} PUBLIC
    -DASTCENC_SSE=42 -DASTCENC_AVX=1 -DASTCENC_POPCNT=0
    -DASTCENC_VECALIGN=16 -DASTCENC_ISA_INVARIANCE=0)

target_compile_definitions(${myTargetLib} PUBLIC
    "-DCOMPILE_ATE=${COMPILE_ATE}"
    "-DCOMPILE_BCENC=${COMPILE_BCENC}"
    "-DCOMPILE_ETCENC=${COMPILE_ETCENC}"
    "-DCOMPILE_SQUISH=${COMPILE_SQUISH}"
    "-DCOMPILE_ASTCENC=${COMPILE_ASTCENC}"
)
