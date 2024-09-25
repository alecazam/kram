vectormath
==========

 
* Simd: 
*   arm64 Neon
*   x64 AVX2, AVX, SSE4.2
* Compiler: Clang mainly 
* Language: C types/ops, C++ matrix type and ops, can pass to ObjC
* Features: Clang/GCC vector extensions (no MSVC)
* C++ usage: C++11 but compiled as C++20
* Platforms: macOS/iOS, Win, Linux, others

Small vector simd kernel based around 4 element int, float, double ops.
  Despite AVX2, it's only using 128-bit ops currently (SSE 4.2.
  
Half (fp16) conversions in case _Float16 not supported (f.e. Android)

Clang vector extensions provide:
* swizzles (f.e. .xzy)
* array ops (f.e. v[0], v[1])
* rgba and xyzw access
* built-in conversions ops
* .even/.odd are even/odd elemnts of vector (reduced)
* .hi/.lo vector chunks (f.e. float8 provides 2x float4 vecs)
* math ops that vectorize to simd ops (+=, *=, +, -, ...)
* comparison ops that generate an int2/3/4/8 op
* can only use extension on C typedef
* C++ vector is typedef to C typedef, no member functions
* Can cast to system simd libs, no conversions needed
* Converts to smaller vectors via swizzles. v.xyzw -> v.x, v.xy, v.xyz
* Splats constants to stay in register v.x -> v.xxxx
* Auto converts to _m128 and float32x4_t
* Neon has to emulated 32B with 2 registers (f.e. double4)


---

Types

* half2/3/4/8/16
* half2x2/3x3/3x4/4x4

* float2/3/4/8/16
* float2x2/3x3/3x4/4x4

* int2/3/4/8/16
* int2x2, int3x3, int3x4, int4x4

* double2/3/4/8/16
* doublet2x2/3x3/3x4/4x4

* u/char2...16
* u/short2...16
* u/long2...8

---

max vec size per register
* 16B      32B
* char16   char32
* short8   short16
* uint4    uint8
* float4   float8
* double2  double4

---

TODO: 
* Finish double2 ops and double2x2...
* Add quatf/d, and conversions
* Add affine inverses
* Add row vector support (vs. columns)
* Split file into float, double, half sub-headers
* Add 2-element Neon vec ops.
* Tests of the calls.
* Disassembly of the calls (MSVC?)
* Formatting in print support
* Move to release Xcode library project and CMakeLists.txt file. 
* Look into how to individually optimize .cpp files that include it
* Rename to simdk.h

---

Small implementation kernel (just using the float4 simd ops), so is easy to add double versions with _pd or whatever Neon has.  I would not recommend using with C, but there are C types for ObjC storage and default math and comparison ops.

You can also bury the impls with a little work, and avoid the simd headers getting pulled into code, but the whole point is to inline the calls for speed and stay in register.  So can drop to SSE4.2, but give up F16C.  And AVX2 provides fma to line up with arm64.  So going between arm64 and AVX2 seems like a good parallel if your systems support it.

Written so many of these libs over the years, but this one is based around the gcc/clang vector extensions.   The vecs extend from 2, 4, 8, 16, 32.   They all use more 4 ops to do so.   I'm tempted to limit counts to 32B for AVX2.   So no ctors or member functions on the vectors (see float4m, half4m - make ops), and some derived structs on the matrices.  You can further wrap these under your own vector math code, but you then have a lot of forwarding and conversion.

I recommend using the make ctors.   The curly brace init is easy to mistake for what it does.

```
float4 v = {1.0f};    v = 1,xxx
float4 v = float4m(1.0f); v = 1,1,1,1
float4 v = 1.0f.          v = 1,1,1,1
```

Matrices are 2x2, 3x3, 3x4, and 4x4 column only.  Matrices have a C++ type with operators and calls.  Chop out with defines float, double, half, but keep int for the conditional tests.   Easy to add more types with the macros - u/char, u/long, u/short.  Had a pretty sucky day, so positive feedback or any changes to optimize this further are welcome.   And this had numerous git crlf failures today trying to fix it for Win.

I gutted the arrmv7 stuff from sse2Neon.h, so that's readable, and updated sse_mathfun for the cos/sin/log ops.  I had the fp16 <-> fp32 calls, since that's all Android has.  Apple has similar calls and structs, but the Accelerate lib holds many of the optimized calls for sin, cos, log, inverse.  And you only get them if you're on a new enough iOS/macOS.   And that api is so much code, that for some things it's not using the best methods.  Mine probably isn't either.  A lot of this was cobbled together out of an old vec math lib for my personal apps.  And there's still more I can salvage.

Followed Fabian's suggestions.   So div and sqrt instead of recip and rsqrt approximations that take many ops.  This is meant to follow naming to match HLSL/MSL/GLSL where possible.

I have a lib Xcode project not yet checked in, so then could optimize whatever calls are buried, but most calls are light and force inline, so need to be in a header, or move your ops into an optimized or -Og call.   That may reduce call overhead.   I haven't looked at the disassembly this generates yet.  But VS is good at dumping that.  Xcode less so. 


