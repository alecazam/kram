// kram - Copyright 2020 by Alec Miller. - MIT License
// The license and copyright notice shall be included
// in all copies or substantial portions of the Software.

#pragma once

#ifdef _WIN32
#define KRAM_WIN 1
#else
#define KRAM_WIN 0
#endif

// this isn't enough for iOS
#if __APPLE__
#define KRAM_MAC 1
#else
#define KRAM_MAC 0
#endif

#if __unix__
#define KRAM_LINUX 1
#else
#define KRAM_LINUX 0
#endif

//------------------------
#if KRAM_MAC
#include <TargetConditionals.h>

#if TARGET_CPU_X86_64
#define USE_SSE 1
#elif TARGET_CPU_ARM64
#define USE_NEON 1
#endif

#endif

//------------------------
#if KRAM_WIN

// avoid conflicts with min/max macros, use std instead
#define NOMINMAX

#include <SDKDDKVer.h>
#define _CRT_SECURE_NO_WARNINGS 1
#include <tchar.h>

// For now assume Intel on Win32
#define USE_SSE 1

#endif

//------------------------
// TODO: unix

//------------------------

// one of these must be set
#ifndef USE_SSE
#define USE_SSE 0
#endif
#ifndef USE_NEON
#define USE_NEON 0
#endif

#ifndef USE_SIMDLIB
#if defined(__APPLE__)
#define USE_SIMDLIB 1
#else
#define USE_SIMDLIB 0
#endif
#endif

#if USE_SIMDLIB
#define USING_SIMD using namespace simd
#else
#define USING_SIMD using namespace simd
#define simd_make_float4(x, y, z, w) float4(x, y, z, w)
#endif

// can override from build system
#if !defined(COMPILE_ATE) && KRAM_MAC
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

// includes that are usable across all files
#include "KramLog.h"

#if USE_SIMDLIB
#include "simd/simd.h"
#else
// emulate float4
#include "float4a.h"
#endif
