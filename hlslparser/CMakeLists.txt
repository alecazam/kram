#-----------------------------------------------------
# hlslparser 

# now setup the app project
set(myTargetApp hlslparser)
project(${myTargetApp})
add_executable(${myTargetApp})

#-----------------------------------------------------
    
if (BUILD_MAC)
    set_target_properties(${myTargetApp} PROPERTIES
        XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20"
        XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++"
        XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS "avx2"
        
        # turn off exceptions/rtti
        XCODE_ATTRIBUTE_GCC_ENABLE_CPP_EXCEPTIONS NO
        XCODE_ATTRIBUTE_GCC_ENABLE_CPP_RTTI NO
    
        # can't believe this isn't on by default in CMAKE
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    
        #-------------------------
        
        # set debug style for apps
        XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO"
    
        #-------------------------
        # app specific settings
        
        # this drops app from 762KB to 174KB with only ATE enabled
        # note about needing -gfull instead of -gused here or debug info messed up:
        # https://gist.github.com/tkersey/39b4fe69e14b859889ffadccb009e397
        XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING YES
        XCODE_ATTRIBUTE_LLVM_LTO[variant=Release] "Incremental"
    
        #-------------------------
        # for now disable signing, and just "sign to run locally"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.hialec.hlslparser"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
    )

    target_compile_options(${myTargetApp} PRIVATE -W -Wall)

endif()

#-----------------------------------------------------
    
if (BUILD_WIN)
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
    
    # all warnings, AVX2, and multiprocess compiles,
    # eliminate duplicate strings, embed full path
    target_compile_options(${myTargetApp} PRIVATE /W3 -march=haswell -mf16c -mfma /MP /GF /FC)
    
    # fix STL (don't use -D here, will remove)
    target_compile_definitions(${myTargetApp} PRIVATE _ITERATOR_DEBUG_LEVEL=0 _HAS_EXCEPTIONS=0)

    if (CMAKE_BUILD_TYPE EQUAL "Debug")
        target_compile_definitions(${myTargetApp} PRIVATE "/INCREMENTAL")
        
    elseif (CMAKE_BUILD_TYPE EQUAL "Release")
        # only dead strip on Release builds since this disables Incremental linking, may want Profile build that doesn't use this
        target_compile_definitions(${myTargetApp} PRIVATE "/OPT:REF")
        
        # other possibliities
        # /GL - whole program optimization
        # /Gy - edit and continue with function level linking (no clang)
        # /Oi - enable intrinsic functions
    
    endif()
    
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


