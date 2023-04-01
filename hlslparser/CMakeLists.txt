#cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)

#-----------------------------------------------------

set(BUILD_MAC FALSE)
set(BUILD_WIN FALSE)

if (APPLE)
    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        message("build for iOS")
        set(BUILD_IOS TRUE)
    else()
        message("build for macOS")
        set(BUILD_MAC TRUE)
    endif()
elseif (WIN32)
    message("build for win x64")
    set(BUILD_WIN TRUE)
endif()

#-----------------------------------------------------
# hlslparser 

# now setup the app project
set(myTargetApp hlslparser)

# not using version in cmake anymore, this is pulled in by KramVersion.h
if (BUILD_MAC)
    project(${myTargetApp} LANGUAGES C CXX OBJCXX)
elseif (BUILD_WIN)
    project(${myTargetApp} LANGUAGES C CXX)
endif()

# **** this the executable target ****, for a CLI App
add_executable(${myTargetApp})

#-----------------------------------------------------
    
if (BUILD_MAC)
    # ate is a macOS/iOS only library, and it varies in encode support by OS revision
    #target_link_libraries(${myTargetApp}
    #    ate
    #    libkram)

    set_target_properties(${myTargetApp} PROPERTIES
         # Note: match this up with CXX version
        # c++11 min
        XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20"
        XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++"

        # removed this in case run on Rosetta.  Not sure if this is Intel/Arm64 build.
        # this is Intel specific, and no settings for Neon
        # avx1
        #XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS "avx"
        
        # turn off exceptions/rtti
        XCODE_ATTRIBUTE_GCC_ENABLE_CPP_EXCEPTIONS NO
        XCODE_ATTRIBUTE_GCC_ENABLE_CPP_RTTI NO
    
        # can't believe this isn't on by default in CMAKE
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    
        #-------------------------
        
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
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ba.hlslparser"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
    )

    target_compile_options(${myTargetApp} PRIVATE -W -Wall)

elseif (BUILD_WIN)
    #target_link_libraries(${myTargetApp} libkram)

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
    
    # all warnings, AVX1, and multiprocess compiles
    target_compile_options(${myTargetApp} PRIVATE /W3 /arch:AVX /MP /GF /FC)
    
    # fix STL
    target_compile_definitions(${myTargetApp} PRIVATE "-D_HAS_EXCEPTIONS=0 -D_ITERATOR_DEBUG_LEVEL=0")
    
    if (CMAKE_BUILD_TYPE EQUAL "Debug")
        target_compile_definitions(${myTargetLib} PRIVATE "/INCREMENTAL")
        
    elseif (CMAKE_BUILD_TYPE EQUAL "Release")
        # only dead strip on Release builds since this disables Incremental linking, may want Profile build that doesn't use this
        target_compile_definitions(${myTargetLib} PRIVATE "/OPT:REF")
        
        # other possibliities
        # /GL - whole program optimization
        # /Gy - edit and continue with function level linking
        # /Oi - enable intrinsic functions
    
    endif()
    
elseif (BUILD_UNIX)
    #target_link_libraries(${myTargetApp} libkram)

    # TODO: finish this
    
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


