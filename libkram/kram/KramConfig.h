// kram - Copyright 2020-2023 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#if defined(_WIN32)
#define KRAM_WIN 1
#elif __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#define KRAM_MAC 1
#else
#define KRAM_IOS 1
#endif
#elif __unix__
#define KRAM_LINUX 1
#endif

// define 0 if undfefined above
#ifndef KRAM_MAC
#define KRAM_MAC 0
#endif
#ifndef KRAM_WIN
#define KRAM_WIN 0
#endif
#ifndef KRAM_LINUX
#define KRAM_LINUX 0
#endif
#ifndef KRAM_IOS
#define KRAM_IOS 0
#endif

// TODO: add Profile build (rename RelWithDbgInfo)

#ifdef NDEBUG
#define KRAM_RELEASE 1
#define KRAM_DEBUG 0
#else
#define KRAM_RELEASE 0
#define KRAM_DEBUG 1
#endif

//------------------------

#if KRAM_WIN

// Disable warnings
// can set disable, once, or error

#pragma warning(disable : 4530 4305 4267 4996 4244 4305)

/* Couldn't seem to place these inside the open parens
    4530 // C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
    4305 // 'initializing': truncation from 'double'
    4267 // 'initializing': conversion from 'size_t' to 'uint32_t', possible loss of data
    4996 // 'fileno': The POSIX name for this item is deprecated. Instead, use the ISO C and C++ conformant name: _fileno. See online help for details.
    4244 // 'initializing': conversion from 'int32_t' to 'float', possible loss of data
    4305 // '*=': truncation from 'double' to 'float'
*/

// avoid conflicts with min/max macros, use std instead
#define NOMINMAX

#include <SDKDDKVer.h>
//#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS 1
#include <tchar.h>

#endif

//------------------------

// SIMD_WORKSPACE is set

// can't have ATE defined to 1 on other platforms
#if !(KRAM_MAC || KRAM_IOS)
#undef COMPILE_ATE
#endif

#ifndef COMPILE_ATE
#define COMPILE_ATE 0
#endif
#ifndef COMPILE_SQUISH
#define COMPILE_SQUISH 0
#endif
#ifndef COMPILE_ETCENC
#define COMPILE_ETCENC 0
#endif
#ifndef COMPILE_BCENC
#define COMPILE_BCENC 0
#endif
#ifndef COMPILE_ASTCENC
#define COMPILE_ASTCENC 0
#endif

#ifndef COMPILE_EASTL
#define COMPILE_EASTL 0
#endif

// basis transcoder only (read not writes)
#ifndef COMPILE_BASIS
#define COMPILE_BASIS 0
#endif

// rgb8/16f/32f formats only supported for import, Metal doesn't expose these formats
#ifndef SUPPORT_RGB
#define SUPPORT_RGB 1
#endif

// This needs debug support that native stl already has.
// EASTL only seems to define that for Visual Studio natvis, and not lldb
#define USE_EASTL COMPILE_EASTL

#if USE_EASTL

#define STL_NAMESPACE eastl

// this probably breaks all STL debugging
#include <EASTL/algorithm.h>  // for max
//#include "EASTL/atomic.h"
#include <EASTL/functional.h>

#include <EASTL/deque.h>
#include <EASTL/iterator.h>    // for copy_if on Win
#include <EASTL/sort.h>
#include <EASTL/basic_string.h>

#include <EASTL/array.h>
#include <EASTL/map.h>
#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>

#include <EASTL/shared_ptr.h>  // includes thread/mutex
#include <EASTL/unique_ptr.h>
#include <EASTL/initializer_list.h>

// std - simpler than using eastl version
#include <atomic>

#else


// in Xcode 14, C++20 Modules have "partial" support... whatever that means.
// These imports are taken from MSVC which has a full implementation.
//import std.memory;
//import std.threading;
//import std.core;
//import std.filesystem;
//import std.regex;

#define STL_NAMESPACE std

// all std
#include <algorithm>  // for max
#include <functional>

#include <deque>
#include <iterator>  // for copy_if on Win
#include <string>

#include <atomic>
#include <memory>    // for shared_ptr
#include <initializer_list>

#include <array>
#include <map>
#include <unordered_map>
#include <vector>

#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <random>

#include <cstdlib>
//#include <exception>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <filesystem>
#include <variant>

#endif

// Get this out of config, it pulls in random and other std::fields
//#if COMPILE_BASIS
//#include "basisu_transcoder.h"
//#endif

// includes that are usable across all files
#include "KramLog.h"

//-------------------------
// simd

#if KRAM_MAC || KRAM_IOS

#define USE_SIMDLIB 0

// This is Apple simd (it's huuuggge!)
// Also can't use the half4 type until iOS18 + macOS15 minspec, so need fallback.
#include <simd/simd.h>

// this is glue code for now
#include "float4a.h"


#else

// this means use vectormath
#define USE_SIMDLIB 1

// Test out on Win build first.
#include "vectormath++.h"

#endif
 
//---------------------------------------

// this just strips args
#define macroUnusedArg(x)

// this just strips args
#define macroUnusedVar(x) (void)x

//---------------------------------------

namespace kram {
using namespace STL_NAMESPACE;

// Use this on vectors
template <typename T>
inline size_t vsizeof(const vector<T>& v)
{
    return sizeof(T) * v.size();
}
}  // namespace kram
