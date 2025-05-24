vectormath
==========

A small vector math library for float and double vectors, matrices, and quaternions.  There are also types for int/long used for the float/double comparisons.  Each type can be disabled (SIMD_FLOAT, SIMD_DOUBLE) to limit usage.  This should be built as an optimized library to keep debug builds running fast.  Most of the calls are inlined, so selectively optimizing the included code in a debug build will also help.

Small implementation kernel (just using float2/4 and double2/4 simd ops).  I would not recommend using with C.  All types have base C types for ObjC and these provide default math and comparison ops.

You can also bury the impls with a little work, and avoid the simd headers getting pulled into code, but the whole point is to inline the calls for speed and stay in register.  So can drop to SSE4.2, but give up F16C.  And AVX2 provides fma to line up with arm64.  So going between arm64 and AVX2 seems like a good parallel if your systems support it.

Based around the gcc/clang vector extensions.  These provide a lot of opimtized base ops.  The vecs extend to 2, 4, 8, 16, 32 operands.   On larger types, these use multiple 4 operand instructions to do so.   I've limited vector counts to 32B for AVX2 for now.   These are c types for the vectors, so no ctors or member functions.  You can further wrap these under your own vector math code, but you then have a lot of forwarding and conversion.  I recommend using the make ctors for the vectors.   But the curly brace init is misuse for what it does.

```
float4 v = {1.0f};        v = 1,xxx
float4 v = float4m(1.0f); v = 1,1,1,1
float4 v = 1.0f.          v = 1,1,1,1
```

Matrices are 2x2, 3x3, 3x4, and 4x4 column only.  Matrices have a C++ type with operators and calls.  Chop out with defines float, double, half, but keep int for the conditional tests.   Easy to add more types with the macros - u/char, u/long, u/short. 

I gutted the armv7 stuff from sse2neon.h so that's readable.  But this is only needed for an _mm_shuffle_ps.  Updated sse_mathfun for the cos/sin/log ops, but it's currently only reference fp32 SSE.  I added the fp16 <-> fp32 calls, since that's all Android has.  

Apple Accelerate has similar calls and structs.  The lib holds the optimized calls for sin, cos, log, inverse, but you only get them if you're on a new enough iOS/macOS.   And that api is so much code, that for some things it's not using the best methods.  

---

* Simd: arm64 Neon, x64 AVX2/AVX/SSE4.1
* Compiler: Clang mainly 
* Language: C types/ops, C++ matrix type and ops, can pass to ObjC
* Features: Clang/GCC vector extensions (no MSVC)
* C++ usage: C++11 but compiled as C++20
* Platforms: macOS/iOS, Win, Linux, others

Small vector simd kernel based around 2 and 4 element int, float, double ops.
  
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

* all types come in three flavors
* float4a - aligned type
* float4p - packed type
* float   - c++ type omits the "a"
*
* int2/3/4/8
* long2/3/4
*
* half2/3/4/8/16
* float2/3/4/8
* float2x2/3x3/3x4/4x4
* double2/3/4
* double2x2/3x3/3x4/4x4
*
* - u/char2...32
* - u/short2...16
* - ulong2...16

---

max vec size per register
* 16B      32B
* char16   char32
* short8   short16
* uint4    uint8
* float4   float8
* double2  double4

---

* DONE: Add double2 ops and double2x2...
* DONE: Add quatf, and conversions
* DONE: Add affine inverses
* DONE: Split file into float and double sub-files
* DONE: Add float2/double2 Neon vec ops.
* DONE: Add double2 SSE ops
* DONE: Add double4 AVX2 ops
* DONE: Formatting in print support
* DONE: Move to release Xcode library project and CMakeLists.txt file. 

* TODO: Tests of the calls.
* TODO: Add row vector support (vs. columns)
* TODO: Consider adding ISPC optimized calls for log, exp, sin, cos, etc
* DONE: Add debugger natvis and lldb formatters
* SOME: Disassembly of the calls (MSVC?)

---


