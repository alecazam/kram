cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)

# This is only configured for a Mac build, but see kram cli app
# for the Windows configuration.  Eventually port to Win.

# have to add this to each file, or run with this
# -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON
# set(CMAKE_VERBOSE_MAKEFILE ON)

#-----------------------------------------------------
# kramv

# now setup the app project, and link to libkram
set(myTargetApp kram-ps)

# the mac build has ObjC++
project(
    ${myTargetApp}
    #VERSION 0.9.0
    LANGUAGES C CXX OBJCXX
)

add_executable(${myTargetApp} EXCLUDE_FROM_ALL)

#-----------------------------------------------------
    
target_link_libraries(${myTargetApp} 
    ate 
    libkram
    "-framework Cocoa" 
    "-framework AppKit"
)
    
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

    # can't believe this isn't on by default in CMAKE
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    
    #-------------------------
        
    # libs can use dwarf, but apps need dSym generated
    XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
    XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO"

    # this drops app from 762KB to 174KB with only ATE enabled
    # note about needing -gfull instead of -gused here or debug info messed up:
    # https://gist.github.com/tkersey/39b4fe69e14b859889ffadccb009e397
    #XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING YES
    XCODE_ATTRIBUTE_LLVM_LTO[variant=Release] "Incremental"
    
    #-------------------------
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ba.kram-ps"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.ba.kram-ps"
    
    # for now "sign to run locally", or entitlements can't be bundled
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
    
    # use the AssetCatalog for icons
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    
    # TODO: not sure how to set this, nothing online either ?
    # MACOSX_BUNDLE_APP_CATEGORY "Developer Tools"
)

target_compile_options(${myTargetApp} PRIVATE -W -Wall)

#--------------
# sdk

# Don't ever use a , in a set call, it causes the glob to process entire hard drive
# and it never seems to return.  Maybe it's building a glob of all files on HD.

set(SDK_DIR "${PROJECT_SOURCE_DIR}/ext/photoshopsdk/pluginsdk")
set(SDK_SOURCE_DIR "${SDK_DIR}/photoshopapi")
set(SDK_COMMON_DIR "${SDK_DIR}/samplecode/common")

file(GLOB_RECURSE sdkSources CONFIGURE_DEPENDS
    "${SDK_SOURCE_DIR}/*.cpp"
    "${SDK_SOURCE_DIR}/*.h"
    "${SDK_SOURCE_DIR}/*.m"
    "${SDK_SOURCE_DIR}/*.mm"
    "${SDK_SOURCE_DIR}/*.r"
)

file(GLOB_RECURSE sdkCommonSources CONFIGURE_DEPENDS
    "${SDK_COMMON_DIR}/*.cpp"
    "${SDK_COMMON_DIR}/*.h"
    "${SDK_COMMON_DIR}/*.m"
    "${SDK_COMMON_DIR}/*.mm"
    "${SDK_COMMON_DIR}/*.r"
)
    
# TODO: had to modify some files to not use exceptions
# pass those onto Adobe

# TODO: can these be combined into one list?
# this is a win file
list(FILTER sdkCommonSources EXCLUDE REGEX ".pstypelibrary.cpp$")
list(FILTER sdkCommonSources EXCLUDE REGEX ".pstypelibrary.h$")
    
list(FILTER sdkCommonSources EXCLUDE REGEX ".PIDLLInstance.cpp$")
list(FILTER sdkCommonSources EXCLUDE REGEX ".PIDLLInstance.h$")
    
list(FILTER sdkCommonSources EXCLUDE REGEX ".PIUFile.cpp$")
list(FILTER sdkCommonSources EXCLUDE REGEX ".PIUFile.h$")
    
list(FILTER sdkCommonSources EXCLUDE REGEX ".PSConstantArray.cpp$")
list(FILTER sdkCommonSources EXCLUDE REGEX ".*Win*.cpp$")
list(FILTER sdkCommonSources EXCLUDE REGEX ".PIWinUI.cpp$")

# intermingled Win files in with Mac
list(FILTER sdkSources EXCLUDE REGEX ".*Win*.cpp$")

source_group(TREE "${SDK_SOURCE_DIR}" PREFIX "sdk" FILES ${sdkSources})
source_group(TREE "${SDK_COMMON_DIR}" PREFIX "sdkcommon" FILES ${sdkCommonSources})

