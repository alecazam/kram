#-----------------------------------------------------

# TODO: hook these up to added code below, and pass to as command line settings
option(ATE "Compile ATE Encoder" ON)
option(ETCENC "Compile ETC2Comp Encoder" ON)
option(SQUISH "Compile Squish Encoder" ON)
option(ASTCENC "Compile ASTCenc Encoder" ON)
option(BCENC "Compile BCenc Encoder" ON)
option(COMP "Compile Compressonator Encoder" ON)

option(EASTL "Compile EASTL" OFF)

# convert ON to 1, UGH
set(COMPILE_ATE 0)
set(COMPILE_BCENC 0)
set(COMPILE_ETCENC 0)
set(COMPILE_SQUISH 0)
set(COMPILE_ASTCENC 0)
set(COMPILE_COMP 0)

if (ATE AND BUILD_MAC)
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

if (COMP)
    set(COMPILE_COMP 1)
endif()

#-----------------------------------------------------
# stl used

# replaces parts of std/stl with eastl
set(COMPILE_EASTL 0)
if (EASTL)
    set(COMPILE_EASTL 1)
endif()

#-----------------------------------------------------
# libkram

# these are names, but app will be renamed to kram at the end
set(myTargetLib libkram)

# **** This will create libkram.a or .so or kram.lib depending on platform.
# can also use OBJECT or SHARED, object cuts compile time
add_library(${myTargetLib} STATIC)

if (CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    set(DEBUG 1)
endif()

if (DEBUG)
    set_target_properties(${myTargetLib} PROPERTIES
        # rename lib to kramd, libkramd will then be the output or kramd.lib
        OUTPUT_NAME kramd
    )
else()
    set_target_properties(${myTargetLib} PROPERTIES
        # rename lib to kram, libkram will then be the output
        OUTPUT_NAME kram
    )
endif()

# add_library doesn't establish a project, so still pointing at root CMake
#set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/libkram)
#set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/libkram)
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

#set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/libkram)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE libSources CONFIGURE_DEPENDS 
	"${SOURCE_DIR}/astc-encoder/*.cpp"
	"${SOURCE_DIR}/astc-encoder/*.h"

    # ATE is Apple specific to macOS)
    "${SOURCE_DIR}/ate/ateencoder.mm"
    "${SOURCE_DIR}/ate/ateencoder.h"

    "${SOURCE_DIR}/bc7enc/*.cpp"
    "${SOURCE_DIR}/bc7enc/*.h"

    "${SOURCE_DIR}/etc2comp/*.cpp"
    "${SOURCE_DIR}/etc2comp/*.h"

    "${SOURCE_DIR}/fmt/*.cpp"
    "${SOURCE_DIR}/fmt/*.h"

    "${SOURCE_DIR}/heman/hedistance.cpp"
    "${SOURCE_DIR}/heman/hedistance.h"

    "${SOURCE_DIR}/kram/*.pch"
    "${SOURCE_DIR}/kram/*.cpp"
    "${SOURCE_DIR}/kram/*.h"

    # other libraries
    "${SOURCE_DIR}/eastl/*.cpp"
    "${SOURCE_DIR}/eastl/*.h"
    
    "${SOURCE_DIR}/lodepng/lodepng.cpp"
    "${SOURCE_DIR}/lodepng/lodepng.h"

    "${SOURCE_DIR}/squish/*.cpp"
    "${SOURCE_DIR}/squish/*.h"

    "${SOURCE_DIR}/compressonator/bc6h/*.cpp"
    "${SOURCE_DIR}/compressonator/bc6h/*.h"

    "${SOURCE_DIR}/tmpfileplus/tmpfileplus.cpp"
    "${SOURCE_DIR}/tmpfileplus/tmpfileplus.h"
     
    "${SOURCE_DIR}/vectormath/*.h"
    "${SOURCE_DIR}/vectormath/*.cpp"
    
    # partial zstd decode-only unity file
    # cd zstd/build/single_file_libs
    # ./combine.sh -r ../../lib -o zstddeclib.c zstddeclib-in.c
    # "${SOURCE_DIR}/zstd/zstddeclib.cpp"
    
    # full zstd encode/decode unity file
    # cd zstd/build/single_file_libs
    # ./combine.sh -r ../../lib -o zstd.c zstd-in.c
    "${SOURCE_DIR}/zstd/zstd.h"
    "${SOURCE_DIR}/zstd/zstd.cpp"
   
    "${SOURCE_DIR}/miniz/miniz.h"
    "${SOURCE_DIR}/miniz/miniz.cpp"
)

