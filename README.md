# kram
Encode/decode/info to and from PNG/KTX files with LDR/HDR and BC/ASTC/ETC2.

This includes the following encoders/decoders:
| Encoder  | Author           | License     | Encodes                    | Decodes | 
-----------|------------------|-------------|----------------------------|---------|
| BCEnc    | Rich Geldrich    | MIT         | BC1,3,4,5,7                | same    |
| Squish   | Simon Brown      | MIT         | BC1,3,4,5                  | same    |
| ATE      | Apple            | no sources  | BC1,4,5,7 ASTC4x4,8x8 LDR  | all LDR |
| Astcenc  | ARM              | Apache 2.0  | ASTC all LDR/HDR           | same    |
| Etc2comp | Google           | MIT         | ETC2r11,rg11,rgb,rgba      | same    |

Additional open-source:
| Library  | Author           | License |    
|----------|------------------|---------|
| lodepng  | Lode Vandevenne  | MIT     |
| SSE2Neon | John W. Ratcliff | MIT     |

There are several commands support by kram
* encode
* decode
* info
* script

Kram uses CMake to setup the projects and build:
```
mkdir build
cmake .. -G Xcode
cmake --build . --config Release
```
If you want to debug from Xcode, then the project is generated:
```
open kram.xcodeproj
```

