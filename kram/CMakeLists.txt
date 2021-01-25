#cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)

#-----------------------------------------------------
# kram 

# now setup the app project, and link to libkram
set(myTargetApp kram)

if (APPLE)
project(${myTargetApp} LANGUAGES C CXX OBJCXX
    VERSION 0.9.0
)
elseif (WIN32)
project(${myTargetApp} LANGUAGES C CXX
    VERSION 0.9.0
)
endif()

# **** this the executable target ****, for a CLI App
add_executable(${myTargetApp})

#-----------------------------------------------------
    
if (APPLE)
    # ate is a macOS/iOS only library, and it varies in encode support by OS revision
    target_link_libraries(${myTargetApp}
        ate
        libkram)

    set_target_properties(${myTargetApp} PROPERTIES
         # Note: match this up with CXX version
        # c++11 min
        XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++14"
        XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++"

        # avx1
        XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS "avx"
        
        # turn off exceptions/rtti
        XCODE_ATTRIBUTE_GCC_ENABLE_CPP_EXCEPTIONS NO
        XCODE_ATTRIBUTE_GCC_ENABLE_CPP_RTTI NO
    
        # set debug style for apps
        XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO"
    
        # this drops app from 762KB to 174KB with only ATE enabled
        # note about needing -gfull instead of -gused here or debug info messed up:
        # https://gist.github.com/tkersey/39b4fe69e14b859889ffadccb009e397
        XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING YES
        XCODE_ATTRIBUTE_LLVM_LTO[variant=Release] "Incremental"
    
        #-------------------------
        # for now disable signing, and just "sign to run locally"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ba.kram"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
    )

    target_compile_options(${myTargetApp} PRIVATE -W -Wall)

elseif (WIN32)
    target_link_libraries(${myTargetApp} "libkram")

    # When Win rebuilds library, it doesn't relink app to correct code when you
    # build the app target project.  Breakpoints stop working after any library source edit,
    # and you have to rebuild solution to get the correct code to exectute.  Since 2014.  Try this.
    # And BUILD_ALL never launches properly.
    # https://cmake.org/pipermail/cmake/2014-October/058798.html
    SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
    SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

    # TODO: switch to add_target_definitions

    # to turn off exceptions/rtti use /GR and /EHsc replacement
    string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    # don't need force with apps, since they only access kram folder files which include KramConfig
    # force include and fix STL
    #target_compile_options(${myTargetApp} PRIVATE /FIKramConfig.h)
    target_compile_definitions(${myTargetApp} PRIVATE "-D_D_HAS_EXCEPTIONS=0")

    target_compile_options(${myTargetApp} PRIVATE /W3 /arch:AVX)
endif()


#------------------
 
file(GLOB_RECURSE appSources CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/*.cpp"
    "${PROJECT_SOURCE_DIR}/*.h"
)

source_group(TREE "${PROJECT_SOURCE_DIR}" PREFIX "source" FILES ${appSources})

target_include_directories(${myTargetApp} PRIVATE
    "${PROJECT_SOURCE_DIR}"
)

target_sources(${myTargetApp} PRIVATE ${appSources})


