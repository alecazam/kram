#-----------------------------------------------------
# kram 

# now setup the app project, and link to libkram
set(myTargetApp kram)
project(${myTargetApp})
add_executable(${myTargetApp})

#-----------------------------------------------------
    
if (BUILD_LINUX)
    # Enable all warnings, and also enable f16c sims op (only x64 though)
    target_compile_options(${myTargetApp} PRIVATE -W -Wall 
        -mavx2 -mfma -mf16c 
        -fno-exceptions -fno-rtti
        -fdata-sections -ffunction-sections
    )

    # librt is for clock_gettime
    target_link_libraries(${myTargetApp} PUBLIC 
        libkram 
        librt)

endif()

if (BUILD_MAC)
    # ate is a macOS/iOS only library, and it varies in encode support by OS revision
    target_link_libraries(${myTargetApp} PUBLIC
        ate
        libkram
    )

    set_target_properties(${myTargetApp} PROPERTIES
        #XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++20"
        #XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++"
        
        # can't believe this isn't on by default in CMAKE
        #XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    
        #-------------------------
        
        # set debug style for apps
        XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO"
    
        #-------------------------
        # app specific settings
       
        # this drops app from 762KB to 174KB with only ATE enabled
        # note about needing -gfull instead of -gused here or debug info messed up:
        # https://gist.github.com/tkersey/39b4fe69e14b859889ffadccb009e397
        #XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING YES

        # This is LTO
        #XCODE_ATTRIBUTE_LLVM_LTO[variant=Release] "Incremental"
    
        #-------------------------
        # for now disable signing, and just "sign to run locally"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.hialec.kram"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
    )

    target_compile_options(${myTargetApp} PRIVATE -W -Wall
        -mavx2 -mfma -mf16c 
        -fno-exceptions -fno-rtti
        -fdata-sections -ffunction-sections
    )

    # only turn on in Release in case this disables incremental linking
    if (CMAKE_BUILD_TYPE EQUAL "Release")
        add_link_options(${myTargetApp}, "-dead_strip")
    endif()
endif()

#-----------------------------------------------------

if (BUILD_WIN)

    # Use the static linked libs, or the exe needs the VCRuntimeDLL installed
    set_property(TARGET ${myTargetApp} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    
    target_link_libraries(${myTargetApp} PUBLIC libkram)

    # When Win rebuilds library, it doesn't relink app to correct code when you
    # build the app target project.  Breakpoints stop working after any library source edit,
    # and you have to rebuild solution to get the correct code to execute.  Since 2014.  Try this.
    # And BUILD_ALL never launches properly.
    # https://cmake.org/pipermail/cmake/2014-October/058798.html
    SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
    SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

    # don't need force with apps, since they only access kram folder files which include KramConfig
    
    # all warnings, AVX2, and multiprocess compiles
    target_compile_options(${myTargetApp} PRIVATE /W3 
	   -march=haswell -mf16c -mfma 
	   /GR- /EHs-c-
	   /MP /GF /FC
    )
    
    # fix STL
    target_compile_definitions(${myTargetApp} PRIVATE -D_ITERATOR_DEBUG_LEVEL=0 -D_HAS_EXCEPTIONS=0)
         
    if (CMAKE_BUILD_TYPE EQUAL "Release")
        # This disables Incremental linking, so only set on Release
        add_link_options(${myTargetApp} PRIVATE "/OPT:REF")
        
        # other possibliities
        # /GL - whole program optimization
        # /Gy - edit and continue with function level linking (unsupported for clang)
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


