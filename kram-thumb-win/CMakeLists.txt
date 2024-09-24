
# dll output can be renamed for debug vs. release, but is hard to debug
set(myTargetLib kram-thumb-win)

# caller already set all this
# project(${myTargetLib} LANGUAGES CXX)

set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /DEF:${CMAKE_CURRENT_SOURCE_DIR}/KramThumbProvider.def /NODEFAULTLIB:LIBCMT")

set(SOURCE_FILES
    Dll.cpp
    Dll.rc
    KramThumbProvider.cpp
)

# Module is a DLL library
add_library(${myTargetLib} MODULE ${SOURCE_FILES})

 # to turn off exceptions/rtti use /GR and /EHsc replacement
string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
# all warnings, AVX2, and multiprocess compiles
target_compile_options(${myTargetLib} PRIVATE /W3 -march=haswell -mf16c -mfma /MP /GF /FC)
  
target_compile_definitions(${myTargetLib} PRIVATE -D_ITERATOR_DEBUG_LEVEL=0 -D_HAS_EXCEPTIONS=0 -DUNICODE -D_UNICODE)
    
if (CMAKE_BUILD_TYPE EQUAL "Debug")
    target_compile_definitions(${myTargetLib} PRIVATE "/INCREMENTAL")
        
elseif (CMAKE_BUILD_TYPE EQUAL "Release")
    # only dead strip on Release builds since this disables Incremental linking, may want Profile build that doesn't use this
    target_compile_definitions(${myTargetLib} PRIVATE "/OPT:REF")
endif()

target_link_libraries(${myTargetLib} PRIVATE shlwapi.lib libkram)
