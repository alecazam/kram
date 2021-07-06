#cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)

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

if (ATE AND APPLE)
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
# can also use OBJECT or SHARED, object cuts compile time
add_library(${myTargetLib} STATIC)

# turn off pch
set_target_properties(${myTargetLib} PROPERTIES 
    # rename lib to kram, libkram will then be the output
    OUTPUT_NAME kram

    DISABLE_PRECOMPILE_HEADERS ON
)

# add_library doesn't establish a project, so still pointing at root CMake
set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/libkram)

file(GLOB_RECURSE libSources CONFIGURE_DEPENDS 
	"${SOURCE_DIR}/astc-encoder/*.cpp"
	"${SOURCE_DIR}/astc-encoder/*.h"

    # ATE is Apple specific to macOS)
    "${SOURCE_DIR}/ate/*.mm"
    "${SOURCE_DIR}/ate/*.h"

    "${SOURCE_DIR}/bc7enc/*.cpp"
    "${SOURCE_DIR}/bc7enc/*.h"

    "${SOURCE_DIR}/etc2comp/*.cpp"
    "${SOURCE_DIR}/etc2comp/*.h"

    "${SOURCE_DIR}/heman/*.cpp"
    "${SOURCE_DIR}/heman/*.h"

    "${SOURCE_DIR}/kram/*.cpp"
    "${SOURCE_DIR}/kram/*.h"

    "${SOURCE_DIR}/lodepng/*.cpp"
    "${SOURCE_DIR}/lodepng/*.h"

    "${SOURCE_DIR}/squish/*.cpp"
    "${SOURCE_DIR}/squish/*.h"

    "${SOURCE_DIR}/tmpfileplus/*.cpp"
    "${SOURCE_DIR}/tmpfileplus/*.h"
    
    # decoder unity file
    # cd zstd/build/single_file_libs
    # ./combine.sh -r ../../lib -o zstddeclib.c zstddeclib-in.c
    # "${SOURCE_DIR}/zstd/zstddeclib.cpp"
    
    # full unity file
    # cd zstd/build/single_file_libs
    # ./combine.sh -r ../../lib -o zstd.c zstd-in.c
    "${SOURCE_DIR}/zstd/zstd.h"
    "${SOURCE_DIR}/zstd/zstd.cpp"
   
    "${SOURCE_DIR}/miniz/miniz.h"
    "${SOURCE_DIR}/miniz/miniz.cpp"
)

# no objc on win or linux
if (WIN32)
    list(FILTER libSources EXCLUDE REGEX ".*ateencoder.mm$")
    list(FILTER libSources EXCLUDE REGEX ".*ateencoder.h$")
elseif (UNIXBUILD)
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
source_group(TREE "${SOURCE_DIR}" PREFIX "source" FILES ${libSources})

target_include_directories(${myTargetLib} PUBLIC
    "${SOURCE_DIR}/kram/"
)

target_include_directories(${myTargetLib} PRIVATE
    "${SOURCE_DIR}/"
    #"${SOURCE_DIR}/kram/"
    #"${SOURCE_DIR}/squish/"
    #"${SOURCE_DIR}/lodepng"
    "${SOURCE_DIR}/astc-encoder/"
    "${SOURCE_DIR}/etc2comp/"
    "${SOURCE_DIR}/zstd/"
    )
 
target_include_directories(${myTargetLib} PUBLIC
    "${SOURCE_DIR}/miniz/"
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
        
        # can't believe this isn't on by default in CMAKE
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    )

    target_compile_options(${myTargetLib} PRIVATE -include KramConfig.h -W -Wall)
elseif (WIN32)
    
    # TODO: switch to add_target_definitions

    # to turn off exceptions/rtti use /GR and /EHsc replacement
    string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    # force include
    target_compile_options(${myTargetLib} PRIVATE /FIKramConfig.h)
        
    # all warnings, AVX1, and multiprocess compiles
    target_compile_options(${myTargetLib} PRIVATE  /W3 /arch:AVX /MP)
    
    # fix STL
    target_compile_definitions(${myTargetLib} PRIVATE "-D_D_HAS_EXCEPTIONS=0 -D_ITERATOR_DEBUG_LEVEL=0")
    
elseif (UNIXBUILD)
    # TODO: finish this
    
    target_compile_options(${myTargetLib} PRIVATE -include KramConfig.h -W -Wall)
    
endif()

target_compile_definitions(${myTargetLib} PUBLIC
    "-DCOMPILE_ATE=${COMPILE_ATE}"
    "-DCOMPILE_BCENC=${COMPILE_BCENC}"
    "-DCOMPILE_ETCENC=${COMPILE_ETCENC}"
    "-DCOMPILE_SQUISH=${COMPILE_SQUISH}"
    "-DCOMPILE_ASTCENC=${COMPILE_ASTCENC}"
)
