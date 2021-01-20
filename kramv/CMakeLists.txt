cmake_minimum_required(VERSION 3.19.1 FATAL_ERROR)

# This is only configured for a Mac build, but see kram cli app
# for the Windows configuration.  Eventually port to Win.

#-----------------------------------------------------
# kramv

# now setup the app project, and link to libkram
set(myTargetApp kramv)

# the mac build has ObjC++
project(
    ${myTargetApp}
    VERSION 0.9.0
    LANGUAGES C CXX OBJCXX
)

#------------------

# **** this the executable target ****, GUI App
add_executable(${myTargetApp})

#-----------------------------------------------------
    

# can this be inherited from library?
target_link_libraries(${myTargetApp} 
    ate 
    libkram
    "-framework Cocoa" 
    "-framework Metal" 
    "-framework MetalKit" 
    "-framework ModelIO"
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

    # libs can use dwarf, but apps need dSym generated
    XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
    XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO"

    # this drops app from 762KB to 174KB with only ATE enabled
    # note about needing -gfull instead of -gused here or debug info messed up:
    # https://gist.github.com/tkersey/39b4fe69e14b859889ffadccb009e397
    XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING YES
    XCODE_ATTRIBUTE_LLVM_LTO[variant=Release] "Incremental"
    
    #-------------------------
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.ba.kramv"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.ba.kramv"
    
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
# sources
file(GLOB_RECURSE appSources CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/*.cpp"
    "${PROJECT_SOURCE_DIR}/*.mm"
    "${PROJECT_SOURCE_DIR}/*.h"
)

source_group(TREE "${PROJECT_SOURCE_DIR}" PREFIX "source" FILES ${appSources})

target_sources(${myTargetApp} PRIVATE
    ${appSources})

target_include_directories(${myTargetApp} PRIVATE
    "${PROJECT_SOURCE_DIR}"
)

#--------------
# shaders

file(GLOB_RECURSE shaderSources CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/*.metal"
)

source_group(TREE "${PROJECT_SOURCE_DIR}" PREFIX "shaders" FILES ${shaderSources})

target_sources(${myTargetApp} PRIVATE 
    ${shaderSources})

set_source_files_properties(${shaderSources} PROPERTIES
    LANGUAGE METAL)


#--------------
# resources

# this might be one way to override the template
#configure_file(Info.plist.in ${PROJECT_SOURCE_DIR}/Info.plist)
#configure_file(InfoPlist.strings.in ${PROJECT_SOURCE_DIR}/InfoPlist.strings)

# for some reason the template gens an Info.plist even though we override it
set_target_properties(${myTargetApp} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/Info.plist
    XCODE_ATTRIBUTE_INFOPLIST_FILE ${PROJECT_SOURCE_DIR}/Info.plist
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${PROJECT_SOURCE_DIR}/kram.entitlements
)

target_sources(${myTargetApp} PRIVATE
    Assets.xcassets
    Base.lproj/Main.storyboard
    Info.plist
    kramv.entitlements
)

# only these 2 resources are copied into the Resource, the other two are signed
# Can't lowercase Resources or files don't go to correct place
set_source_files_properties(Assets.xcassets Base.lproj/Main.storyboard PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
)