# no objc on win or linux
if (BUILD_WIN OR BUILD_LINUX)
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

# this EASTL file is causing problems on Windows with the x86/arch_x86.h include
list(FILTER libSources EXCLUDE REGEX ".atomic.h$")
list(FILTER libSources EXCLUDE REGEX ".atomic.cpp$")

# this is trying to setup C++20 module, but that causes compile failures
list(FILTER libSources EXCLUDE REGEX ".fmt.cpp$")

# this will preserve hierarchy of sources in a build project
source_group(TREE "${SOURCE_DIR}" PREFIX "source" FILES ${libSources})

target_include_directories(${myTargetLib} PUBLIC
    # public since included in other project files
    "${INCLUDE_DIR}/kram/"
    "${INCLUDE_DIR}/eastl/include/"
    "${INCLUDE_DIR}/vectormath/"
)

target_include_directories(${myTargetLib} PRIVATE
    "${INCLUDE_DIR}/astc-encoder/"
    "${INCLUDE_DIR}/ate/"
    "${INCLUDE_DIR}/bc7enc/"
    "${INCLUDE_DIR}/compressonator/bc6h/"
    "${INCLUDE_DIR}/etc2comp/"
    "${INCLUDE_DIR}/fmt/"
    "${INCLUDE_DIR}/heman/"
    "${INCLUDE_DIR}/lodepng"
    "${INCLUDE_DIR}/miniz/"
    "${INCLUDE_DIR}/squish/"
    "${INCLUDE_DIR}/tmpfileplus/"
    "${INCLUDE_DIR}/zstd/"
)
     
# only add sources to the library
target_sources(${myTargetLib} PRIVATE ${libSources})

#-----------------------------------------------------

if (BUILD_LINUX)
    # Enable all warnings, and also enable f16c sims op (only x64 though)
    target_compile_options(${myTargetLib} PRIVATE -W -Wall 
        -mavx2 -mfma -mf16c 
        -fno-exceptions -fno-rtti
        -fdata-sections -ffunction-sections
    )

    # this is already done by pch for libkram, but other projects need the force include inherited
    # force include (public)
    target_compile_options(${myTargetLib} PUBLIC -include KramConfig.h)
endif()

# note: mac build is all done via Xcode workspace/project now, this cmake build is legacy
if (BUILD_MAC)
    set_target_properties(${myTargetLib} PROPERTIES
        # set debug style for apps
        XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO"
    )

    # Enable all warnings, and also enable f16c sims op (only x64 though)
    target_compile_options(${myTargetLib} PRIVATE -W -Wall 
        -mavx2 -mfma -mf16c 
        -fobjc-arc
        -fno-exceptions -fno-rtti
        -fdata-sections -ffunction-sections
    )

    # this is already done by pch for libkram, but other projects need the force include inherited
    # force include (public)
    target_compile_options(${myTargetLib} PUBLIC -include KramConfig.h)
endif()

if (BUILD_WIN)
    # TODO: switch to add_target_definitions

    set_property(TARGET ${myTargetLib} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )

    # this is already done by pch for libkram, but other projects need the force include inherited
    # force include (public)
    target_compile_options(${myTargetLib} PUBLIC /FIKramConfig.h)
       
    # all warnings, AVX2, and multiprocess compiles,
    # eliminate duplicate strings, embed full path
    # TODO: -fdata-sections -ffunction-sections
    target_compile_options(${myTargetLib} PRIVATE  /W3 
        -march=haswell -mf16c -mfma 
        /GR- /EHs-c-
        /MP /GF /FC
    )
    
    # fix STL (don't use -D here, will remove)
    target_compile_definitions(${myTargetLib} PRIVATE _ITERATOR_DEBUG_LEVEL=0 _HAS_EXCEPTIONS=0)

endif()



# This will be force include (-include, /FI) on GCC/clang/VS.
# Can't seem to ref KramPrefix.pch file.  Goes into cmake_pch.hxx file
target_precompile_headers(${myTargetLib} PRIVATE
    ${SOURCE_DIR}/kram/KramPrefix.h
)

# These turns into -D, don't add to the lines
target_compile_definitions(${myTargetLib}
    PUBLIC
    COMPILE_EASTL=${COMPILE_EASTL}
   
    PRIVATE
    COMPILE_ATE=${COMPILE_ATE}
    COMPILE_BCENC=${COMPILE_BCENC}
    COMPILE_ETCENC=${COMPILE_ETCENC}
    COMPILE_SQUISH=${COMPILE_SQUISH}
    COMPILE_ASTCENC=${COMPILE_ASTCENC}
    COMPILE_COMP=${COMPILE_COMP}
)
