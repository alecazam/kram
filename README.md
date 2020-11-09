# kram
Encode/decode/info to and from PNG/KTX files with LDR/HDR and BC/ASTC/ETC2.

kram is a wrapper to several popular encoders.  Most encoders have sources, and have been optimized to use very little memory and generate high quality encodings at all settings.  All kram encoders are currently cpu-based.  Some of these encoders use SSE but not Neon, and I'd like to fix that.  kram was built to be small and used as a library or app.  It's also designed for mobile and desktop use.  The final size with all encoders is under 1MB, and disabling each encoder chops off around 200KB down to a final 200KB app size via dead-code stripping.  The code should compile with C++11 or higher.

kram focuses on sending data efficiently and precisely to the encoders.  kram handles srgb and premul at key points in mip generation.  Source files use mmap to reduce memory, but fallback to file ops if that fails.  Temp files are generated for output, and then renamed in case the app fails or is terminated.  Mips are done in-place, and mip data is written out to a file to reduce memory usage. kram leaves out BC2 and etcrgb8a1 and PVRTC.  Also BC6 still needs an encoder, and ASTC HDR encoding needs a bit more work to pull from the float4 pixels.  

Many of the encoder sources can multithread a single image, but that is unused.  kram is designed to batch process one texture per thread via a python script or a C++11 task system inside kram.  These currently both take the same amount of cpu time, but the latter is best if kram ever adds gpu accelerated encoding.

kram encourages the use of lossless and hdr source data.  There are not many choices for lossless data - PNG, EXR, and Tiff to name a few.  Instead, kram can pull in KTX files for 8u/16f/32f data, or 8u from PNG.  Let kram convert source pixels to premultiplied alpha and from srgb to linear, since ordering matters here and kram does this using float4.  LDR and HDR data can come in as horizontal or vertical strips, and these strips can then have mips generated for them.  So cube maps, cube map arrays, 2D arrays, and 1d arrays are all handled the same.

Similar to a makefile system, the script sample kramtexture.py uses modstamps to skip textures that have already been processed.  If the source png is older than the ktx output, then the file is skipped.  Command line options are not yet compared, so if those change then use --force on the python script to rebuild all textures.  Also a crc/hash could be used instead when modstamp isn't sufficient or the same data could come from different folders.

Kram adds props to the KTX file to store data.  Currently props store Metal and Vulkan formats.  Also props are saved for channel content and post-swizzle.  Loaders, viewers, and shaders can utilize this metadata.

KTX can be converted to KTX2 and each mip supercompressed via ktx2ktx2 and ktxsc.  But there are no viewers for that format.

There are several commands supported by kram.
* encode - encode/decode block formats, mipmaps, fast sdf, premul, srgb, swizzles, LDR and HDR support, 16f/32f
* decode - can convert any of the encode formats to s/rgba8 ktx files for display 
* info   - dump dimensions and formats and metadata/props from png and ktx files
* script - send a series of kram commands that are processed in a task system.  Ammenable to gpu acceleration.

There are sample scripts.
* kramTextures.py  - python3 example that recursively walks directories and calls kram, or accumulates command and runs as a script
* formatSources.sh - zsh script to run clang_format on the kram source directory (excludes open source)

Kram uses CMake to setup the projects and build.  An executable kram and libkram are generated, but only kram is needed to run.  The library can be useful in apps that want to include the decoder, or runtime compression of gpu-generated data.

```
mkdir build
cmake .. -G Xcode
cmake --build . --config Release
```
If you want to debug from Xcode, then the project is generated from CMake.
```
open kram.xcodeproj
```
To see how kram works, there is a sample script that applies platform-specific presets based on source filenames.
```
cd build
../scripts/kramTextures.py -p android
../scripts/kramTextures.py -p ios --script
../scripts/kramTextures.py -p mac --force --script
```

To test individual encoders.
```
cd build
./Release/kram -testall
./Release/kram -test 1002
```

This app would not be possible without the open-source contributions from the following people and organizations.  These people also inspired me to make this app open-source, and maybe this will encourage more great tools or game tech.

Kram includes the following encoders/decoders:

| Encoder  | Author           | License     | Encodes                     | Decodes | 
|----------|------------------|-------------|-----------------------------|---------|
| BCEnc    | Rich Geldrich    | MIT         | BC1,3,4,5,7                 | same    |
| Squish   | Simon Brown      | MIT         | BC1,3,4,5                   | same    |
| ATE      | Apple            | no sources  | BC1,4,5,7 ASTC4x4,8x8 LDR   | all LDR |
| Astcenc  | ARM              | Apache 2.0  | ASTC4x4,5x5,6x6,8x8 LDR/HDR | same    |
| Etc2comp | Google           | MIT         | ETC2r11,rg11,rgb,rgba       | same    |
| Explicit | Me               | MIT         | r/rg/rgba 8u/16f/32f        | none    |


```
ATE
Simple wrapper for encode/decode around this.

BCEnc
Commented out some unused code to suppress warnings

Squish
Simplified to single folder.
Replaced sse vector with float4/a.

Astcenc v2.0
Provide F32 source pixels.
Improved 1 and 2 channel format encoding.
Avoid reading off end of arrays.
Fix various decode bugs.

Etc2comp 
Simplified to single folder.
Keep r11 and rg11 in integer space. 6x faster.
Memory reduced signficantly.  One block allocated.
Single pass encoder works on one block at a time, and does not skip blocks.
Multipass and multithread algorithm sorts vector, and split out blockPercentage from iteration count.
RGB8A1 is broken.
Optimized encodes by inlining CalcPixelError. 2x faster.
```