set_target_properties(${myTargetApp} PROPERTIES

    XCODE_ATTRIBUTE_WRAPPER_EXTENSION "plugin"
    
    # these aren't supported anymore, only on archival projects with Rez support
    #XCODE_ATTRIBUTE_REZ_PREFIX_FILE
    #    $(SDK_COMMON_DIR)/includes/MachOMacrezXcode.h
    #XCODE_ATTRIBUTE_REZ_SEARCH_PATHS
    #    $(SDK_SOURCE_DIR)/resources/
    #    $(SDK_SOURCE_DIR)/photoshop/
    #    $(SDK_COMMON_DIR)/includes/
)


#--------------
# sources

set(KPS_SOURCE_DIR "${PROJECT_SOURCE_DIR}/kps")

file(GLOB_RECURSE appSources CONFIGURE_DEPENDS
    "${KPS_SOURCE_DIR}/*.cpp"
    "${KPS_SOURCE_DIR}/*.mm"
    "${KPS_SOURCE_DIR}/*.h"
    
    # TODO: also include .r files from
    "${KPS_SOURCE_DIR}/*.r"
    "${KPS_SOURCE_DIR}/${myTargetApp}.rsrc"
)

file(GLOB_RECURSE appNibSources CONFIGURE_DEPENDS
    "${KPS_SOURCE_DIR}/*.xib" # TODO: move this to resource area below
)

# win files
list(FILTER appSources EXCLUDE REGEX ".KPSInputDialog.cpp$")
list(FILTER appSources EXCLUDE REGEX ".KPSOutputDialog.cpp$")
list(FILTER appSources EXCLUDE REGEX ".resource.h$")

source_group(TREE "${KPS_SOURCE_DIR}" PREFIX "source" FILES ${appSources})

target_sources(${myTargetApp} PRIVATE
    ${appSources}
    
    ${sdkSources}
    ${sdkCommonSources}
)

target_include_directories(${myTargetApp} PRIVATE
    "${KPS_SOURCE_DIR}"
    
    # the sdk includes and resources
    "${SDK_SOURCE_DIR}/photoshop"
    "${SDK_SOURCE_DIR}/pica_sp"
    "${SDK_SOURCE_DIR}/resources"
    
    "${SDK_COMMON_DIR}/includes"
    "${SDK_COMMON_DIR}/resources"
)


#--------------
# resources

# for some reason the Cmake template gens/add an Info.plist even though we override it
set_target_properties(${myTargetApp} PROPERTIES
    MACOSX_BUNDLE TRUE
    
    MACOSX_BUNDLE_INFO_PLIST ${KPS_SOURCE_DIR}/mac/Info.plist
    XCODE_ATTRIBUTE_INFOPLIST_FILE ${KPS_SOURCE_DIR}/mac/Info.plist
    #XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${KPS_SOURCE_DIR}/mac/kramv.entitlements
)

target_sources(${myTargetApp} PRIVATE
#    Assets.xcassets
#    Base.lproj/Main.storyboard
    ${appNibSources}
    
    ${KPS_SOURCE_DIR}/mac/Info.plist
    
#    ${KPS_SOURCE_DIR}/mac/Info.plist
#    kramv.entitlements
)

# only these 2 resources are copied into the Resource, the other two are signed
# Can't lowercase Resources or files don't go to correct place
set_source_files_properties(
    ${appNibSources}
    
    # this is created in the PRE_BUILD step below
    ${KPS_SOURCE_DIR}/${myTargetApp}.rsrc
                        
    PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
)

#--------------
# rez

# note that despite the usage printed, -i and -s don't actually work
# for some reason only -I actually includes search paths.  Ugh.
# But even though this succeeds, it gens a 0 size rsrc file.  Ugh!

# turned off for now, and checking in pre-built resource
# but app still can't find _main entrpoint.

if (FALSE)

execute_process(
    COMMAND xcrun -f Rez
    OUTPUT_VARIABLE rezCompiler
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_custom_command(TARGET ${myTargetApp} PRE_BUILD
    DEPENDS ${KPS_SOURCE_DIR}/KPS.r
    COMMAND ${rezCompiler}
    -I ${SDK_SOURCE_DIR}/resources/
    -I ${SDK_SOURCE_DIR}/photoshop/
    # -I ${SDK_COMMON_DIR}/includes/
    
    -arch x86_64
    
    # needs this for Carbon.r and CoreServices.r in the Adobe .r headers
    #-F Carbon
    #-F CoreServices
    -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/

    -o "${KPS_SOURCE_DIR}/${myTargetApp}.rsrc"
    ${KPS_SOURCE_DIR}/KPS.r
)

endif()