Kram includes additional open-source:

| Library    | Author           | License | Purpose                   |
|------------|------------------|---------|---------------------------|
| lodepng    | Lode Vandevenne  | MIT     | png encode/decode         |
| SSE2Neon   | John W. Ratcliff | MIT     | sse to neon               |
| heman      | Philip Rideout   | MIT     | parabola EDT for SDF      |
| TaskSystem | Sean Parent      | MIT     | C++11 work queue          |

```
lodepng
Altered header paths.

SSE2Neon
Needs updated to new arm64 permute instructions.

heman 
SDF altered to support mip generation from bigger distance fields
   This requires srcWidth x dstHeight floats.
  
```
Features to complete:
Tile command for SVT tiling
Merge command to combine images (similar to ImageMagick)
Atlas command to atlas to 2D and 2D array textures
Add GPU encoder (use compute in Metal/Vulkan)
Add BC6H encoder
Improve permute in SSE2Neon
Save prop with args and compare args and modstamp before rebuilding to avoid --force
Multichannel SDF
Plumb float4 through to ASTC HDR encoding

Test Images
Samples from test files used for the various encoders.
SDF image taken from EDT that inspired heman.
collectorbarrel-n/a from Id's old GPU BC1/3 compression article.

```

SYNTAX
kram[encode | decode | info | script | ...]
Usage: kram encode
	 -f/ormat (bc1 | astc4x4 | etc2rgba | rgba16f)
	 [-srgb] [-signed] [-normal]
	 -i/nput <source.png | .ktx>
	 -o/utput <target.ktx | .ktxa>

	 [-type 2d|3d|..]
	 [-e/ncoder (squish | ate | etcenc | bcenc | astcenc | explicit | ..)]
	 [-resize (16x32 | pow2)]

	 [-mipalign] [-mipnone]
	 [-mipmin size] [-mipmax size]

	 [-swizzle rg01]
	 [-avg rxbx]
	 [-sdf]
	 [-premul]
	 [-quality 0-100]
	 [-optopaque]
	 [-v]
   
         [-test 1002]
         [-testall]

OPTIONS
	-type 2d|3d|cube|1darray|2darray|cubearray

	-format [r|rg|rgba| 8|16f|32f]	Explicit format to build mips and for hdr.
	-format bc[1,3,4,5,7]	BC compression
	-format etc2[r|rg|rgb|rgba]	ETC2 compression - r11sn, rg11sn, rgba, rgba
	-format astc[4x4|5x5|6x6|8x8]	ASTC and block size. ETC/BC are 4x4 only.

	-encoder squish	bc[1,3,4,5]
	-encoder bcenc	bc[1,3,4,5,7]
	-encoder ate	bc[1,4,5,7]
	-encoder ate	astc[4x4,8x8]
	-encoder astcenc	astc[4x4,5x5,6x6,8x8] ldr/hdr support
	-encoder etcenc	etc2[r,rg,rgb,rgba]
	-encoder explicit	r|rg|rgba[8|16f|32f]

	-mipalign	Align mip levels with .ktxa output 
	-mipnone	Don't build mips even if pow2 dimensions
	-mipmin size	Only output mips >= size px
	-mipmax size	Only output mips <= size px

	-srgb	sRGB for rgb/rgba formats
	-signed	Signed r or rg for etc/bc formats, astc doesn't have signed format.
	-normal	Normal map rg storage signed for etc/bc (rg01), only unsigned astc L+A (gggr).
	-sdf	Generate single-channel SDF from a bitmap, can mip and drop large mips. Encode to r8, bc4, etc2r, astc4x4 (Unorm LLL1) to encode
	-premul	Premultiplied alpha to src pixels before output

	-optopaque	Change format from bc7/3 to bc1, or etc2rgba to rgba if opaque

	-swizzle [rgba01 x4]	Specifies pre-encode swizzle pattern
	-avg [rgba]	Post-swizzle, average channels per block (f.e. normals) lrgb astc/bc3/etc2rgba
	-v	Verbose encoding output

Usage: kram info
	 -i/nput <.png | .ktx> [-o/utput info.txt] [-v]

Usage: kram decode
	 -i/nput .ktx -o/utput .ktx [-swizzle rgba01] [-v]

Usage: kram script
	 -i/nput kramscript.txt [-v] [-j/obs numJobs]

```

These encoders have their own wrappers with different functionality.
* Astcenc (astcenc) WML, ASTC
* ETC2comp (etctool) WML, ETC2
* Squish (squishpng) WML, BC
* BCEnc - WML, BC

Other great encoder wrappers to try.  Many of these require building the app from CMake, and many only supply Windows executables. 
* Cuttlefish (cuttlefish) - WML, ASTC/BC/ETC/PVRTC
* PVRTexTool (PVRTexToolCLI) - WML, ASTC/BC/ETC/PVRTC, no BC on ML
* Nvidia Texture Tools (nvtt) - WML, ASTC/BC/ETC/PVRTC
* Basis Universal (basisu) - WML, ASTC/ETC1, transodes to 4x4 formats
* KTX Software (toktx, ktx2ktx2, ktxsc) - basis as encode
* Intel ISPC - WML, BC/ASTC
* Ignacio Costano's BC encoder ICBC - WML, BC


